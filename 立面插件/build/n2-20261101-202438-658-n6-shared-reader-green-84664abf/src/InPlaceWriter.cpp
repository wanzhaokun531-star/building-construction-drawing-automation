#include "InPlaceWriter.h"
#include <acdocman.h>
#include <actrans.h>
#include <aced.h>
#include <adscodes.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
namespace bce {
namespace {
template<class T> Result<T> fail(const char* code,const std::string& message) { return {std::nullopt,{{code,message,{},true}}}; }
void check(Acad::ErrorStatus status,const char* operation) {
    if(status!=Acad::eOk) throw std::runtime_error(std::string(operation)+": "+std::to_string(status));
}
template<class T> class Read final {
public:
    explicit Read(AcDbObjectId id) { check(acdbOpenObject(value_,id,AcDb::kForRead),"read native write prerequisite"); }
    ~Read() noexcept { if(value_) value_->close(); }
    Read(const Read&)=delete;
    Read& operator=(const Read&)=delete;
    T* operator->() const noexcept { return value_; }
    T* get() const noexcept { return value_; }
private:T* value_{};
};
class Transaction final {
public:
    explicit Transaction(AcDbTransactionManager& manager):manager_(manager), transaction_(manager.startTransaction()) {
        if(!transaction_) throw std::runtime_error("start transaction failed");
    }
    ~Transaction() noexcept { if(active_&&manager_.numActiveTransactions()==1&&manager_.topTransaction()==transaction_) manager_.abortTransaction(); }
    Transaction(const Transaction&)=delete;
    Transaction& operator=(const Transaction&)=delete;
    template<class T> T* open(AcDbObjectId id,AcDb::OpenMode mode) {
        AcDbObject* object{}; check(transaction_->getObject(object,id,mode),"transaction get object");
        auto* typed=T::cast(object); if(!typed) throw std::runtime_error("unexpected native object type"); return typed;
    }
    void adopt(AcDbObject* object) {
        const auto status=manager_.addNewlyCreatedDBRObject(object);
        if(status!=Acad::eOk) {
            // Already database-owned, but not transaction-owned: never delete. Close before abort.
            object->erase(); object->close(); check(status,"transaction adopt newly created object");
        }
    }
    void commit() { requireOwned(); check(manager_.endTransaction(),"commit transaction"); active_=false; }
    void rollback() { requireOwned(); check(manager_.abortTransaction(),"abort transaction"); active_=false; }
private:
    void requireOwned() const {
        if(!active_ || manager_.numActiveTransactions()!=1 || manager_.topTransaction()!=transaction_)
            throw std::runtime_error("transaction ownership changed; refusing to close caller transaction");
    }
    AcDbTransactionManager& manager_;
    AcTransaction* transaction_{};
    bool active_{true};
};
struct Cancelled final {};
void notify(CommitObserver* observer,CommitStage stage) { if(observer&&!observer->continueCommit(stage)) throw Cancelled{}; }
int undoControl() {
    resbuf value{};
    if(acedGetVar(L"UNDOCTL",&value)!=RTNORM || value.restype!=RTSHORT) throw std::runtime_error("UNDOCTL unreadable");
    return value.resval.rint;
}
bool finitePoint(const AcGePoint3d& point) noexcept {
    return std::isfinite(point.x)&&std::isfinite(point.y)&&std::isfinite(point.z);
}
bool preservedSegment(const AcGePoint3d& originalStart,const AcGePoint3d& originalEnd,
    const AcGePoint3d& movedStart,const AcGePoint3d& movedEnd) noexcept {
    if(!finitePoint(originalStart)||!finitePoint(originalEnd)||!finitePoint(movedStart)||!finitePoint(movedEnd)) return false;
    const auto before=originalEnd-originalStart,after=movedEnd-movedStart;
    if(!std::isfinite(before.x)||!std::isfinite(before.y)||!std::isfinite(before.z)||
        !std::isfinite(after.x)||!std::isfinite(after.y)||!std::isfinite(after.z)) return false;
    const double scale=(std::max)({1.0,std::abs(before.x),std::abs(before.y),std::abs(before.z)});
    const double tolerance=128.0*std::numeric_limits<double>::epsilon()*scale;
    return std::abs(before.x-after.x)<=tolerance&&std::abs(before.y-after.y)<=tolerance&&std::abs(before.z-after.z)<=tolerance;
}
bool translationPreservesLine(const AcDbLine& line,const Point2& displacement) {
    // Exercise the same SDK transform on an unowned ordinary Line before touching the drawing.
    AcDbLine prediction(line.startPoint(),line.endPoint());
    if(prediction.transformBy(AcGeMatrix3d::translation(AcGeVector3d(displacement.x,displacement.y,0.0)))!=Acad::eOk) return false;
    return preservedSegment(line.startPoint(),line.endPoint(),prediction.startPoint(),prediction.endPoint());
}
std::string layerReadback(const AcDbLayerTableRecord& layer) {
    const auto linetype=layer.linetypeObjectId();
    const auto boolean=[](bool value){return std::string(value?"true":"false");};
    AcString name; check(layer.getName(name),"diagnostic layer name");
    return "{name="+native_scene::utf8(name.kACharPtr())+",handle="+native_scene::handle(layer.objectId())+
        ",colorRaw="+std::to_string(layer.color().color())+",colorIndex="+std::to_string(layer.color().colorIndex())+
        ",colorMethod="+std::to_string(layer.color().colorMethod())+",linetypeHandle="+native_scene::handle(linetype)+
        ",linetypeName="+native_scene::symbolName(linetype)+",lineWeight="+std::to_string(layer.lineWeight())+
        ",off="+boolean(layer.isOff())+",frozen="+boolean(layer.isFrozen())+",locked="+boolean(layer.isLocked())+
        ",plottable="+boolean(layer.isPlottable())+",transparencyAlpha="+std::to_string(layer.transparency().alpha())+
        ",transparencyRaw="+std::to_string(layer.transparency().serializeOut())+
        ",isNewObject="+boolean(layer.isNewObject())+",isWriteEnabled="+boolean(layer.isWriteEnabled())+
        ",isTransactionResident="+boolean(layer.isTransactionResident())+"}";
}
}
Result<WriteScopePlan> InPlaceWriter::beginWriteScope(const LayoutPlan& plan, const SceneSnapshot& snapshot) {
    try {
        auto* database=context_.currentDatabase();
        if(!database) return fail<WriteScopePlan>(diagnostic::staleDocument,"Current document context is invalid");
        auto* document=acDocManager->curDocument();
        if(database->transactionManager()->numActiveTransactions()!=0 || document->transactionManager()->numActiveTransactions()!=0)
            return fail<WriteScopePlan>("OUTER_TRANSACTION_ACTIVE","Caller transaction must remain caller-owned; in-place service refuses nesting");
        if(snapshot.documentIdentity!=context_.documentIdentity() || snapshot.databaseIdentity!=context_.documentIdentity()+"/database" ||
            snapshot.revision!=context_.revision()) return fail<WriteScopePlan>(diagnostic::staleDocument,"Document context or revision changed");
        if(acDocManager->isApplicationContext() || document->myLockMode()==AcAp::kNone || document->myLockMode()==AcAp::kNotLocked ||
            document->myLockMode()==AcAp::kRead) return fail<WriteScopePlan>("COMMAND_CONTEXT_REQUIRED","A document write command is required");
        const int undo=undoControl();
        if(!(undo&1) || (undo&8)) return fail<WriteScopePlan>("UNDO_BOUNDARY_UNAVAILABLE","Undo must be enabled with no existing caller undo group");
        SceneRequest request{context_,{},snapshot.units.confirmedMmPerInputUnit};
        for(const auto& target:snapshot.targets) request.targetHandles.push_back(target.key.handle);
        const auto actual=readScene(request);
        if(!actual.canCommit()) return {std::nullopt,actual.issues};
        if(actual.value->revision!=snapshot.revision) return fail<WriteScopePlan>(diagnostic::staleDocument,"Scene changed during current-document revalidation");
        WriteRequest write{snapshot.documentIdentity,snapshot.revision,plan.viewIdentity,{},plan.requiredResources};
        std::set<std::string> seen;
        for(const auto& movement:plan.translations) {
            if(!std::isfinite(movement.displacement.x)||!std::isfinite(movement.displacement.y))
                return fail<WriteScopePlan>(diagnostic::outsideWriteScope,"Nonfinite translation rejected");
            write.modifications.push_back(movement.target);
        }
        auto scope=planWriteScope(*actual.value,write);
        if(!scope.canCommit()) return scope;
        for(const auto& movement:plan.translations) {
            const auto id=native_scene::objectId(*database,movement.target.handle);
            if(!seen.insert(native_scene::handle(id)).second) return fail<WriteScopePlan>(diagnostic::outsideWriteScope,"Duplicate target translations rejected");
            Read<AcDbEntity> entity(id);
            const auto* line=AcDbLine::cast(entity.get());
            if(!line) return fail<WriteScopePlan>(diagnostic::unsupportedNativeWrite,"Only ordinary Line translations are supported");
            if(!translationPreservesLine(*line,movement.displacement))
                return fail<WriteScopePlan>("INVALID_TRANSLATION_GEOMETRY","SDK translation would overflow or change the original Line segment vector");
            Read<AcDbLayerTableRecord> layer(entity->layerId());
            if(layer->isLocked()) return fail<WriteScopePlan>("LOCKED_TARGET_LAYER","Target layer is locked");
        }
        return scope;
    } catch(const std::exception& ex) { return fail<WriteScopePlan>(diagnostic::unsupportedNativeWrite,ex.what()); }
}
Result<CommitReceipt> InPlaceWriter::applyToCurrentDocument(PreparedBatch& batch, const LayoutPlan& plan, const SceneSnapshot& snapshot) {
    // Do not touch the prepared/current database before rejecting stale or caller transaction state.
    const auto initialScope=beginWriteScope(plan,snapshot);
    if(!initialScope.canCommit()) return {std::nullopt,initialScope.issues};
    if(!batch.validateOrdinaryLine()) return fail<CommitReceipt>(diagnostic::unsupportedNativeWrite,"Prepared database is not the unchanged supported single Line batch");
    const auto layerDefinition=batch.layerDefinition();
    if(!layerDefinition.canCommit()) return {std::nullopt,layerDefinition.issues};
    LayoutPlan actualPlan=plan; actualPlan.requiredResources.push_back(*layerDefinition.value);
    const auto scope=beginWriteScope(actualPlan,snapshot);
    if(!scope.canCommit()) return {std::nullopt,scope.issues};
    for(const auto& resource:scope.value->newResources) {
        if(resource.kind!="layer"||resource.name!=layerDefinition.value->name||resource.definitionSignature!=layerDefinition.value->definitionSignature)
            return fail<CommitReceipt>(diagnostic::unsupportedNativeWrite,"Creating this resource type is not supported by ordinary Line capability");
    }
    CommitReceipt receipt; receipt.documentIdentity=snapshot.documentIdentity;
    try {
        auto* database=context_.currentDatabase();
        if(!database) return fail<CommitReceipt>(diagnostic::staleDocument,"Document changed before write");
        Transaction transaction(*database->transactionManager());
        try {
            auto* layers=transaction.open<AcDbLayerTable>(database->layerTableId(),AcDb::kForWrite);
            const auto layerName=native_scene::utf16(batch.line().layerName); AcDbObjectId layerId;
            if(layers->has(layerName.c_str())) check(layers->getAt(layerName.c_str(),layerId),"reuse existing identical layer");
            else {
                auto layer=std::make_unique<AcDbLayerTableRecord>(); check(layer->setName(layerName.c_str()),"set requested layer name");
                AcCmColor color; color.setColorIndex(static_cast<Adesk::UInt16>(batch.line().layerColor)); layer->setColor(color);
                // A newly added transaction-resident layer has not received close-time defaults.
                // Resolve the prepared layer's symbolic linetype in this target database explicitly;
                // its prepared ObjectId must never be assigned across database boundaries.
                Read<AcDbLayerTable> sourceLayers(batch.database()->layerTableId()); AcDbObjectId sourceLayerId;
                check(sourceLayers->getAt(layerName.c_str(),sourceLayerId),"prepared layer dependency");
                Read<AcDbLayerTableRecord> sourceLayer(sourceLayerId);
                const auto sourceLinetypeName=native_scene::utf16(native_scene::symbolName(sourceLayer->linetypeObjectId()));
                if(sourceLinetypeName.empty()) throw std::runtime_error("prepared layer has no resolved linetype dependency");
                auto* targetLinetypes=transaction.open<AcDbLinetypeTable>(database->linetypeTableId(),AcDb::kForRead);
                AcDbObjectId targetLinetypeId; check(targetLinetypes->getAt(sourceLinetypeName.c_str(),targetLinetypeId),"resolve target linetype dependency");
                check(layer->setLinetypeObjectId(targetLinetypeId),"assign target linetype dependency");
                check(layers->add(layerId,layer.get()),"add requested layer"); auto* owned=layer.release();
                transaction.adopt(owned); receipt.createdResourceHandles.push_back(native_scene::handle(layerId));
                const auto definition=native_scene::resourceDefinition(*owned);
                if(definition.definitionSignature!=layerDefinition.value->definitionSignature) {
                    Read<AcDbLayerTable> preparedLayers(batch.database()->layerTableId()); AcDbObjectId preparedId;
                    check(preparedLayers->getAt(layerName.c_str(),preparedId),"diagnostic prepared layer id");
                    Read<AcDbLayerTableRecord> preparedLayer(preparedId);
                    throw std::runtime_error("created layer does not match prepared definition; expectedSignature="+
                        layerDefinition.value->definitionSignature+"; actualSignature="+definition.definitionSignature+
                        "; expectedFields="+layerReadback(*preparedLayer.get())+"; actualFields="+layerReadback(*owned));
                }
            }
            notify(observer_,CommitStage::resourcesCreated);
            auto* blocks=transaction.open<AcDbBlockTable>(database->blockTableId(),AcDb::kForRead); AcDbObjectId modelId;
            check(blocks->getAt(ACDB_MODEL_SPACE,modelId),"model space id");
            auto* model=transaction.open<AcDbBlockTableRecord>(modelId,AcDb::kForWrite);
            const auto& spec=batch.line(); auto entity=std::make_unique<AcDbLine>(AcGePoint3d(spec.start.x,spec.start.y,0.0),AcGePoint3d(spec.end.x,spec.end.y,0.0));
            entity->setDatabaseDefaults(database); check(entity->setLayer(layerId),"assign requested layer");
            check(entity->setColorIndex(256),"line bylayer color"); check(entity->setLinetype(L"ByLayer"),"line bylayer linetype");
            check(entity->setLinetypeScale(1.0),"line linetype scale"); check(entity->setLineWeight(AcDb::kLnWtByLayer),"line lineweight");
            AcDbObjectId entityId; check(model->appendAcDbEntity(entityId,entity.get()),"append prepared ordinary Line"); auto* owned=entity.release();
            transaction.adopt(owned); receipt.createdEntities.push_back({snapshot.databaseIdentity,native_scene::handle(entityId),snapshot.revision});
            notify(observer_,CommitStage::entitiesAppended);
            for(const auto& movement:plan.translations) {
                const auto id=native_scene::objectId(*database,movement.target.handle);
                auto* target=transaction.open<AcDbLine>(id,AcDb::kForWrite);
                const auto originalStart=target->startPoint(),originalEnd=target->endPoint();
                check(target->transformBy(AcGeMatrix3d::translation(AcGeVector3d(movement.displacement.x,movement.displacement.y,0.0))),"translate explicit target");
                if(!preservedSegment(originalStart,originalEnd,target->startPoint(),target->endPoint()))
                    throw std::runtime_error("actual Line translation lost finite segment geometry");
            }
            notify(observer_,CommitStage::targetsTranslated);
            transaction.commit(); receipt.committed=true;
            // One completed ACRX_CMD_MODAL caller forms the actual AutoCAD undo record.
            // No manual inverse, database reload, global setting change, or caller transaction closure.
            return {std::move(receipt),{}};
        } catch(const Cancelled&) {
            transaction.rollback(); return {std::move(receipt),{{"COMMIT_CANCELLED","Commit observer cancelled after mutation; own transaction aborted",{},true}}};
        } catch(const std::exception& ex) {
            transaction.rollback(); return {std::move(receipt),{{"INPLACE_COMMIT_FAILED",ex.what(),{},true}}};
        } catch(...) {
            transaction.rollback(); return {std::move(receipt),{{"INPLACE_COMMIT_FAILED","Non-standard observer exception; own transaction aborted",{},true}}};
        }
    } catch(const std::exception& ex) {
        return {std::move(receipt),{{"INPLACE_RECOVERY_ERROR",ex.what(),{},true}}};
    }
}
}
