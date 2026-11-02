#include "PreparedBatch.h"
#include "SceneReader.h"
#include <dbents.h>
#include <dbsymtb.h>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace bce {
namespace {
void check(Acad::ErrorStatus status,const char* operation) { if(status!=Acad::eOk) throw std::runtime_error(std::string(operation)+": "+std::to_string(status)); }
template<class T> class Opened final {
public:
    Opened(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead) { check(acdbOpenObject(value_,id,mode),"open prepared object"); }
    ~Opened() noexcept { if(value_) value_->close(); }
    Opened(const Opened&)=delete;
    Opened& operator=(const Opened&)=delete;
    T* operator->() const noexcept { return value_; }
    T* get() const noexcept { return value_; }
private:T* value_{};
};
std::string lineExpression(const AcDbLine& line) {
    // A mutable database() is an explicit native escape hatch, not authorization to silently
    // reinterpret edited prepared entities. Capture the supported Line's complete expression.
    AcDbObjectId plotStyle; const auto plotType=line.getPlotStyleNameId(plotStyle);
    const auto start=line.startPoint(),end=line.endPoint(); const auto normal=line.normal();
    std::ostringstream out; out<<std::setprecision(17)<<start.x<<','<<start.y<<','<<start.z<<';'<<end.x<<','<<end.y<<','<<end.z<<';'
        <<normal.x<<','<<normal.y<<','<<normal.z<<';'<<line.thickness()<<';'<<line.color().color()<<';'
        <<native_scene::handle(line.layerId())<<';'<<native_scene::handle(line.linetypeId())<<';'<<line.linetypeScale()<<';'
        <<line.lineWeight()<<';'<<line.visibility()<<';'<<line.transparency().serializeOut()<<';'
        <<native_scene::handle(line.materialId())<<';'<<plotType<<';'<<native_scene::handle(plotStyle)<<';'
        <<native_scene::handle(line.visualStyleId())<<';'<<line.castShadows()<<';'<<line.receiveShadows()<<';'
        <<native_scene::handle(line.extensionDictionary());
    return out.str();
}
}
struct PreparedBatch::Impl {
    PreparedLine line;
    std::unique_ptr<AcDbDatabase> database;
    AcDbObjectId lineId,layerId,modelId;
    ResourceDefinition originalLayer;
    std::string originalLineExpression;
};
PreparedBatch::PreparedBatch(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
PreparedBatch::~PreparedBatch() = default;
AcDbDatabase* PreparedBatch::database() noexcept { return impl_->database.get(); }
const PreparedLine& PreparedBatch::line() const noexcept { return impl_->line; }
Result<std::unique_ptr<PreparedBatch>> PreparedBatch::create(const PreparedLine& line) {
    try {
        if(!std::isfinite(line.start.x)||!std::isfinite(line.start.y)||!std::isfinite(line.end.x)||!std::isfinite(line.end.y)||
            line.layerName.empty()||line.layerColor<1||line.layerColor>255 || (line.start.x==line.end.x && line.start.y==line.end.y))
            throw std::runtime_error("invalid ordinary Line specification");
        auto impl=std::make_unique<Impl>(); impl->line=line; impl->database=std::make_unique<AcDbDatabase>(true,true);
        auto& db=*impl->database; const auto layerName=native_scene::utf16(line.layerName);
        {
            Opened<AcDbLayerTable> layers(db.layerTableId(),AcDb::kForWrite);
            if(layers->has(layerName.c_str())) throw std::runtime_error("prepared layer name conflicts with database default layer");
            auto layer=std::make_unique<AcDbLayerTableRecord>(); check(layer->setName(layerName.c_str()),"prepared layer name");
            AcCmColor color; color.setColorIndex(static_cast<Adesk::UInt16>(line.layerColor)); layer->setColor(color);
            check(layers->add(impl->layerId,layer.get()),"prepared layer add"); auto* owned=layer.release(); check(owned->close(),"prepared layer close");
        }
        { Opened<AcDbLayerTableRecord> layer(impl->layerId); impl->originalLayer=native_scene::resourceDefinition(*layer.get()); }
        { Opened<AcDbBlockTable> blocks(db.blockTableId()); check(blocks->getAt(ACDB_MODEL_SPACE,impl->modelId),"prepared model id"); }
        {
            Opened<AcDbBlockTableRecord> space(impl->modelId,AcDb::kForWrite);
            auto entity=std::make_unique<AcDbLine>(AcGePoint3d(line.start.x,line.start.y,0.0),AcGePoint3d(line.end.x,line.end.y,0.0));
            entity->setDatabaseDefaults(&db); check(entity->setLayer(impl->layerId),"prepared line layer");
            check(space->appendAcDbEntity(impl->lineId,entity.get()),"prepared line append"); auto* owned=entity.release(); check(owned->close(),"prepared line close");
        }
        { Opened<AcDbLine> lineEntity(impl->lineId); impl->originalLineExpression=lineExpression(*lineEntity.get()); }
        return {std::unique_ptr<PreparedBatch>(new PreparedBatch(std::move(impl))),{}};
    } catch(const std::exception& ex) { return {std::nullopt,{{diagnostic::unsupportedNativeWrite,ex.what(),{},true}}}; }
}
Result<ResourceDefinition> PreparedBatch::layerDefinition() const {
    try {
        Opened<AcDbLayerTableRecord> layer(impl_->layerId);
        const auto actual=native_scene::resourceDefinition(*layer.get());
        if(actual.name!=impl_->originalLayer.name || actual.definitionSignature!=impl_->originalLayer.definitionSignature)
            throw std::runtime_error("prepared resource changed after creation");
        return {actual,{}};
    } catch(const std::exception& ex) { return {std::nullopt,{{diagnostic::unsupportedNativeWrite,ex.what(),{},true}}}; }
}
bool PreparedBatch::validateOrdinaryLine() const {
    try {
        if(!layerDefinition().canCommit()) return false;
        Opened<AcDbBlockTableRecord> space(impl_->modelId); AcDbBlockTableRecordIterator* raw{};
        check(space->newIterator(raw),"prepared model iterator"); std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw);
        if(iterator->done()) return false;
        AcDbObjectId first; check(iterator->getEntityId(first),"prepared entity id"); iterator->step();
        if(first!=impl_->lineId || !iterator->done()) return false;
        Opened<AcDbLine> line(first);
        if(lineExpression(*line.get())!=impl_->originalLineExpression) return false;
        return line->startPoint()==AcGePoint3d(impl_->line.start.x,impl_->line.start.y,0.0) &&
            line->endPoint()==AcGePoint3d(impl_->line.end.x,impl_->line.end.y,0.0) && line->layerId()==impl_->layerId &&
            line->normal()==AcGeVector3d::kZAxis && line->thickness()==0.0 && line->colorIndex()==256 &&
            line->linetypeScale()==1.0 && native_scene::symbolName(line->linetypeId())=="ByLayer";
    } catch(...) { return false; }
}
}
