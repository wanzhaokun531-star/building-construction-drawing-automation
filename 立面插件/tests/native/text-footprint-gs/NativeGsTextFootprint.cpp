#include "NativeEvidence.h"
#include "TextOccupancy.h"
#include <aced.h>
#include <adscodes.h>
#include <accmd.h>
#include <acdocman.h>
#include <acutads.h>
#include <acutmem.h>
class CPoint;
#include <core_rxmfcapi.h>
#include <dbapserv.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <dbdict.h>
#include <dbvisualstyle.h>
#include <acgi.h>
#include <acgivisualstyle.h>
#include <drawable.h>
#include <rxregsvc.h>
class CView;
#include <acgs.h>
#include <AcGsManager.h>
#pragma warning(push)
#pragma warning(disable:5033) // ATIL 2020 half.h uses register; retain /W4 /WX for our code.
#include <Image.h>
#include <ImageContext.h>
#include <RgbModel.h>
#pragma warning(pop)
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace {
using namespace bce::native;
HMODULE moduleHandle{};
bool running{}, completed{};
std::string q(const char* p){return quote(std::string(p));}
std::string num(double d){require(std::isfinite(d),"nonfinite value");std::ostringstream o;o.imbue(std::locale::classic());o<<std::setprecision(17)<<d;return o.str();}
std::string address(const void* p){return std::to_string(reinterpret_cast<std::uintptr_t>(p));}
std::string matrix(const AcGeMatrix3d& m){std::vector<std::string> a;for(int r=0;r<4;++r)for(int c=0;c<4;++c)a.push_back(num(m(r,c)));return array(a);}
std::string point(bce::Point2 p){return array({num(p.x),num(p.y)});}
std::string bounds(const bce::Bounds2& b){return array({point(b.minimum),point(b.maximum)});}
std::string affine(const bce::TextAffine2& m){return array({num(m.a),num(m.b),num(m.tx),num(m.c),num(m.d),num(m.ty)});}
void ok(Acad::ErrorStatus s,const char* what){require(s==Acad::eOk,std::string(what)+" status="+std::to_string(s));}
void save(const std::wstring& path,const std::string& bytes){auto f=create_new(path);write_flushed(f.get(),bytes);}
struct Close{void operator()(AcDbObject* p)const noexcept{if(p)p->close();}};
template<class T>std::unique_ptr<T,Close> open(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead){T* p{};auto s=acdbOpenObject(p,id,mode);std::unique_ptr<T,Close> v(p);ok(s,"open owned object");require(p!=nullptr,"null object");return v;}
struct Control{const char* id;const wchar_t* text;AcGePoint3d position;double angle{},tracking{1},oblique{};bool backward{},nested{};};
const std::array<Control,5> controls{{
    {"digits",L"1234567890",{25,30,0}}, {"chinese",L"中文测试",{25,30,0}},
    {"rotated-backward",L"123ABC",{88,28,0},0.47,1,0,true},
    {"nested-nonuniform",L"1200中文",{4,6,0},0.18,1,0,false,true},
    {"tracking-oblique",L"12中文ABC",{25,30,0},0,1.6,0.25}
}};
// Independent nontext colored squares. The five positions are asymmetric and noncollinear.
const std::array<AcGePoint3d,5> calibration{{{10.17,11.31,0},{111.23,15.47,0},{14.39,81.19,0},{107.11,78.37,0},{67.43,43.29,0}}};
const std::array<std::array<unsigned char,3>,5> colors{{{{255,0,0}},{{0,255,0}},{{0,0,255}},{{255,0,255}},{{0,255,255}}}};
void drawCalibration(AcGiWorldDraw& wd){
    auto& g=wd.geometry();auto& traits=wd.subEntityTraits();traits.setLineWeight(AcDb::kLnWt000);traits.setFillType(kAcGiFillAlways);
    for(std::size_t i=0;i<calibration.size();++i){AcCmEntityColor color;color.setRGB(colors[i][0],colors[i][1],colors[i][2]);traits.setTrueColor(color);const auto p0=calibration[i];
        const AcGePoint3d square[]{p0+AcGeVector3d(-0.7,-0.7,0),p0+AcGeVector3d(0.7,-0.7,0),p0+AcGeVector3d(0.7,0.7,0),p0+AcGeVector3d(-0.7,0.7,0)};require(!g.polygon(4,square),"calibration polygon terminated");}
}
struct AmbientWatch final:AcDbDatabaseReactor{
    unsigned opened{},modified{},appended{},erased{},headerWrites{};
    void objectOpenedForModify(const AcDbDatabase*,const AcDbObject*)override{++opened;}
    void objectModified(const AcDbDatabase*,const AcDbObject*)override{++modified;}
    void objectAppended(const AcDbDatabase*,const AcDbObject*)override{++appended;}
    void objectErased(const AcDbDatabase*,const AcDbObject*,Adesk::Boolean)override{++erased;}
    void headerSysVarChanged(const AcDbDatabase*,const ACHAR*,Adesk::Boolean success)override{if(success)++headerWrites;}
    std::string json()const{return object({{"openedForModify",std::to_string(opened)},{"modified",std::to_string(modified)},{"appended",std::to_string(appended)},{"erased",std::to_string(erased)},{"headerWrites",std::to_string(headerWrites)}});}
    bool untouched()const{return opened==0&&modified==0&&appended==0&&erased==0&&headerWrites==0;}
};
struct Session;
// No userdata parameter exists in the 2020 callback ABI. One stable generic Session
// owns this provider from before createModel until deleteModel has returned.
std::mutex providerMutex;
Session* modelProvider{};
std::uint64_t providerOrphanCalls{};
std::atomic<bool> providerBoundaryFailure{false};
AcGiDrawable* diagnosticGetInterface(Adesk::IntDbId objectId,bool bNeedsValidation)noexcept;
void diagnosticReleaseInterface(AcGiDrawable* drawable)noexcept;
class RootObserver final:public AcGsModelReactor{
public:explicit RootObserver(Session& owner):s(owner){}
    bool onSceneGraphRootAdded(AcGsModel*,AcGiDrawable*)override;
    bool onSceneGraphRootErased(AcGsModel*,AcGiDrawable*)override;
private:
    void record(bool,AcGsModel*,AcGiDrawable*)noexcept;
    Session& s;
};
class PrimitiveRoot final:public AcGiDrawable{
public:explicit PrimitiveRoot(Session& owner):s(owner){}
    Adesk::Boolean isPersistent()const override{return Adesk::kFalse;}
    AcDbObjectId id()const override{return AcDbObjectId::kNull;}
protected:
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits*)override;
    Adesk::Boolean subWorldDraw(AcGiWorldDraw*)override;
    void subViewportDraw(AcGiViewportDraw*)override;
private:Session& s;
};
class Rule final:public AcGiDrawableOverrule{
public:explicit Rule(Session& v):s(v){}
    bool isApplicable(const AcRxObject*)const override;
    Adesk::UInt32 setAttributes(AcGiDrawable*,AcGiDrawableTraits*)override;
    Adesk::Boolean worldDraw(AcGiDrawable*,AcGiWorldDraw*)override;
    void viewportDraw(AcGiDrawable*,AcGiViewportDraw*)override;
private:Session& s;
};
struct OwnedResource{
    AcDbObjectId id;Adesk::IntDbId key{};std::vector<std::string> roles;std::string className,unavailable;
    AcDbObject* object{};AcGiDrawable* drawable{};std::uintptr_t objectAddress{},drawableAddress{},databaseAddress{};
    bool usable{},identityReadabilityVerified{},pinOwned{},closeAttempted{};int openStatus{-1},closeStatus{-1};std::uint64_t gets{},releases{},borrowed{};
};
struct Session{
    Config config;std::unique_ptr<Evidence> evidence;File trace;std::mutex traceMutex;
    AcApDocument* current{};AcApDocument* active{};AcDbDatabase* ambient{};AmbientWatch watch;bool watchAttached{},ambientInitialized{};int dbmodBefore{};
    std::unique_ptr<AcDbDatabase> db;AcDbObjectId markerId,styleId,rootId;std::unique_ptr<AcDbPoint,Close> marker;std::unique_ptr<AcDbBlockTableRecord,Close> sceneRoot;bool styleBound{},styleQualified{},diagnosticOnly{};
    AcGsManager* manager{};AcGsGraphicsKernel* kernel{};AcGsModel* model{};AcGsModel* priorModel{};AcGsDevice* device{};AcGsView* view{};bool associated{},viewAttached{};
    std::array<AcGsColor,256> controlPalette{};bool physicalPaletteControl{},logicalPaletteCallStarted{},logicalPaletteReturned{},physicalPaletteCallStarted{},physicalPaletteReturned{};
    RootObserver rootObserver{*this};AcGiDrawable* expectedRoot{};bool rootObserverAttached{},rootObservationFailed{};std::uint64_t rootsAdded{},rootsErased{},rootDetails{},rootLogFailures{};
    std::unique_ptr<Rule> rule;bool ruleAttached{},priorOverruling{};
    int sample{-1};unsigned callbacks{},frameCallbacks{},applicableCalls{},ownedApplicableCalls{},attributeCalls{},viewportCalls{};std::string callbackError,executionFailure;int faultPoint{};
    bool positiveControl{},valueStyleControl{},diagnosticExecutionStarted{},diagnosticIdentityVerified{},diagnosticIdentityFailed{},zeroWorldDrawObserved{},positiveMappingVerified{};std::unique_ptr<PrimitiveRoot> primitive;std::wstring positiveRawPath;std::vector<std::string> styleObservations,styleBindings;
    bool kernelModelControl{},providerRegistered{},providerEverInstalled{},persistentAdapterUnavailable{},providerObservationFailed{};Adesk::IntDbId modelCookie{};std::uint64_t providerGetCalls{},providerReleaseCalls{},providerLogFailures{};std::string modelDefaultStyle;std::vector<std::string> updateObservations;
    std::map<Adesk::IntDbId,OwnedResource> resources;std::vector<std::string> catalogErrors;bool catalogStarted{},catalogReady{},catalogEnumerationComplete{},catalogPinsClosed{},providerBorrowBalanceFailed{};std::uint64_t cacheOpened{},cacheClosed{},cacheCloseErrors{},providerMatchedGets{},providerMatchedReleases{},borrowedAtModelDeletion{};
    std::wstring font,bigfont;std::string fontSHA,bigfontSHA;std::vector<std::string> frames;
    void styleLog(Fields f){if(positiveControl)styleObservations.push_back(object(f));log("ownedViewVisualStyle",std::move(f));}
    void log(const char* event,Fields f={}){std::lock_guard<std::mutex> lock(traceMutex);if(!trace.valid())return;f.push_back({"event",q(event)});const auto record=object(f);write_flushed(trace.get(),record+"\n");if(positiveControl&&(std::string(event)=="deviceUpdate.before"||std::string(event)=="deviceUpdate.after"||std::string(event)=="viewUpdate.before"||std::string(event)=="viewUpdate.after"))updateObservations.push_back(record);}
    bool ambientSame()const{return current==acDocManager->curDocument()&&active==acDocManager->mdiActiveDocument()&&ambient==acdbHostApplicationServices()->workingDatabase();}
    bool resourcesReleased()const noexcept{return !providerRegistered&&(!catalogStarted||catalogPinsClosed)&&!db&&!marker&&!sceneRoot&&!primitive&&!kernel&&!model&&!device&&!view&&!associated&&!viewAttached&&!rootObserverAttached&&!rule&&!ruleAttached&&!watchAttached&&!styleBound&&styleId.isNull()&&rootId.isNull();}
    void cleanupLog(const char* event,Fields f={})noexcept{try{log(event,std::move(f));}catch(...){}}
    bool cleanup(){
        cleanupLog("cleanup.enter",{{"faultPoint",std::to_string(faultPoint)},{"view",address(view)},{"marker",address(marker.get())},{"model",address(model)},{"device",address(device)},{"kernel",address(kernel)},{"database",address(db.get())},{"ruleAttached",ruleAttached?"true":"false"}});
        if(view){cleanupLog("view.eraseAll.before");view->eraseAll();cleanupLog("view.eraseAll.after");}
        if(ruleAttached){cleanupLog("removeOverrule.before");const auto st=AcRxOverrule::removeOverrule(AcDbPoint::desc(),rule.get());cleanupLog("removeOverrule.after",{{"status",std::to_string(st)}});if(st!=Acad::eOk)return false;ruleAttached=false;cleanupLog("restoreOverruling.before");AcRxOverrule::setIsOverruling(priorOverruling);cleanupLog("restoreOverruling.after");}
        cleanupLog("rule.reset.before");rule.reset();cleanupLog("rule.reset.after");
        cleanupLog("sceneRoot.close.before");sceneRoot.reset();cleanupLog("sceneRoot.close.after");
        cleanupLog("marker.close.before");marker.reset();cleanupLog("marker.close.after");
        if(viewAttached){cleanupLog("device.eraseView.before");const bool erased=device->erase(view);cleanupLog("device.eraseView.after",{{"removed",erased?"true":"false"}});if(!erased)return false;viewAttached=false;}
        if(view){cleanupLog("kernel.deleteView.before");kernel->deleteView(view);view=nullptr;styleBound=false;cleanupLog("kernel.deleteView.after");}
        if(associated){cleanupLog("restoreOwnedDatabaseAssociation.before");kernel->associate(reinterpret_cast<Adesk::LongPtr>(db.get()),priorModel,AcGsModel::kMain);cleanupLog("restoreOwnedDatabaseAssociation.after");cleanupLog("restoredAssociation.find.before");const bool restored=kernel->find(reinterpret_cast<Adesk::LongPtr>(db.get()),AcGsModel::kMain)==priorModel;cleanupLog("restoredAssociation.find.after",{{"matches",restored?"true":"false"}});if(!restored)return false;associated=false;}
        if(rootObserverAttached){cleanupLog("removeModelReactor.before",{{"model",address(model)},{"observer",address(&rootObserver)}});kernel->removeModelReactor(model,&rootObserver);rootObserverAttached=false;try{log("removeModelReactor.after",{{"added",std::to_string(rootsAdded)},{"erased",std::to_string(rootsErased)},{"detailAttempts",std::to_string(rootDetails)},{"detailLimit","32"},{"logFailures",std::to_string(rootLogFailures)},{"observationFailed",rootObservationFailed?"true":"false"}});}catch(...){++rootLogFailures;rootObservationFailed=true;}}
        if(model){if(kernelModelControl){cleanupLog("kernel.deleteModel.before");kernel->deleteModel(model);model=nullptr;cleanupLog("kernel.deleteModel.after");}else{cleanupLog("destroyAutoCADModel.before");manager->destroyAutoCADModel(model);model=nullptr;cleanupLog("destroyAutoCADModel.after");}}
        if(providerRegistered){std::lock_guard<std::mutex> lock(providerMutex);if(modelProvider!=this)return false;modelProvider=nullptr;providerRegistered=false;cleanupLog("modelProvider.withdrawn",{{"modelNoLongerLive","true"},{"getCalls",std::to_string(providerGetCalls)},{"releaseCalls",std::to_string(providerReleaseCalls)}});}
        if(catalogStarted&&!catalogPinsClosed){
            require(!model&&!providerRegistered,"cannot close resource cache while model/provider remains live");
            borrowedAtModelDeletion=0;for(const auto& item:resources)borrowedAtModelDeletion+=item.second.borrowed;
            cleanupLog("ownedResourceCache.close.begin",{{"modelNoLongerLive","true"},{"providerWithdrawn",providerRegistered?"false":"true"},{"opened",std::to_string(cacheOpened)},{"borrowedAtModelDeletion",std::to_string(borrowedAtModelDeletion)},{"observedReturnIncomplete",borrowedAtModelDeletion?"true":"false"},{"allBorrowsReturned",borrowedAtModelDeletion==0&&!providerBorrowBalanceFailed?"true":"false"}});
            bool closed=true;for(auto& item:resources){auto& r=item.second;if(!r.pinOwned)continue;r.closeAttempted=true;
                try{r.closeStatus=r.object->close();}catch(...){r.closeStatus=-1;}
                if(r.closeStatus==Acad::eOk){r.pinOwned=false;r.usable=false;r.object=nullptr;r.drawable=nullptr;++cacheClosed;}else{++cacheCloseErrors;closed=false;}
                cleanupLog("ownedResourceCache.close.item",{{"realOldId",std::to_string(r.key)},{"roles",array(r.roles)},{"object",std::to_string(r.objectAddress)},{"status",std::to_string(r.closeStatus)}});
            }
            catalogPinsClosed=closed;catalogReady=false;
            cleanupLog("ownedResourceCache.close.end",{{"opened",std::to_string(cacheOpened)},{"closed",std::to_string(cacheClosed)},{"closeErrors",std::to_string(cacheCloseErrors)},{"allOwnReadOpenReferencesClosed",closed?"true":"false"}});
            if(!closed)return false;
        }
        if(primitive){cleanupLog("nonpersistentRoot.destroy.before");primitive.reset();cleanupLog("nonpersistentRoot.destroy.after");}
        if(device){cleanupLog("kernel.deleteDevice.before");kernel->deleteDevice(device);device=nullptr;cleanupLog("kernel.deleteDevice.after");}
        if(kernel){cleanupLog("releaseGraphicsKernel.before");AcGsManager::releaseGraphicsKernel(kernel);kernel=nullptr;cleanupLog("releaseGraphicsKernel.after");}
        if(db){cleanupLog("destroyOwnedDatabase.before");db.reset();styleId.setNull();rootId.setNull();cleanupLog("destroyOwnedDatabase.after");}
        if(watchAttached){cleanupLog("ambient.removeReactor.before");ambient->removeReactor(&watch);watchAttached=false;cleanupLog("ambient.removeReactor.after");}
        cleanupLog("cleanup.return",{{"resourcesReleased",resourcesReleased()?"true":"false"}});
        return true;
    }
};
std::string paletteObservation(const Session& s){return object({{"scope",q("this owned offscreen device")},{"physicalRequested",s.physicalPaletteControl?"true":"false"},{"logicalCallStarted",s.logicalPaletteCallStarted?"true":"false"},{"logicalReturned",s.logicalPaletteReturned?"true":"false"},{"physicalCallStarted",s.physicalPaletteCallStarted?"true":"false"},{"physicalReturned",s.physicalPaletteReturned?"true":"false"},{"count",std::to_string(s.controlPalette.size())},{"sessionMemberInput",address(s.controlPalette.data())},{"logicalInput",s.logicalPaletteCallStarted?address(s.controlPalette.data()):"null"},{"physicalInput",s.physicalPaletteCallStarted?address(s.controlPalette.data()):"null"},{"inputValueStrategy",q("same shared initialization: entry 0 red,green,blue,padding=255,255,255,0; entries 1..255=0,0,0,0; original control values, not general ACI")},{"inputReferenceStrategy",q("both setters receive this exact Session member array; stable through owned device deletion")},{"physicalReadbackAvailable","false"},{"physicalAdoptionVerified","false"}});}
std::string resourceRecord(const OwnedResource& r){return object({{"realOldId",std::to_string(r.key)},{"roles",array(r.roles)},{"publicClass",r.className.empty()?"null":quote(r.className)},{"database",std::to_string(r.databaseAddress)},{"object",std::to_string(r.objectAddress)},{"drawable",std::to_string(r.drawableAddress)},{"openStatus",std::to_string(r.openStatus)},{"ownershipIdReadabilityVerified",r.identityReadabilityVerified?"true":"false"},{"usable",r.usable?"true":"false"},{"unavailableReason",quote(r.unavailable)},{"ownReadOpenHeld",r.pinOwned?"true":"false"},{"closeAttempted",r.closeAttempted?"true":"false"},{"closeStatus",r.closeAttempted?std::to_string(r.closeStatus):"null"},{"getReturns",std::to_string(r.gets)},{"releaseMatches",std::to_string(r.releases)},{"outstandingBorrows",std::to_string(r.borrowed)}});}
std::string catalogObservation(const Session& s){std::vector<std::string> records;for(const auto& item:s.resources)records.push_back(resourceRecord(item.second));return object({{"started",s.catalogStarted?"true":"false"},{"readyForModel",s.catalogReady?"true":"false"},{"enumerationComplete",s.catalogEnumerationComplete?"true":"false"},{"scope",q("nine owned SymbolTables and records; recursive owned NOD dictionaries/values; own marker/modelspace aliases")},{"allGsResourcesCoveredClaimed","false"},{"uniqueIds",std::to_string(s.resources.size())},{"readOpenReferencesAcquired",std::to_string(s.cacheOpened)},{"readOpenReferencesClosed",std::to_string(s.cacheClosed)},{"closeErrors",std::to_string(s.cacheCloseErrors)},{"pinsClosed",s.catalogPinsClosed?"true":"false"},{"errors",array(s.catalogErrors)},{"entries",array(records)}});}
void catalogError(Session& s,const std::string& reason){s.catalogErrors.push_back(quote(reason));s.catalogEnumerationComplete=false;s.log("ownedResourceCatalog.unavailable",{{"reason",quote(reason)}});}
OwnedResource* pinOwnedResource(Session& s,AcDbObjectId id,const std::wstring& role){
    if(id.isNull()){catalogError(s,"null owned ID at "+utf8(role));return nullptr;}
    const auto key=id.asOldId();auto inserted=s.resources.try_emplace(key);auto& r=inserted.first->second;r.roles.push_back(quote(role));
    if(!inserted.second)return &r; // A repeated role never takes an additional open reference.
    r.id=id;r.key=key;r.databaseAddress=reinterpret_cast<std::uintptr_t>(id.database());
    if(id.database()!=s.db.get()){r.unavailable="public directory ID does not belong to owned database";catalogError(s,r.unavailable);return &r;}
    if(id.isErased()){r.unavailable="public directory ID is erased";catalogError(s,r.unavailable);return &r;}
    AcDbObject* p{};r.openStatus=acdbOpenObject(p,id,AcDb::kForRead,false);
    if(r.openStatus!=Acad::eOk||!p){r.unavailable="kForRead open failed or returned null";catalogError(s,r.unavailable+" at "+utf8(role)+" status="+std::to_string(r.openStatus));return &r;}
    // Record the acquired reference before any allocating observation can throw.
    r.object=p;r.pinOwned=true;++s.cacheOpened;r.objectAddress=reinterpret_cast<std::uintptr_t>(p);
    r.className=utf8(std::wstring(p->isA()->name()));
    r.usable=p->database()==s.db.get()&&p->objectId()==id&&p->objectId().asOldId()==key&&!p->isErased()&&p->isReadEnabled();
    r.identityReadabilityVerified=r.usable;
    if(r.usable){r.drawable=static_cast<AcGiDrawable*>(p);r.drawableAddress=reinterpret_cast<std::uintptr_t>(r.drawable);}
    if(!r.usable){r.unavailable="opened object ownership/ID/readability verification failed";catalogError(s,r.unavailable+" at "+utf8(role));}
    s.log("ownedResourceCache.pin",{{"entry",resourceRecord(r)},{"independentProviderReadOpen","true"}});return &r;
}
void catalogSymbolTable(Session& s,AcDbObjectId id,const wchar_t* name){
    const std::wstring path=std::wstring(L"SymbolTable/")+name;auto* entry=pinOwnedResource(s,id,path);if(!entry||!entry->usable)return;
    auto* table=AcDbSymbolTable::cast(entry->object);if(!table){catalogError(s,"public SymbolTable cast failed at "+utf8(path));return;}
    AcDbSymbolTableIterator* raw{};const auto status=table->newIterator(raw,true,false);std::unique_ptr<AcDbSymbolTableIterator> it(raw);
    if(status!=Acad::eOk||!it){catalogError(s,"SymbolTable iterator unavailable at "+utf8(path)+" status="+std::to_string(status));return;}
    for(;!it->done();it->step(true,false)){AcDbObjectId recordId;const auto result=it->getRecordId(recordId);if(result!=Acad::eOk){catalogError(s,"SymbolTable record ID unavailable at "+utf8(path)+" status="+std::to_string(result));continue;}
        auto* record=pinOwnedResource(s,recordId,path+L"/record");if(!record||!record->usable)continue;auto* symbol=AcDbSymbolTableRecord::cast(record->object);AcString symbolName;
        if(!symbol){catalogError(s,"public SymbolTableRecord cast failed at "+utf8(path));continue;}const auto named=symbol->getName(symbolName);
        if(named==Acad::eOk)record->roles.push_back(quote(path+L"/"+symbolName.kACharPtr()));else catalogError(s,"record name unavailable at "+utf8(path)+" status="+std::to_string(named));
    }
}
void catalogDictionary(Session& s,AcDbObjectId id,const std::wstring& path,std::map<Adesk::IntDbId,bool>& visited){
    auto* entry=pinOwnedResource(s,id,path);if(!entry||!entry->usable)return;auto* dictionary=AcDbDictionary::cast(entry->object);if(!dictionary)return;
    if(!visited.emplace(entry->key,true).second)return; // Preserve the alias above, but do not recurse through cycles.
    std::unique_ptr<AcDbDictionaryIterator> it(dictionary->newIterator());if(!it){catalogError(s,"dictionary iterator unavailable at "+utf8(path));return;}
    for(;!it->done();it->next()){AcString name;const bool named=it->name(name);const auto child=it->objectId();
        if(!named){catalogError(s,"dictionary entry name unavailable at "+utf8(path));catalogDictionary(s,child,path+L"/<name-unavailable>",visited);}
        else catalogDictionary(s,child,path+L"/"+name.kACharPtr(),visited);
    }
}
void prepareOwnedResourceCache(Session& s){
    require(s.positiveControl&&s.valueStyleControl&&s.diagnosticOnly&&!s.model&&!s.view,"resource preparation restricted to pre-model owned diagnostic");
    s.catalogStarted=true;s.catalogEnumerationComplete=true;
    catalogSymbolTable(s,s.db->getSymbolTableId<AcDbBlockTable>(),L"Block");catalogSymbolTable(s,s.db->getSymbolTableId<AcDbLayerTable>(),L"Layer");
    catalogSymbolTable(s,s.db->getSymbolTableId<AcDbTextStyleTable>(),L"TextStyle");catalogSymbolTable(s,s.db->getSymbolTableId<AcDbLinetypeTable>(),L"Linetype");
    catalogSymbolTable(s,s.db->getSymbolTableId<AcDbViewTable>(),L"View");catalogSymbolTable(s,s.db->getSymbolTableId<AcDbUCSTable>(),L"UCS");
    catalogSymbolTable(s,s.db->getSymbolTableId<AcDbViewportTable>(),L"Viewport");catalogSymbolTable(s,s.db->getSymbolTableId<AcDbRegAppTable>(),L"RegApp");catalogSymbolTable(s,s.db->getSymbolTableId<AcDbDimStyleTable>(),L"DimStyle");
    std::map<Adesk::IntDbId,bool> visited;catalogDictionary(s,s.db->namedObjectsDictionaryId(),L"NOD",visited);
    pinOwnedResource(s,s.markerId,L"OwnedMarker");pinOwnedResource(s,s.rootId,L"OwnedModelSpace");
    s.catalogReady=true;s.log("ownedResourceCatalog.ready",{{"catalog",catalogObservation(s)},{"explicitDbWritesCompletedBeforePin","true"},{"modelCreated","false"}});
    save(s.config.output+L"\\owned-resource-catalog.json",catalogObservation(s));
}
AcGiDrawable* diagnosticGetInterface(Adesk::IntDbId objectId,bool bNeedsValidation)noexcept{
    try{std::lock_guard<std::mutex> lock(providerMutex);if(!modelProvider){++providerOrphanCalls;return nullptr;}auto& s=*modelProvider;++s.providerGetCalls;
        const auto found=s.resources.find(objectId);auto* r=found==s.resources.end()?nullptr:&found->second;
        const bool matched=s.catalogReady&&r&&r->usable&&r->pinOwned&&r->drawable;
        const auto reason=matched?std::string():!s.catalogReady?std::string("owned cache not ready"):!r?std::string("raw ID absent from public owned catalog"):r->unavailable.empty()?std::string("catalog entry has no safely pinned drawable"):r->unavailable;
        if(matched){++r->gets;++r->borrowed;++s.providerMatchedGets;}else s.persistentAdapterUnavailable=true;
        try{s.log("modelProvider.get",{{"objectIdRaw",std::to_string(objectId)},{"bNeedsValidation",bNeedsValidation?"true":"false"},{"threadId",std::to_string(GetCurrentThreadId())},{"call",std::to_string(s.providerGetCalls)},{"matched",matched?"true":"false"},{"status",matched?q("owned-read-open-drawable"):q("persistent-adapter-unavailable")},{"unsupportedReason",quote(reason)},{"entry",r?resourceRecord(*r):"null"},{"returnDrawable",matched?address(r->drawable):"null"},{"nullReturnHandlingGuaranteed","false"},{"validationProtocolInferred","false"}});}catch(...){++s.providerLogFailures;s.providerObservationFailed=true;}
        return matched?r->drawable:nullptr;
    }catch(...){providerBoundaryFailure=true;}return nullptr;
}
void diagnosticReleaseInterface(AcGiDrawable* drawable)noexcept{
    try{std::lock_guard<std::mutex> lock(providerMutex);if(!modelProvider){++providerOrphanCalls;return;}auto& s=*modelProvider;++s.providerReleaseCalls;OwnedResource* r{};
        for(auto& item:s.resources)if(item.second.pinOwned&&item.second.drawable==drawable){r=&item.second;break;}
        const bool matched=r&&r->borrowed!=0;if(matched){--r->borrowed;++r->releases;++s.providerMatchedReleases;}else s.providerBorrowBalanceFailed=true;
        try{s.log("modelProvider.release",{{"drawable",address(drawable)},{"threadId",std::to_string(GetCurrentThreadId())},{"call",std::to_string(s.providerReleaseCalls)},{"matchedBorrow",matched?"true":"false"},{"status",matched?q("borrow-returned-pin-retained"):q("unknown-pointer-or-borrow-underflow")},{"entry",r?resourceRecord(*r):"null"},{"closedOrDeleted","false"}});}catch(...){++s.providerLogFailures;s.providerObservationFailed=true;}
    }catch(...){providerBoundaryFailure=true;}
}
bool providerUnsupported(Session& s){std::lock_guard<std::mutex> lock(providerMutex);return s.persistentAdapterUnavailable||s.providerObservationFailed||s.providerBorrowBalanceFailed||providerOrphanCalls!=0||providerBoundaryFailure;}
bool providerBlocksUnload(){std::lock_guard<std::mutex> lock(providerMutex);return modelProvider||providerOrphanCalls||providerBoundaryFailure;}
std::string providerObservation(Session& s){std::lock_guard<std::mutex> lock(providerMutex);return object({{"installed",s.providerEverInstalled?"true":"false"},{"getFunction",s.kernelModelControl?q("diagnosticGetInterface"):"null"},{"releaseFunction",s.kernelModelControl?q("diagnosticReleaseInterface"):"null"},{"getCalls",std::to_string(s.providerGetCalls)},{"releaseCalls",std::to_string(s.providerReleaseCalls)},{"matchedGetReturns",std::to_string(s.providerMatchedGets)},{"matchedReleaseReturns",std::to_string(s.providerMatchedReleases)},{"borrowedAtModelDeletion",std::to_string(s.borrowedAtModelDeletion)},{"observedReturnIncomplete",s.borrowedAtModelDeletion?"true":"false"},{"borrowBalanceFailed",s.providerBorrowBalanceFailed?"true":"false"},{"actualInvocation",!s.providerEverInstalled?q("not-installed"):s.providerGetCalls||s.providerReleaseCalls?q("called"):q("not-called")},{"managerInternalCallbacksObserved","false"},{"boundaryFailure",providerBoundaryFailure?"true":"false"},{"persistentAdapterUnavailable",s.persistentAdapterUnavailable?"true":"false"},{"observationFailed",s.providerObservationFailed?"true":"false"},{"logFailures",std::to_string(s.providerLogFailures)},{"orphanCalls",std::to_string(providerOrphanCalls)},{"providerStillRegistered",s.providerRegistered?"true":"false"},{"persistentAdapterImplemented",s.catalogStarted?q("finite-public-owned-read-open-catalog"):q("not-installed")},{"dynamicResourceMatchObserved",s.providerMatchedGets?"true":"false"},{"callbackCorrectnessProven","false"},{"nullReturnHandlingGuaranteed","false"},{"catalog",catalogObservation(s)}});}
const char* modelFactory(const Session& s){return s.kernelModelControl?"kernel.createModel(kMain,cookie,diagnosticGetInterface,diagnosticReleaseInterface)":"manager.createAutoCADModel";}
const char* modelDestroyer(const Session& s){return s.kernelModelControl?"kernel.deleteModel":"manager.destroyAutoCADModel";}
std::unique_ptr<Session> session;
std::vector<std::unique_ptr<Session>> lifecycleSessions;
std::vector<std::unique_ptr<Session>> diagnosticSessions;
AcGiDrawable* graphRoot(Session& s){return s.positiveControl?static_cast<AcGiDrawable*>(s.primitive.get()):static_cast<AcGiDrawable*>(s.sceneRoot.get());}
void RootObserver::record(bool added,AcGsModel* model,AcGiDrawable* root)noexcept{
    if(added)++s.rootsAdded;else ++s.rootsErased;
    if(s.positiveControl&&(model!=s.model||root!=s.expectedRoot))s.diagnosticIdentityFailed=true;
    if(s.rootDetails>=32)return;
    ++s.rootDetails;
    try{s.log(added?"modelReactor.rootAdded":"modelReactor.rootErased",{{"model",address(model)},{"root",address(root)},{"expectedModel",address(s.model)},{"expectedRoot",address(s.expectedRoot)},{"modelMatches",model==s.model?"true":"false"},{"rootMatches",root==s.expectedRoot?"true":"false"},{"addedCount",std::to_string(s.rootsAdded)},{"erasedCount",std::to_string(s.rootsErased)},{"detailIndex",std::to_string(s.rootDetails)},{"returnValue","true"}});}
    catch(...){++s.rootLogFailures;s.rootObservationFailed=true;}
}
bool RootObserver::onSceneGraphRootAdded(AcGsModel* model,AcGiDrawable* root){record(true,model,root);return true;}
bool RootObserver::onSceneGraphRootErased(AcGsModel* model,AcGiDrawable* root){record(false,model,root);return true;}
void observeModels(Session& s,const char* stage){
    AcArray<AcGsModel*> models;s.view->getModelList(models);std::vector<std::string> pointers;bool contains{};
    for(int i=0;i<models.length();++i){pointers.push_back(address(models[i]));if(models[i]==s.model)contains=true;}
    s.log("view.modelList",{{"stage",q(stage)},{"view",address(s.view)},{"count",std::to_string(models.length())},{"models",array(pointers)},{"expectedModel",address(s.model)},{"containsExpected",contains?"true":"false"}});
}
void observeDisplayConfig(Session& s){
    // The SDK documents byte lengths; this number fits both WCHAR and byte capacities.
    constexpr int length=32768;std::array<ACHAR,length> path{},driver{},name{};
    const auto usable=[=](const std::array<ACHAR,length>& value){const auto end=std::find(value.begin(),value.end(),L'\0');const auto used=static_cast<std::size_t>(end-value.begin());return end!=value.end()&&used>0&&used<length/sizeof(ACHAR)-1;};
    const auto buffer=[=](const std::array<ACHAR,length>& value){const auto end=std::find(value.begin(),value.end(),L'\0');const auto used=static_cast<std::size_t>(end-value.begin());const bool nul=end!=value.end();
        const bool complete=usable(value);
        return object({{"raw",quote(std::wstring(value.begin(),end))},{"allocatedWchars",std::to_string(value.size())},{"allocatedBytes",std::to_string(sizeof(value))},{"lengthArgument",std::to_string(length)},{"nulPresent",nul?"true":"false"},{"firstNulIndex",nul?std::to_string(used):"null"},{"valueStatus",complete?q("nonempty-within-conservative-limit"):q("unknown-empty-or-possibly-truncated")}});};
    auto* config=s.kernel->getConfig();s.log("displayConfig.source",{{"kernel",address(s.kernel)},{"config",address(config)},{"scope",q("singleton display configuration")},{"offscreenActiveDriverProven","false"}});if(!config){s.log("displayConfig.unavailable",{{"valueStatus",q("unknown-config-unavailable")}});return;}
    bool accelerated=false;config->currentDisplayDriver(path.data(),length,driver.data(),length,accelerated);
    const bool outputAvailable=usable(path)&&usable(driver);
    s.log("displayConfig.currentDisplayDriver",{{"path",buffer(path)},{"driver",buffer(driver)},{"acceleratedInitial","false"},{"acceleratedAfterRaw",accelerated?"true":"false"},{"hardwareAccelerationCapability",outputAvailable?(accelerated?"true":"false"):"null"},{"capabilityStatus",outputAvailable?q("public-output-contract-with-nonempty-terminated-names"):q("unknown-incomplete-driver-output-raw-storage-only")},{"accelerationEnabledProven","false"}});
    config->driverName(name.data(),length);s.log("displayConfig.driverName",{{"name",buffer(name)}});
    const auto virtualName=config->getVirtualDeviceName();s.log("displayConfig.virtualDeviceName",{{"raw",quote(std::wstring(virtualName.kACharPtr()))},{"valueStatus",virtualName.isEmpty()?q("unknown-empty"):q("nonempty-getter-output")}});
}
Adesk::UInt32 PrimitiveRoot::subSetAttributes(AcGiDrawableTraits*){++s.attributeCalls;if(s.attributeCalls<=64){try{s.log("nonpersistent.subSetAttributes",{{"flags",std::to_string(kDrawableNone)},{"diagnosticOnly","true"}});}catch(...){s.callbackError="nonpersistent setAttributes observation log failed";}}return kDrawableNone;}
Adesk::Boolean PrimitiveRoot::subWorldDraw(AcGiWorldDraw* wd){
    ++s.callbacks;++s.frameCallbacks;try{s.log("nonpersistent.realSubWorldDraw",{{"diagnosticOnly","true"},{"qualification","false"},{"isPersistent","false"},{"objectId","null"},{"context",address(wd?wd->context():nullptr)},{"contextDatabase",address(wd&&wd->context()?wd->context()->database():nullptr)},{"ownedDatabaseForComparisonOnly",address(s.db.get())},{"regenType",wd?std::to_string(wd->regenType()):"null"},{"sideDbSuccessClaimed","false"}});
        require(wd!=nullptr,"nonpersistent GS callback has null WorldDraw");require(s.callbacks<=64,"nonpersistent callback budget");drawCalibration(*wd);
    }catch(const std::exception& e){s.callbackError=e.what();}catch(...){s.callbackError="nonpersistent callback unknown exception";}return Adesk::kTrue;
}
void PrimitiveRoot::subViewportDraw(AcGiViewportDraw*){++s.viewportCalls;if(s.viewportCalls<=64){try{s.log("nonpersistent.subViewportDraw",{{"diagnosticOnly","true"}});}catch(...){s.callbackError="nonpersistent viewportDraw observation log failed";}}}
int dbmod(){resbuf value{};require(acedGetVar(L"DBMOD",&value)==RTNORM&&value.restype==RTSHORT,"DBMOD read failed");return value.resval.rint;}
std::string variant(const AcGiVariant& v){
    std::string value;
    switch(v.type()){
    case AcGiVariant::kUndefined:value="null";break;
    case AcGiVariant::kBoolean:value=v.asBoolean()?"true":"false";break;
    case AcGiVariant::kInt:value=std::to_string(v.asInt());break;
    case AcGiVariant::kDouble:value=num(v.asDouble());break;
    case AcGiVariant::kColor:{const auto c=v.asColor();value=object({{"method",std::to_string(c.colorMethod())},{"index",std::to_string(c.colorIndex())},{"rgb",array({std::to_string(c.red()),std::to_string(c.green()),std::to_string(c.blue())})}});break;}
    case AcGiVariant::kString:value=quote(std::wstring(v.asString().kACharPtr()));break;
    default:throw std::runtime_error("unexpected table variant for controlled visual style property");
    }
    return object({{"variantType",std::to_string(v.type())},{"value",value}});
}
struct StyleCheck{bool raw{},active{};};
StyleCheck inspectStyle(Session& s,const AcGiVisualStyle& requested,const AcGiVisualStyle& actual,bool styleSet,const char* stage,bool gateApplied,const char* origin){
    using namespace AcGiVisualStyleProperties;
    const std::array<std::pair<Property,const char*>,16> properties{{
        {kFaceLightingModel,"faceLightingModel"},{kFaceLightingQuality,"faceLightingQuality"},{kFaceColorMode,"faceColorMode"},{kFaceOpacity,"faceOpacity"},
        {kEdgeModel,"edgeModel"},{kEdgeStyles,"edgeStyles"},{kEdgeModifiers,"edgeModifiers"},{kEdgeColor,"edgeColor"},{kEdgeOpacity,"edgeOpacity"},{kEdgeWidth,"edgeWidth"},
        {kDisplayStyles,"displayStyles"},{kDisplayBrightness,"displayBrightness"},{kUseDrawOrder,"useDrawOrder"},{kViewportTransparency,"viewportTransparency"},{kLightingEnabled,"lightingEnabled"},{kFaceModifiers,"faceModifiers"}
    }};
    if(!styleSet){
        std::vector<std::string> unavailable;for(const auto& p:properties){AcGiVisualStyle::Operation expectedOp;const auto& expected=requested.trait(p.first,&expectedOp);
            unavailable.push_back(object({{"property",q(p.second)},{"propertyId",std::to_string(p.first)},{"expected",variant(expected)},{"actual","null"},{"expectedOperation",std::to_string(expectedOp)},{"actualOperation","null"},{"equivalent","null"},{"applicable","null"},{"inapplicableReason",q("value getter returned false; output is not an observation")}}));}
        s.styleLog({{"contract",q("XY-custom-invisible-faces-object-color-edges-v1")},{"stage",q(stage)},{"actualOrigin",q(origin)},{"observationAvailable","false"},{"gateApplied",gateApplied?"true":"false"},{"requestedType",std::to_string(requested.type())},{"styleSet","false"},{"actualType","null"},{"allRawPropertiesEquivalent","null"},{"activeDrawingPropertiesEquivalent","null"},{"properties",array(unavailable)}});return {};
    }
    const auto& expectedFaces=requested.trait(kFaceLightingModel);const auto& actualFaces=actual.trait(kFaceLightingModel);
    const bool facesInvisible=expectedFaces.type()==AcGiVariant::kInt&&actualFaces.type()==AcGiVariant::kInt&&expectedFaces.asInt()==kInvisible&&actualFaces.asInt()==kInvisible&&requested.operation(kFaceLightingModel)==AcGiVisualStyleOperations::kSet&&actual.operation(kFaceLightingModel)==AcGiVisualStyleOperations::kSet;
    bool activeEqual=true,rawEqual=true;std::vector<std::string> traits;
    for(const auto& p:properties){AcGiVisualStyle::Operation expectedOp,actualOp;const auto& expected=requested.trait(p.first,&expectedOp);const auto& observed=actual.trait(p.first,&actualOp);const bool same=expected==observed&&expectedOp==actualOp;
        const bool applicable=!(facesInvisible&&(p.first==kFaceLightingQuality||p.first==kFaceColorMode||p.first==kFaceOpacity));rawEqual=rawEqual&&same;if(applicable)activeEqual=activeEqual&&same;
        traits.push_back(object({{"property",q(p.second)},{"propertyId",std::to_string(p.first)},{"expected",variant(expected)},{"actual",variant(observed)},{"expectedOperation",std::to_string(expectedOp)},{"actualOperation",std::to_string(actualOp)},{"equivalent",same?"true":"false"},{"applicable",applicable?"true":"false"},{"inapplicableReason",applicable?"null":q("kInvisible means faces are not drawn")}}));}
    s.styleLog({{"contract",q("XY-custom-invisible-faces-object-color-edges-v1")},{"stage",q(stage)},{"actualOrigin",q(origin)},{"observationAvailable","true"},{"gateApplied",gateApplied?"true":"false"},{"requestedType",std::to_string(requested.type())},{"styleSet",styleSet?"true":"false"},{"actualType",styleSet?std::to_string(actual.type()):"null"},{"allRawPropertiesEquivalent",rawEqual?"true":"false"},{"activeDrawingPropertiesEquivalent",activeEqual?"true":"false"},{"properties",array(traits)}});
    return {styleSet&&rawEqual,styleSet&&activeEqual};
}
void configureStyle(AcGiVisualStyle& style){
    using namespace AcGiVisualStyleProperties;const auto set=AcGiVisualStyleOperations::kSet;
    require(style.setType(AcGiVisualStyle::kCustom),"prepare own custom style failed");
    auto integer=[&](Property p,int value){require(style.setTrait(p,value,set),"custom integer style trait failed: "+std::to_string(p));};
    auto real=[&](Property p,double value){require(style.setTrait(p,value,set),"custom real style trait failed: "+std::to_string(p));};
    auto boolean=[&](Property p,bool value){require(style.setTrait(p,value,set),"custom boolean style trait failed: "+std::to_string(p));};
    integer(kFaceLightingModel,kInvisible);integer(kFaceLightingQuality,kNoLighting);integer(kFaceColorMode,kObjectColor);integer(kFaceModifiers,kNoFaceModifiers);real(kFaceOpacity,1.0);
    integer(kEdgeModel,kIsolines);integer(kEdgeStyles,kVisibleFlag|kObscuredFlag);integer(kEdgeModifiers,kNoEdgeModifiers);real(kEdgeOpacity,1.0);integer(kEdgeWidth,1);
    AcCmColor unusedEdgeColor;ok(unusedEdgeColor.setColorIndex(256),"prepare inactive edge color");require(style.setTrait(kEdgeColor,&unusedEdgeColor,set),"custom edge color trait failed");
    integer(kDisplayStyles,kBackgroundsFlag);real(kDisplayBrightness,0.0);boolean(kUseDrawOrder,false);boolean(kViewportTransparency,false);boolean(kLightingEnabled,false);
}
void observeModelDefaultStyle(Session& s){
    const auto id=s.model->visualStyle();AcGiVisualStyle value,requested;configureStyle(requested);const bool available=s.model->visualStyle(value);
    s.modelDefaultStyle=object({{"styleOldId",std::to_string(id.asOldId())},{"styleDatabase",address(id.database())},{"valueGetterReturned",available?"true":"false"},{"modelStyleSetterUsed","false"}});
    s.log("model.defaultVisualStyle",{{"observation",s.modelDefaultStyle}});
    inspectStyle(s,requested,value,available,"fresh-model-default",false,"model-value-getter-before-any-style-setter");
}
void verifyOwnedStyleSource(Session& s,const AcGiVisualStyle& requested,const char* stage){
    auto reopened=open<AcDbVisualStyle>(s.styleId);require(reopened->database()==s.db.get()&&reopened->objectId()==s.styleId,"persistent style ownership mismatch");AcGiVisualStyle stored;ok(reopened->copyTo(&stored),"copy reopened own style to value");
    s.log("ownedPersistentStyle",{{"stage",q(stage)},{"styleOldId",std::to_string(s.styleId.asOldId())},{"styleDatabase",address(reopened->database())},{"targetDatabase",address(s.db.get())},{"styleObjectIdDatabase",address(s.styleId.database())}});
    require(inspectStyle(s,requested,stored,true,stage,true,"owned-AcDbVisualStyle-copyTo").raw,"owned persistent style roundtrip differs in raw values or operations");
}
void createOwnedStyle(Session& s,const AcGiVisualStyle& requested){
    AcDbDictionary* dictionary{};ok(s.db->getVisualStyleDictionary(dictionary,AcDb::kForWrite),"open own visual style dictionary");std::unique_ptr<AcDbDictionary,Close> ownedDictionary(dictionary);require(dictionary&&dictionary->database()==s.db.get(),"visual style dictionary database mismatch");
    auto style=std::make_unique<AcDbVisualStyle>();ok(style->copyFrom(&requested),"copy custom value to own AcDbVisualStyle");
    ok(dictionary->setAt(L"N9C_OWNED_XY_CUSTOM",style.get(),s.styleId),"post owned visual style");auto* posted=style.release();ok(posted->close(),"close posted owned visual style");ownedDictionary.reset();verifyOwnedStyleSource(s,requested,"owned-db-close-reopen-copyTo");
}
bool bindOwnedStyle(Session& s,const AcGiVisualStyle& requested,const char* stage){
    require(!s.styleId.isNull()&&s.styleId.database()==s.db.get(),"owned style ID database mismatch");
    verifyOwnedStyleSource(s,requested,"frame-owned-source-reopened");
    // Caller has already successfully added the exact marker/model to this view.
    s.log("view.setOwnedStyleId.before",{{"stage",q(stage)},{"styleOldId",std::to_string(s.styleId.asOldId())}});s.view->setVisualStyle(s.styleId);const auto observedId=s.view->visualStyle();s.styleBound=observedId==s.styleId;
    s.log("view.setOwnedStyleId.after",{{"stage",q(stage)},{"requestedOldId",std::to_string(s.styleId.asOldId())},{"actualOldId",std::to_string(observedId.asOldId())},{"actualDatabase",address(observedId.database())},{"targetDatabase",address(s.db.get())},{"bound",s.styleBound?"true":"false"}});
    require(s.styleBound&&observedId.database()==s.db.get(),"view persistent style ID binding mismatch");AcGiVisualStyle observed;const bool styleSet=s.view->visualStyle(observed);const auto value=inspectStyle(s,requested,observed,styleSet,stage,styleSet&&!s.positiveControl,"view-value-getter-after-owned-ObjectId-setter");
    if(s.positiveControl)s.styleBindings.push_back(object({{"stage",q(stage)},{"requestedTransport",q("owned-ObjectId")},{"requestedOldId",std::to_string(s.styleId.asOldId())},{"actualOldId",std::to_string(observedId.asOldId())},{"actualDatabase",address(observedId.database())},{"idMatchesSource",s.styleBound?"true":"false"},{"valueGetterReturned",styleSet?"true":"false"}}));
    s.log("ownedIdStyleConfiguration",{{"sourceAndIdVerified","true"},{"valueObservationAvailable",styleSet?"true":"false"},{"valueContradiction",styleSet&&!value.active?"true":"false"},{"rendererConsistencyClaimed","false"}});return !styleSet||value.active;
}
bool bindStyle(Session& s,const AcGiVisualStyle& requested,const char* stage){
    if(!s.valueStyleControl)return bindOwnedStyle(s,requested,stage);
    require(s.positiveControl&&s.diagnosticOnly,"value transport restricted to unqualified nonpersistent diagnostic");
    require(!s.styleId.isNull()&&s.styleId.database()==s.db.get(),"value arm persistent source ID database mismatch");
    verifyOwnedStyleSource(s,requested,"frame-owned-source-reopened");
    const auto priorId=s.view->visualStyle();
    s.log("view.setValueStyle.before",{{"stage",q(stage)},{"priorOldId",std::to_string(priorId.asOldId())},{"priorDatabase",address(priorId.database())},{"ownedIdSetterUsed","false"}});
    s.view->setVisualStyle(requested);const auto observedId=s.view->visualStyle();
    AcGiVisualStyle observed;const bool styleSet=s.view->visualStyle(observed);const auto value=inspectStyle(s,requested,observed,styleSet,stage,false,"view-value-getter-after-direct-value-setter");
    const auto binding=Fields{{"stage",q(stage)},{"requestedTransport",q("direct-AcGiVisualStyle-value")},{"sourceOldId",std::to_string(s.styleId.asOldId())},{"priorOldId",std::to_string(priorId.asOldId())},{"actualOldId",std::to_string(observedId.asOldId())},{"actualDatabase",address(observedId.database())},{"idMatchesSource",observedId==s.styleId?"true":"false"},{"valueGetterReturned",styleSet?"true":"false"},{"sourceAndIdentityVerified","true"},{"ownedIdSetterUsed","false"},{"idClearingAssumed","false"}};
    s.styleBindings.push_back(object(binding));s.log("view.setValueStyle.after",binding);return styleSet&&value.active;
}
bool Rule::isApplicable(const AcRxObject* object)const{
    ++s.applicableCalls;const auto* p=AcDbPoint::cast(object);const bool owned=p&&p->database()==s.db.get()&&p->objectId()==s.markerId;
    if(owned&&++s.ownedApplicableCalls<=64){try{s.log("ownedMarker.isApplicable",{{"running",running?"true":"false"},{"objectOldId",std::to_string(p->objectId().asOldId())},{"database",address(p->database())},{"ownedCall",std::to_string(s.ownedApplicableCalls)}});}catch(...){s.callbackError="isApplicable observation log failed";}}
    return running&&owned;
}
Adesk::UInt32 Rule::setAttributes(AcGiDrawable* drawable,AcGiDrawableTraits* traits){
    const auto flags=AcGiDrawableOverrule::setAttributes(drawable,traits);++s.attributeCalls;
    if(s.attributeCalls<=64){try{s.log("ownedMarker.setAttributes",{{"flags",std::to_string(flags)},{"viewIndependentViewportDraw",(flags&AcGiDrawable::kDrawableViewIndependentViewportDraw)?"true":"false"},{"regenDraw",(flags&AcGiDrawable::kDrawableRegenDraw)?"true":"false"}});}catch(...){s.callbackError="setAttributes observation log failed";}}return flags;
}
void Rule::viewportDraw(AcGiDrawable* drawable,AcGiViewportDraw* vd){
    ++s.viewportCalls;if(s.viewportCalls<=64){try{s.log("ownedMarker.viewportDraw",{{"context",address(vd?vd->context():nullptr)},{"contextDatabase",address(vd&&vd->context()?vd->context()->database():nullptr)}});}catch(...){s.callbackError="viewportDraw observation log failed";}}
    AcGiDrawableOverrule::viewportDraw(drawable,vd);
}
Adesk::Boolean Rule::worldDraw(AcGiDrawable* drawable,AcGiWorldDraw* wd){
    try{
        auto* p=AcDbPoint::cast(drawable);++s.callbacks;++s.frameCallbacks;
        s.log("realWorldDraw",{{"sample",std::to_string(s.sample)},{"context",address(wd?wd->context():nullptr)},{"contextDatabase",address(wd&&wd->context()?wd->context()->database():nullptr)},
            {"entityDatabase",address(p?p->database():nullptr)},{"targetDatabase",address(s.db.get())},{"entityOldId",p?std::to_string(p->objectId().asOldId()):"0"},
            {"expectedOldId",std::to_string(s.markerId.asOldId())},{"boundModel",address(s.kernel->find(reinterpret_cast<Adesk::LongPtr>(s.db.get()),AcGsModel::kMain))},{"expectedModel",address(s.model)}});
        require(p&&p->database()==s.db.get()&&p->objectId()==s.markerId,"unexpected drawable");
        require(wd&&wd->context()&&wd->context()->database()==s.db.get(),"real WorldDraw context database differs from owned SideDB");
        require(s.kernel->find(reinterpret_cast<Adesk::LongPtr>(s.db.get()),AcGsModel::kMain)==s.model,"GS association changed");
        require(s.callbacks<=300,"callback budget");
        auto& g=wd->geometry();auto& traits=wd->subEntityTraits();traits.setLineWeight(AcDb::kLnWt000);
        if(s.sample<0){
            drawCalibration(*wd);
            return Adesk::kTrue;
        }
        AcCmEntityColor black;black.setRGB(0,0,0);traits.setTrueColor(black);
        const auto& c=controls.at(static_cast<std::size_t>(s.sample));
        AcGiTextStyle style(s.font.c_str(),s.bigfont.c_str(),7,0.75,c.oblique,c.tracking,c.backward,false,false,false,false);
        const int flags=style.loadStyleRec(s.db.get());
        s.log("loadStyleRec",{{"id",q(c.id)},{"bits",std::to_string(flags)},{"fontAfter",quote(std::wstring(style.fileName()?style.fileName():L""))},{"bigfontAfter",quote(std::wstring(style.bigFontFileName()?style.bigFontFileName():L""))}});
        require((flags&1)!=0&&(flags&30)==0,"controlled font missing or substituted");
        require(style.fileName()&&style.bigFontFileName()&&_wcsicmp(style.fileName(),s.font.c_str())==0&&_wcsicmp(style.bigFontFileName(),s.bigfont.c_str())==0,"loaded font path differs");
        require(sha256(s.font)==s.fontSHA&&sha256(s.bigfont)==s.bigfontSHA,"font changed");
        unsigned pushed{};
        struct Pop final{AcGiGeometry& g;unsigned& n;Session& s;~Pop(){while(n){const auto r=g.popModelTransform();--n;try{s.log("popModelTransform",{{"raw",r?"true":"false"}});}catch(...){}if(!r)s.callbackError="popModelTransform failed";}}} pop{g,pushed,s};
        if(c.nested){AcGeMatrix3d outer,inner;outer.setCoordSystem({32,25,0},{1.4,0.3,0},{0.2,0.8,0},{0,0,1});inner.setCoordSystem({4,3,0},{0.8,0.25,0},{-0.15,1.1,0},{0,0,1});
            require(g.pushModelTransform(outer),"push outer failed");++pushed;require(g.pushModelTransform(inner),"push inner failed");++pushed;}
        AcGeMatrix3d net;g.getModelToWorldTransform(net);s.log("textRequest",{{"id",q(c.id)},{"modelToWorld",matrix(net)},{"height","7"},{"widthFactor","0.75"},{"tracking",num(c.tracking)},{"oblique",num(c.oblique)},{"backward",c.backward?"true":"false"}});
        const auto raw=g.text(c.position,AcGeVector3d::kZAxis,{std::cos(c.angle),std::sin(c.angle),0},c.text,static_cast<int>(wcslen(c.text)),true,style);
        s.log("textReturn",{{"raw",raw?"true":"false"},{"falseMeansStored","true"}});require(!raw,"text terminated");return Adesk::kTrue;
    }catch(const std::exception& e){s.callbackError=e.what();return Adesk::kTrue;}catch(...){s.callbackError="unknown worldDraw exception";return Adesk::kTrue;}
}
std::wstring resolve(AcDbDatabase* db,const wchar_t* name){AcString p;ok(acdbHostApplicationServices()->findFile(p,name,db,AcDbHostApplicationServices::kFontFile),"resolve font");std::wstring result=p.kACharPtr();require(result.size()>3&&result[1]==L':'&&GetFileAttributesW(result.c_str())!=INVALID_FILE_ATTRIBUTES,"font path not existing absolute");return result;}
std::vector<std::uint8_t> snapshot(Session& s,int w,int h){
    Atil::RgbModel dm(Atil::RgbModelAttributes::k3Channels,Atil::DataModelAttributes::kRedGreenBlueAlpha);
    Atil::ImagePixel white(Atil::DataModelAttributes::kData32);white.value.rgba=0xffffffff;
    Atil::Image image(Atil::Size(w,h),&dm,white);s.device->getSnapShot(&image,AcGsDCPoint(0,0));
    const auto order=image.dataModel().channelOrder();require(order==Atil::DataModelAttributes::kRedGreenBlueAlpha||order==Atil::DataModelAttributes::kBlueGreenRedAlpha,"unexpected ATIL channel order");
    require(Atil::DataModel::bitsPerPixel(image.dataModel().bitsPerPixel())==32,"ATIL pixel not 32 bit");
    s.log("snapshot",{{"channelOrder",order==Atil::DataModelAttributes::kRedGreenBlueAlpha?q("RGBA"):q("BGRA")},{"ATILRowOrigin",q("upper-left")},{"width",std::to_string(w)},{"height",std::to_string(h)}});
    std::unique_ptr<Atil::ImageContext> ctx(image.createContext(Atil::ImageContext::kRead,Atil::Size(w,h),Atil::Offset(0,0)));require(ctx!=nullptr,"ATIL read context unavailable");
    std::vector<std::uint8_t> row(static_cast<std::size_t>(w)*4),rgb(static_cast<std::size_t>(w)*h*3);
    for(int y=0;y<h;++y){ctx->getRow(y,0,w,row.data());for(int x=0;x<w;++x){const auto a=static_cast<std::size_t>(x)*4,b=(static_cast<std::size_t>(y)*w+x)*3;rgb[b]=row[a+(order==Atil::DataModelAttributes::kRedGreenBlueAlpha?0:2)];rgb[b+1]=row[a+1];rgb[b+2]=row[a+(order==Atil::DataModelAttributes::kRedGreenBlueAlpha?2:0)];}}
    return rgb;
}
struct Mapping{bce::TextAffine2 affine;double phase{};bool flipY{};};
Mapping calibrate(Session& s,const std::vector<std::uint8_t>& rgb,int w,int h,const AcGeMatrix3d& m){
    std::array<bce::Point2,5> observed{};std::array<std::size_t,5> count{};
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){const auto k=(static_cast<std::size_t>(y)*w+x)*3;for(std::size_t c=0;c<colors.size();++c)if(rgb[k]==colors[c][0]&&rgb[k+1]==colors[c][1]&&rgb[k+2]==colors[c][2]){observed[c].x+=x+0.5;observed[c].y+=y+0.5;++count[c];}}
    for(std::size_t i=0;i<count.size();++i){require(count[i]>=16,"calibration color absent or too small");observed[i].x/=count[i];observed[i].y/=count[i];}
    // Only documented row orientation alternatives and integer-vs-center phase are candidates.
    // Fixed tolerance set before host run. No text fitting and no free affine fit.
    double best=1e100;Mapping selected;std::vector<std::string> trials,points;
    for(bool flip:{false,true})for(double phase:{0.0,0.5}){
        double residual{};for(std::size_t i=0;i<3;++i){const auto p=m*calibration[i];const double ex=p.x+phase,ey=(flip?h-p.y:p.y)+phase;residual=std::max(residual,std::max(std::abs(ex-observed[i].x),std::abs(ey-observed[i].y)));}
        trials.push_back(object({{"flipY",flip?"true":"false"},{"phase",num(phase)},{"maxCenterResidual",num(residual)}}));
        if(residual<best){best=residual;selected={{m(0,0),m(0,1),m(0,3)+phase,flip?-m(1,0):m(1,0),flip?-m(1,1):m(1,1),(flip?h-m(1,3):m(1,3))+phase},phase,flip};}
    }
    double heldOut{};for(std::size_t i=0;i<count.size();++i){const auto p=m*calibration[i];const double dx=p.x+selected.phase-observed[i].x,dy=(selected.flipY?h-p.y:p.y)+selected.phase-observed[i].y;
        if(i>=3)heldOut=std::max(heldOut,std::max(std::abs(dx),std::abs(dy)));
        points.push_back(object({{"world",array({num(calibration[i].x),num(calibration[i].y)})},{"role",i<3?q("selection"):q("held-out")},{"observedPixelCellCenter",point(observed[i])},{"solidColorPixels",std::to_string(count[i])},{"selectedResidual",array({num(dx),num(dy)})}}));}
    save(s.config.output+L"\\calibration-"+std::to_wstring(w)+L".json",object({{"worldToDevice",matrix(m)},{"points",array(points)},{"candidates",array(trials)},{"selectedWorldToPixel",affine(selected.affine)},{"selectionMaxResidual",num(best)},{"heldOutMaxResidual",num(heldOut)},{"fixedTolerancePixels","0.55"},{"selectedPhase",num(selected.phase)},{"flipY",selected.flipY?"true":"false"}}));
    require(best<=0.55&&heldOut<=0.55,"GS/ATIL selection or independent held-out calibration failed fixed 0.55 pixel bound");return selected;
}
std::vector<std::uint8_t> render(Session& s,int w,int h,int sample,const AcGiVisualStyle& requested,bool diagnosticOnly=false){
    s.sample=sample;s.frameCallbacks=0;s.callbackError.clear();s.view->eraseAll();require(s.view->add(graphRoot(s),s.model),"add controlled scene root failed");
    observeModels(s,"frame-after-root-bind");require(!s.rootObservationFailed,"root notification observation failed");
    const bool styleMatches=bindStyle(s,requested,"frame-after-erase-readd-model");s.log("frameStyleQualification",{{"diagnosticOnly",diagnosticOnly?"true":"false"},{"activeContractSatisfied",styleMatches?"true":"false"}});if(!diagnosticOnly)require(styleMatches,"active style contract changed after erase/readd");
    AcGsDCRect viewport;s.view->getViewport(viewport);s.log("ownedViewState",{{"visible",s.view->isVisible()?"true":"false"},{"interactive",s.view->isInteractive()?"true":"false"},{"viewValid",s.view->isValid()?"true":"false"},{"deviceValid",s.device->isValid()?"true":"false"},{"viewDevice",address(s.view->getDevice())},{"expectedDevice",address(s.device)},{"rootModel",address(s.view->getModel(graphRoot(s)))},{"expectedModel",address(s.model)},{"markerPointInView",s.view->pointInView(s.marker->position())?"true":"false"},{"viewport",array({std::to_string(viewport.m_min.x),std::to_string(viewport.m_min.y),std::to_string(viewport.m_max.x),std::to_string(viewport.m_max.y)})},{"worldToDevice",matrix(s.view->worldToDeviceMatrix())},{"updateSuppressedObservation","null"}});
    if(s.positiveControl){s.diagnosticIdentityVerified=false;require(!s.diagnosticIdentityFailed&&s.view->getDevice()==s.device&&s.view->getModel(graphRoot(s))==s.model,"nonpersistent diagnostic view/device/root/model identity mismatch");s.diagnosticIdentityVerified=true;}
    s.model->invalidate(AcGsModel::kInvalidateAll);s.view->invalidate();s.device->invalidate();observeModels(s,"device-update-before");s.log("deviceUpdate.before",{{"sample",std::to_string(sample)},{"width",std::to_string(w)}});s.device->update();observeModels(s,"device-update-after");
    s.log("deviceUpdate.after",{{"callbacks",std::to_string(s.frameCallbacks)},{"isApplicableCalls",std::to_string(s.applicableCalls)},{"ownedIsApplicableCalls",std::to_string(s.ownedApplicableCalls)},{"attributeCalls",std::to_string(s.attributeCalls)},{"viewportDrawCalls",std::to_string(s.viewportCalls)},{"viewVisible",s.view->isVisible()?"true":"false"},{"viewValid",s.view->isValid()?"true":"false"},{"deviceValid",s.device->isValid()?"true":"false"},{"callbackError",quote(s.callbackError)}});
    require(s.callbackError.empty(),s.callbackError);const bool interactive=s.view->isInteractive();s.log("viewUpdate.precondition",{{"interactive",interactive?"true":"false"}});require(!interactive,"owned view is interactive; noninteractive update test contract not met");
    observeModels(s,"view-update-before");s.log("viewUpdate.before");s.view->update();observeModels(s,"view-update-after");s.log("viewUpdate.after",{{"callbacks",std::to_string(s.frameCallbacks)},{"isApplicableCalls",std::to_string(s.applicableCalls)},{"ownedIsApplicableCalls",std::to_string(s.ownedApplicableCalls)},{"attributeCalls",std::to_string(s.attributeCalls)},{"viewportDrawCalls",std::to_string(s.viewportCalls)},{"viewValid",s.view->isValid()?"true":"false"},{"deviceValid",s.device->isValid()?"true":"false"},{"callbackError",quote(s.callbackError)}});
    require(s.callbackError.empty(),s.callbackError);require(!s.rootObservationFailed,"root notification observation failed");if(s.frameCallbacks==0){require(!providerUnsupported(s),"persistent-adapter-unavailable or provider observation failed; not an ordinary zero-WorldDraw result");s.zeroWorldDrawObserved=true;}require(s.frameCallbacks>0,"GS produced no real owned WorldDraw callback");return snapshot(s,w,h);
}
void styleMismatchDiagnostic(Session& s,const AcGiVisualStyle& requested){
    s.diagnosticOnly=true;s.log("styleMismatchDiagnostic.begin",{{"diagnosticOnly","true"},{"qualification","false"},{"faultPoint",std::to_string(s.faultPoint)},{"sessionOutput",quote(s.config.output)},{"textRequested","false"}});
    std::string error;std::wstring path;bool mappingVerified{};
    try{auto rgb=render(s,1024,768,-1,requested,true);path=s.config.output+L"\\style-mismatch-calibration-1024.rgb";save(path,std::string(reinterpret_cast<const char*>(rgb.data()),rgb.size()));
        calibrate(s,rgb,1024,768,s.view->worldToDeviceMatrix());mappingVerified=true;
    }catch(const std::exception& e){error=e.what();s.log("styleMismatchDiagnostic.failure",{{"message",quote(error)}});}
    save(s.config.output+L"\\style-mismatch-diagnostic.json",object({{"diagnosticOnly","true"},{"qualification","false"},{"faultPoint",std::to_string(s.faultPoint)},{"sessionOutput",quote(s.config.output)},{"styleIdBound",s.styleBound?"true":"false"},{"styleContractSatisfied","false"},{"realWorldDrawCallbacks",std::to_string(s.frameCallbacks)},{"callbackError",quote(s.callbackError)},{"rawRGBPath",path.empty()?"null":quote(path)},{"rawRGBSHA256",path.empty()?"null":quote(sha256(path))},{"mappingVerified",mappingVerified?"true":"false"},{"diagnosticError",quote(error)},{"textRequested","false"}}));
    throw std::runtime_error("owned style ID bound but active value contract differs; diagnosticOnly=true; "+(error.empty()?std::string("one nontext raw-buffer diagnostic completed"):error));
}
void execute(Session& s){
    if(s.positiveControl)s.diagnosticExecutionStarted=true;
    s.current=acDocManager->curDocument();s.active=acDocManager->mdiActiveDocument();s.ambient=acdbHostApplicationServices()->workingDatabase();
    require(s.current&&s.active==s.current&&s.current->database()==s.ambient,"ambient identities inconsistent");s.dbmodBefore=dbmod();s.ambientInitialized=true;s.ambient->addReactor(&s.watch);s.watchAttached=true;
    s.log("ambient.before",{{"current",address(s.current)},{"active",address(s.active)},{"workingDatabase",address(s.ambient)},{"dbmod",std::to_string(s.dbmodBefore)}});
    s.db=std::make_unique<AcDbDatabase>(true,true);require(s.db.get()!=s.ambient,"owned database aliases ambient");
    s.font=resolve(s.db.get(),L"simplex.shx");s.bigfont=resolve(s.db.get(),L"gbcbig.shx");s.fontSHA=sha256(s.font);s.bigfontSHA=sha256(s.bigfont);
    save(s.config.output+L"\\fonts.json",object({{"font",quote(s.font)},{"fontSHA256",quote(s.fontSHA)},{"bigfont",quote(s.bigfont)},{"bigfontSHA256",quote(s.bigfontSHA)},{"height","7"},{"widthFactor","0.75"}}));
    require(_wcsicmp(s.font.c_str(),L"C:\\users\\pc\\appdata\\roaming\\autodesk\\autocad 2020\\r23.1\\chs\\support\\simplex.shx")==0&&s.fontSHA=="30DC50FA379935E4D7EB6E60041FA070A11FBA6DEAC337A2FA9E34DCB5BFBCC1","simplex control differs from N9a identity");
    require(_wcsicmp(s.bigfont.c_str(),L"D:\\cad2020\\autocad 2020\\fonts\\gbcbig.shx")==0&&s.bigfontSHA=="B6D7F2C5E3057840A63433EE78A49D59FBD030C999D706F7368A14D52B2A4150","bigfont control differs from N9a identity");
    {auto ms=open<AcDbBlockTableRecord>(acdbSymUtil()->blockModelSpaceId(s.db.get()),AcDb::kForWrite);auto p=std::make_unique<AcDbPoint>(AcGePoint3d(75,50,0));p->setDatabaseDefaults(s.db.get());ok(ms->appendAcDbEntity(s.markerId,p.get()),"append owned marker");p.release()->close();}
    s.marker=open<AcDbPoint>(s.markerId);
    if(s.positiveControl)s.primitive=std::make_unique<PrimitiveRoot>(s);
    s.rootId=acdbSymUtil()->blockModelSpaceId(s.db.get());s.sceneRoot=open<AcDbBlockTableRecord>(s.rootId);require(s.sceneRoot->database()==s.db.get(),"owned scene root database mismatch");
    {AcDbBlockTableRecordIterator* raw{};ok(s.sceneRoot->newIterator(raw),"iterate owned scene root");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);unsigned roots{};for(;!it->done();it->step()){AcDbObjectId id;ok(it->getEntityId(id),"read owned root child");require(id==s.markerId,"unexpected owned modelspace child");++roots;}require(roots==1,"owned root must contain exact marker only");}
    s.log("ownedSceneRoot",{{"type",q("AcDbBlockTableRecord modelspace")},{"rootOldId",std::to_string(s.rootId.asOldId())},{"rootDatabase",address(s.sceneRoot->database())},{"markerOldId",std::to_string(s.markerId.asOldId())},{"children","1"}});
    if(s.positiveControl){AcGiVisualStyle prepared;configureStyle(prepared);createOwnedStyle(s,prepared);
        s.log("ownedDatabase.explicitWritesComplete",{{"markerOldId",std::to_string(s.markerId.asOldId())},{"styleOldId",std::to_string(s.styleId.asOldId())},{"source16FieldsVerified","true"},{"modelCreated","false"},{"catalogPinStarted","false"},{"preparationOrderChangedFromV24","true"}});prepareOwnedResourceCache(s);}
    s.manager=acgsGetGsManager();require(s.manager!=nullptr,"GS manager unavailable");AcGsKernelDescriptor descriptor;descriptor.addRequirement(AcGsKernelDescriptor::k3DDrawing);
    s.log("acquireGraphicsKernel.before",{{"requirement",q("k3DDrawing")},{"capabilityAddress",address(AcGsKernelDescriptor::k3DDrawing)},{"descriptorRequires3D",descriptor.requires(AcGsKernelDescriptor::k3DDrawing)?"true":"false"}});
    s.kernel=AcGsManager::acquireGraphicsKernel(descriptor);AcGsKernelDescriptor twoD;twoD.addRequirement(AcGsKernelDescriptor::k2DDrawing);
    s.log("acquireGraphicsKernel.after",{{"kernel",address(s.kernel)},{"compatibleWith3D",s.kernel?(s.kernel->isCompatibleWith(descriptor)?"true":"false"):"null"},{"compatibleWith2D",s.kernel?(s.kernel->isCompatibleWith(twoD)?"true":"false"):"null"}});
    require(s.kernel!=nullptr,"k3DDrawing kernel unavailable");require(s.kernel->isCompatibleWith(descriptor),"acquired kernel incompatible with requested k3DDrawing");
    if(s.kernelModelControl){require(s.positiveControl&&s.valueStyleControl&&s.diagnosticOnly&&s.catalogReady,"generic model restricted to cache-ready value-style nonpersistent dev probe");
        s.modelCookie=reinterpret_cast<Adesk::IntDbId>(s.db.get());
        {std::lock_guard<std::mutex> lock(providerMutex);require(!modelProvider&&providerOrphanCalls==0&&!providerBoundaryFailure,"model provider already active or orphan callback observed");modelProvider=&s;s.providerRegistered=true;s.providerEverInstalled=true;}
        const AcGsGetInterfaceFunc get=&diagnosticGetInterface;const AcGsReleaseInterfaceFunc release=&diagnosticReleaseInterface;
        s.log("kernel.createModel.before",{{"renderType",q("kMain")},{"opaqueCookie",std::to_string(s.modelCookie)},{"cookieMeaning",q("owned database pointer numeric label only")},{"contextDatabaseBindingProven","false"},{"getFunction",q("diagnosticGetInterface")},{"releaseFunction",q("diagnosticReleaseInterface")},{"callbacksNonNull",get&&release?"true":"false"},{"providerSession",address(&s)},{"persistentAdapterImplemented",q("finite-public-owned-read-open-catalog")},{"cacheReady",s.catalogReady?"true":"false"},{"cacheReadOpenReferences",std::to_string(s.cacheOpened)},{"nullReturnHandlingGuaranteed","false"}});
        s.model=s.kernel->createModel(AcGsModel::kMain,s.modelCookie,get,release);s.log("kernel.createModel.after",{{"model",address(s.model)}});
    }else s.model=s.manager->createAutoCADModel(*s.kernel);
    require(s.model!=nullptr,"GS model unavailable");
    if(s.positiveControl){s.log("model.factory",{{"factory",q(modelFactory(s))},{"pairedDestroyer",q(modelDestroyer(s))}});observeModelDefaultStyle(s);}
    const auto* modelKernel=&s.model->graphicsKernel();s.log("model.graphicsKernel",{{"model",address(s.model)},{"actualKernel",address(modelKernel)},{"expectedKernel",address(s.kernel)},{"matches",modelKernel==s.kernel?"true":"false"}});require(modelKernel==s.kernel,"owned model kernel mismatch");
    s.expectedRoot=graphRoot(s);s.log("addModelReactor.before",{{"model",address(s.model)},{"observer",address(&s.rootObserver)},{"expectedRoot",address(s.expectedRoot)}});s.rootObserverAttached=true;s.kernel->addModelReactor(s.model,&s.rootObserver);s.log("addModelReactor.after");
    if(s.faultPoint==0)observeDisplayConfig(s);
    const auto dbKey=reinterpret_cast<Adesk::LongPtr>(s.db.get());s.priorModel=s.kernel->find(dbKey,AcGsModel::kMain);s.kernel->associate(dbKey,s.model,AcGsModel::kMain);s.associated=true;
    s.log("ownedModelBinding",{{"targetDatabase",address(s.db.get())},{"model",address(s.model)},{"priorModel",address(s.priorModel)},{"readBackModel",address(s.kernel->find(dbKey,AcGsModel::kMain))},{"factory",q(modelFactory(s))},{"route",q("model factory + kernel.associate; real context checked separately")}});
    require(s.kernel->find(dbKey,AcGsModel::kMain)==s.model,"GS model binding readback failed");
    if(s.faultPoint==1)throw std::runtime_error("injected-after-model-association");
    s.device=s.kernel->createOffScreenDevice();require(s.device!=nullptr,"kernel offscreen device unavailable");const auto* deviceKernel=&s.device->graphicsKernel();
    const auto originalRenderer=s.device->getDeviceRenderer();s.log("ownedDeviceFactory",{{"factory",q("kernel.createOffScreenDevice/deleteDevice")},{"device",address(s.device)},{"deviceKernel",address(deviceKernel)},{"expectedKernel",address(s.kernel)},{"rendererOriginal",std::to_string(originalRenderer)}});require(deviceKernel==s.kernel,"owned offscreen device kernel mismatch");
    require(s.device->setBackgroundColor({255,255,255,0}),"white background rejected");
    const auto bg=s.device->getBackgroundColor();require(bg.m_red==255&&bg.m_green==255&&bg.m_blue==255,"background readback not white");
    for(auto& c:s.controlPalette)c={0,0,0,0};s.controlPalette[0]={255,255,255,0};
    s.log("ownedDevice.setLogicalPalette.before",{{"palette",paletteObservation(s)}});s.logicalPaletteCallStarted=true;s.device->setLogicalPalette(s.controlPalette.data(),static_cast<int>(s.controlPalette.size()));s.logicalPaletteReturned=true;s.log("ownedDevice.setLogicalPalette.after",{{"palette",paletteObservation(s)}});
    if(s.physicalPaletteControl){s.log("ownedDevice.setPhysicalPalette.before",{{"palette",paletteObservation(s)}});s.physicalPaletteCallStarted=true;s.device->setPhysicalPalette(s.controlPalette.data(),static_cast<int>(s.controlPalette.size()));s.physicalPaletteReturned=true;s.log("ownedDevice.setPhysicalPalette.after",{{"palette",paletteObservation(s)}});}
    s.log("kernel.createView.before");s.view=s.kernel->createView(nullptr,false);s.log("kernel.createView.after",{{"view",address(s.view)}});require(s.view!=nullptr,"offscreen kernel view unavailable");
    const auto* viewKernel=&s.view->graphicsKernel();s.log("view.graphicsKernel",{{"view",address(s.view)},{"actualKernel",address(viewKernel)},{"expectedKernel",address(s.kernel)},{"matches",viewKernel==s.kernel?"true":"false"}});require(viewKernel==s.kernel,"owned view kernel mismatch");observeModels(s,"view-created");
    s.log("device.addView.before");s.viewAttached=s.device->add(s.view);s.log("device.addView.after",{{"added",s.viewAttached?"true":"false"}});require(s.viewAttached,"attach owned kernel view failed");
    observeModels(s,"device-view-attached");
    if(s.faultPoint==3)throw std::runtime_error("injected-after-view-attached-before-style-camera");
    s.view->setViewport(AcGePoint2d(0,0),AcGePoint2d(1,1));s.view->setViewportBorderVisibility(false);
    AcGiVisualStyle wireframe;configureStyle(wireframe);if(!s.positiveControl)createOwnedStyle(s,wireframe);
    s.device->onSize(1024,768);
    s.view->setEnableFrontClip(false);s.view->setEnableBackClip(false);
    s.view->setView({75,50,100},{75,50,0},AcGeVector3d::kYAxis,150,112.5,AcGsView::kParallel);
    require(s.view->add(graphRoot(s),s.model),"add controlled root/model before style ID setter failed");s.styleQualified=bindStyle(s,wireframe,"initial-after-controlled-root-model-added");
    observeModels(s,"initial-after-root-and-style-bind");require(!s.rootObservationFailed,"root notification observation failed");
    s.log("ownedDeviceSettings",{{"backgroundRGB",array({std::to_string(bg.m_red),std::to_string(bg.m_green),std::to_string(bg.m_blue)})},{"foregroundRGB",array({"0","0","0"})},{"lineweight",q("AcDb::kLnWt000 hairline")},{"frontClip",s.view->isFrontClipped()?"true":"false"},{"backClip",s.view->isBackClipped()?"true":"false"},{"border",s.view->isViewportBorderVisible()?"true":"false"}});
    if(s.positiveControl){s.diagnosticOnly=true;s.log("nonpersistentControl.begin",{{"diagnosticOnly","true"},{"qualification","false"},{"root",address(s.primitive.get())},{"modelFactory",q(modelFactory(s))},{"pairedModelDestroyer",q(modelDestroyer(s))},{"pointOverruleInstalled","false"},{"width","1024"},{"height","768"}});
        auto rgb=render(s,1024,768,-1,wireframe,true);const auto path=s.config.output+L"\\nonpersistent-calibration-1024.rgb";save(path,std::string(reinterpret_cast<const char*>(rgb.data()),rgb.size()));s.positiveRawPath=path;calibrate(s,rgb,1024,768,s.view->worldToDeviceMatrix());s.positiveMappingVerified=true;return;}
    s.rule=std::make_unique<Rule>(s);s.priorOverruling=AcRxOverrule::isOverruling();ok(AcRxOverrule::addOverrule(AcDbPoint::desc(),s.rule.get(),false),"add own overrule");s.ruleAttached=true;AcRxOverrule::setIsOverruling(true);
    if(!s.styleQualified)styleMismatchDiagnostic(s,wireframe);
    if(s.faultPoint==2)throw std::runtime_error("injected-after-overrule-install");
    for(int scale:{1,2}){const int w=1024*scale,h=768*scale;s.device->onSize(w,h);
        auto rgb=render(s,w,h,-1,wireframe);save(s.config.output+L"\\calibration-"+std::to_wstring(w)+L".rgb",std::string(reinterpret_cast<const char*>(rgb.data()),rgb.size()));
        const auto m=s.view->worldToDeviceMatrix();const auto mapping=calibrate(s,rgb,w,h,m);
        s.log("viewMatrices",{{"width",std::to_string(w)},{"viewing",matrix(s.view->viewingMatrix())},{"projection",matrix(s.view->projectionMatrix())},{"screen",matrix(s.view->screenMatrix())},{"worldToDevice",matrix(m)},{"worldToPixel",affine(mapping.affine)},{"fieldWidth",num(s.view->fieldWidth())},{"fieldHeight",num(s.view->fieldHeight())},{"perspective",s.view->isPerspective()?"true":"false"},{"border",s.view->isViewportBorderVisible()?"true":"false"}});
        for(std::size_t i=0;i<controls.size();++i){rgb=render(s,w,h,static_cast<int>(i),wireframe);const auto id=std::string(controls[i].id)+"-"+std::to_string(w);const auto path=s.config.output+L"\\"+std::wstring(id.begin(),id.end())+L".rgb";
            save(path,std::string(reinterpret_cast<const char*>(rgb.data()),rgb.size()));
            bce::TextRenderContext context;context.deviceIdentity="AutoCAD2020/k3DDrawing/default-device/XY-custom-invisible-faces-object-color-edges-v1/owned-offscreen/"+std::to_string(w)+"x"+std::to_string(h);context.frameIdentity=utf8(s.config.nonce)+"/"+id;context.fontIdentity=s.fontSHA+"/"+s.bigfontSHA;context.sourceIdentity="owned-AcDbPoint/"+std::to_string(s.markerId.asOldId())+"/"+controls[i].id;
            // Explicit control paper: view is 150 x 112.5 mm at 1:1. These are conversion values, not a printer capability claim.
            context.dpiX=w*25.4/150.0;context.dpiY=h*25.4/112.5;context.pixelBoundaryPhase={mapping.phase,mapping.phase};
            const auto result=bce::measureTextFootprint({rgb.data(),rgb.size(),static_cast<std::size_t>(w),static_cast<std::size_t>(h),static_cast<std::size_t>(w)*3},mapping.affine,context);
            require(result.canCommit(),result.issues.empty()?"footprint absent":result.issues.front().code+": "+result.issues.front().message);const auto& v=*result.value;
            std::vector<std::string> corners;for(const auto& p:v.world.corners)corners.push_back(point(p));
            const auto json=object({{"id",quote(id)},{"text",quote(std::wstring(controls[i].text))},{"rgbPath",quote(path)},{"rgbSHA256",quote(sha256(path))},{"width",std::to_string(w)},{"height",std::to_string(h)},{"rowStride",std::to_string(w*3)},{"inkPixels",std::to_string(v.inkPixels)},{"deviceCellBounds",bounds(v.deviceCellBounds)},{"worldCorners",array(corners)},{"worldAABB",bounds(v.world.bounds)},{"worldToPixel",affine(v.worldToPixel)},{"deviceIdentity",quote(context.deviceIdentity)},{"frameIdentity",quote(context.frameIdentity)},{"fontIdentity",quote(context.fontIdentity)},{"sourceIdentity",quote(context.sourceIdentity)},{"dpiX",num(context.dpiX)},{"dpiY",num(context.dpiY)},{"controlPaperMm",array({"150","112.5"})},{"pixelBoundaryPhase",point(context.pixelBoundaryPhase)},{"glyphComplete","false"},{"capabilityUpgradeAllowed","false"}});
            save(s.config.output+L"\\"+std::wstring(id.begin(),id.end())+L".json",json);s.frames.push_back(json);
        }
    }
}
void lifecycleControls(Session& primary){
    std::vector<std::string> results;
    for(int fault:{1,3,2}){
        lifecycleSessions.push_back(std::make_unique<Session>());auto& s=*lifecycleSessions.back();s.config=primary.config;s.faultPoint=fault;
        if(fault==1){s.positiveControl=true;s.valueStyleControl=true;s.kernelModelControl=true;s.diagnosticOnly=true;}
        s.config.output+=L"\\lifecycle-"+std::to_wstring(fault);require(CreateDirectoryW(s.config.output.c_str(),nullptr)!=FALSE,"create lifecycle evidence directory");
        s.trace=File(CreateFileW((s.config.output+L"\\gs-trace.jsonl").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));require(s.trace.valid(),"lifecycle trace unavailable");
        std::string observed;try{execute(s);}catch(const std::exception& e){observed=e.what();s.cleanupLog("execute.cppException.beforeCleanup",{{"message",quote(observed)}});}
        const bool cleaned=s.cleanup();const auto after=s.ambientInitialized?dbmod():0;const bool stable=s.ambientInitialized&&s.ambientSame()&&s.watch.untouched()&&s.dbmodBefore==after;
        const auto expected=fault==1?"injected-after-model-association":fault==3?"injected-after-view-attached-before-style-camera":"injected-after-overrule-install";
        const bool cacheProtected=fault!=1||(s.catalogStarted&&s.cacheOpened>0&&s.catalogPinsClosed&&s.cacheOpened==s.cacheClosed&&s.providerEverInstalled&&!s.providerRegistered&&!providerUnsupported(s));
        const bool passed=observed==expected&&cleaned&&s.resourcesReleased()&&stable&&!s.rootObservationFailed&&cacheProtected;
        const auto record=object({{"faultPoint",q(expected)},{"faultReached",observed==expected?"true":"false"},{"observed",quote(observed)},{"modelFactory",q(modelFactory(s))},{"provider",providerObservation(s)},{"cacheLifecycleProtected",cacheProtected?"true":"false"},{"diagnosticOnly",s.diagnosticOnly?"true":"false"},{"styleContractSatisfied",s.styleQualified?"true":"false"},{"cleanupComplete",cleaned&&s.resourcesReleased()?"true":"false"},{"ambientObserved",s.ambientInitialized?"true":"false"},{"ambientUnchanged",s.ambientInitialized?(stable?"true":"false"):"null"},{"ambientWriteEvents",s.ambientInitialized?s.watch.json():"null"},{"dbmodBefore",s.ambientInitialized?std::to_string(s.dbmodBefore):"null"},{"dbmodAfter",s.ambientInitialized?std::to_string(after):"null"},{"passed",passed?"true":"false"}});
        save(s.config.output+L"\\result.json",record);results.push_back(record);require(passed,"real GS resource lifecycle fault control failed: "+observed);
    }
    save(primary.config.output+L"\\lifecycle-controls.json",array(results));
}
bool nonpersistentControl(Session& primary,bool physicalPalette,std::vector<std::string>& results){
    diagnosticSessions.push_back(std::make_unique<Session>());auto& s=*diagnosticSessions.back();s.config=primary.config;s.positiveControl=true;s.valueStyleControl=true;s.physicalPaletteControl=physicalPalette;s.diagnosticOnly=true;s.config.output+=physicalPalette?L"\\nonpersistent-physical-palette-control":L"\\nonpersistent-control";
    require(CreateDirectoryW(s.config.output.c_str(),nullptr)!=FALSE,"create nonpersistent control output");s.trace=File(CreateFileW((s.config.output+L"\\gs-trace.jsonl").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));require(s.trace.valid(),"nonpersistent trace unavailable");
    s.log("physicalPaletteArm.begin",{{"arm",physicalPalette?q("B"):q("A")},{"requestedModelFactory",q("manager.createAutoCADModel")},{"pairedModelDestroyer",q("manager.destroyAutoCADModel")},{"physicalPaletteRequested",physicalPalette?"true":"false"},{"requestedTransport",q("direct-AcGiVisualStyle-value")},{"session",address(&s)},{"instanceIdentity",quote(s.config.output)},{"freshExclusiveSession","true"},{"freshViewRequested","true"},{"diagnosticOnly","true"},{"qualification","false"},{"addressReuseIsInstanceSharing","false"}});
    std::string failure;try{execute(s);}catch(const std::exception& e){failure=e.what();s.cleanupLog("nonpersistentControl.cppException",{{"message",quote(failure)}});}catch(...){failure="nonpersistent control unknown exception";}
    const bool freshViewCreated=s.view!=nullptr;
    const auto identities=object({{"database",address(s.db.get())},{"marker",address(s.marker.get())},{"modelspace",address(s.sceneRoot.get())},{"persistentSourceOldId",std::to_string(s.styleId.asOldId())},{"root",address(s.primitive.get())},{"kernel",address(s.kernel)},{"model",address(s.model)},{"device",address(s.device)},{"view",address(s.view)}});
    const bool cleaned=s.cleanup();const auto after=s.ambientInitialized?dbmod():0;const bool stable=s.ambientInitialized&&s.ambientSame()&&s.watch.untouched()&&s.dbmodBefore==after;
    if(providerUnsupported(s)){if(!failure.empty())failure+="; ";failure+="persistent-adapter-unavailable or provider observation failed";}
    const bool mayContinue=!providerUnsupported(s)&&cleaned&&s.resourcesReleased()&&stable&&!s.rootObservationFailed&&s.diagnosticIdentityVerified&&!s.diagnosticIdentityFailed&&s.callbackError.empty()&&(failure.empty()||s.zeroWorldDrawObserved||!s.positiveRawPath.empty());
    const auto blockReason=mayContinue?std::string():std::string("error=")+failure+"; cleanup="+(cleaned&&s.resourcesReleased()?"true":"false")+"; ambient="+(stable?"true":"false")+"; identityVerified="+(s.diagnosticIdentityVerified?"true":"false")+"; rootIdentityMismatch="+(s.diagnosticIdentityFailed?"true":"false")+"; rootObservationFailed="+(s.rootObservationFailed?"true":"false")+"; callbackError="+s.callbackError;
    s.executionFailure=blockReason;
    if(failure.empty()&&!mayContinue)failure="diagnostic protection failed: "+blockReason;
    const auto result=object({{"arm",physicalPalette?q("B"):q("A")},{"palette",paletteObservation(s)},{"executed","true"},{"requestedTransport",q("direct-AcGiVisualStyle-value")},{"instanceIdentity",quote(s.config.output)},{"session",address(&s)},{"freshExclusiveSession","true"},{"freshViewCreated",freshViewCreated?"true":"false"},{"modelFactory",q(modelFactory(s))},{"pairedModelDestroyer",q(modelDestroyer(s))},{"opaqueCookie",s.kernelModelControl?std::to_string(s.modelCookie):"null"},{"cookieIsDatabaseBindingEvidence","false"},{"provider",providerObservation(s)},{"modelDefaultStyle",s.modelDefaultStyle.empty()?"null":s.modelDefaultStyle},{"updates",array(s.updateObservations)},{"resourcesBeforeCleanup",identities},{"styleBindings",array(s.styleBindings)},{"styleObservations",array(s.styleObservations)},{"rootNotificationsAdded",std::to_string(s.rootsAdded)},{"rootNotificationsErased",std::to_string(s.rootsErased)},{"rootObservationFailed",s.rootObservationFailed?"true":"false"},{"identityVerifiedBeforeNonemptyUpdate",s.diagnosticIdentityVerified?"true":"false"},{"rootNotificationIdentityMismatch",s.diagnosticIdentityFailed?"true":"false"},{"callbackError",quote(s.callbackError)},{"nextArmAllowed",mayContinue?"true":"false"},{"continuationBlockReason",quote(blockReason)},{"tracePath",quote(s.config.output+L"\\gs-trace.jsonl")},{"diagnosticOnly","true"},{"qualification","false"},{"originalMainFailure",quote(primary.executionFailure)},{"status",failure.empty()?q("diagnostic-buffer-observed"):q("diagnostic-failed")},{"error",quote(failure)},{"worldDrawCalls",std::to_string(s.callbacks)},{"attributeCalls",std::to_string(s.attributeCalls)},{"viewportDrawCalls",std::to_string(s.viewportCalls)},{"rawRGBPath",s.positiveRawPath.empty()?"null":quote(s.positiveRawPath)},{"rawRGBSHA256",s.positiveRawPath.empty()?"null":quote(sha256(s.positiveRawPath))},{"mappingVerified",s.positiveMappingVerified?"true":"false"},{"sideDbSuccessClaimed","false"},{"textRequested","false"},{"cleanupComplete",cleaned&&s.resourcesReleased()?"true":"false"},{"ambientObserved",s.ambientInitialized?"true":"false"},{"ambientUnchanged",s.ambientInitialized?(stable?"true":"false"):"null"},{"ambientWriteEvents",s.ambientInitialized?s.watch.json():"null"},{"dbmodBefore",s.ambientInitialized?std::to_string(s.dbmodBefore):"null"},{"dbmodAfter",s.ambientInitialized?std::to_string(after):"null"}});
    save(s.config.output+L"\\result.json",result);results.push_back(result);
    primary.log("nonpersistentControl.complete",{{"diagnosticOnly","true"},{"qualification","false"},{"worldDrawCalls",std::to_string(s.callbacks)},{"rawRGBObserved",s.positiveRawPath.empty()?"false":"true"},{"cleanupComplete",cleaned&&s.resourcesReleased()?"true":"false"},{"ambientUnchanged",stable?"true":"false"},{"rootObservationFailed",s.rootObservationFailed?"true":"false"}});return mayContinue;
}
bool physicalPaletteComparison(Session& primary){
    std::vector<std::string> results;bool protectedArms{};std::string error;
    try{protectedArms=nonpersistentControl(primary,false,results);
        if(protectedArms)protectedArms=nonpersistentControl(primary,true,results);
        else results.push_back(object({{"arm",q("B")},{"executed","false"},{"requestedModelFactory",q("manager.createAutoCADModel")},{"pairedModelDestroyer",q("manager.destroyAutoCADModel")},{"physicalPaletteRequested","true"},{"physicalPaletteReturned","false"},{"requestedTransport",q("direct-AcGiVisualStyle-value")},{"reason",quote(diagnosticSessions.back()->executionFailure)},{"diagnosticOnly","true"},{"qualification","false"}}));
    }catch(const std::exception& e){protectedArms=false;error=e.what();}catch(...){protectedArms=false;error="physical palette comparison unknown exception";}
    while(results.size()<2){const auto i=results.size();const bool attempted=diagnosticSessions.size()>i;const bool executed=attempted&&diagnosticSessions[i]->diagnosticExecutionStarted;results.push_back(object({{"arm",i==0?q("A"):q("B")},{"attempted",attempted?"true":"false"},{"executed",executed?"true":"false"},{"requestedTransport",q("direct-AcGiVisualStyle-value")},{"requestedModelFactory",q("manager.createAutoCADModel")},{"pairedModelDestroyer",q("manager.destroyAutoCADModel")},{"physicalPaletteRequested",i==0?"false":"true"},{"physicalPaletteReturned",attempted&&diagnosticSessions[i]->physicalPaletteReturned?"true":"false"},{"status",q("arm-evidence-incomplete-or-skipped")},{"error",quote(error)},{"reason",attempted?q("arm setup, cleanup or evidence operation raised; comparison protection failed"):q("preceding arm raised before safe continuation")},{"diagnosticOnly","true"},{"qualification","false"}}));}
    save(primary.config.output+L"\\physical-palette-comparison.json",object({{"contract",q("bce.n9i-physical-palette.v1")},{"diagnosticOnly","true"},{"qualification","false"},{"originalMainFailure",quote(primary.executionFailure)},{"error",quote(error)},{"protectionVerified",protectedArms?"true":"false"},{"freshSessionsSequential","true"},{"addressReuseIsInstanceSharing","false"},{"commonSetup",q("all explicit owned DB writes and source verification before public catalog read-open cache before model/view")},{"commonPaletteInput",q("same original 256 control values initialized identically in each stable Session member; lifetime extended through device deletion in both arms")},{"soleRenderingConfigurationDifference",q("B additionally calls setPhysicalPalette with the exact array passed to setLogicalPalette, after logical and before any view creation")},{"physicalReadbackAvailable","false"},{"physicalAdoptionClaimed","false"},{"knownRootCauseClaimed","false"},{"arms",array(results)}}));return protectedArms;
}
void runGsCore()noexcept{
    if(running||completed)return;running=true;
    try{
        session=std::make_unique<Session>();auto& s=*session;s.config=load_config(moduleHandle);s.evidence=std::make_unique<Evidence>(s.config);validate_identity(s.config);require(s.config.caseName==L"NativeGsTextFootprint","case mismatch");
        s.trace=File(CreateFileW((s.config.output+L"\\gs-trace.jsonl").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));require(s.trace.valid(),"trace unavailable");
        std::string failure;try{lifecycleControls(s);execute(s);}catch(const std::exception& e){failure=e.what();s.cleanupLog("execute.cppException.beforeCleanup",{{"message",quote(failure)}});}catch(...){failure="unknown native execution exception";s.cleanupLog("execute.unknownException.beforeCleanup");}
        s.executionFailure=failure;
        const bool primaryCleaned=s.cleanup();if(s.rootObservationFailed&&failure.empty())failure="root notification observation failed during cleanup";bool cleaned=primaryCleaned&&s.resourcesReleased()&&std::all_of(lifecycleSessions.begin(),lifecycleSessions.end(),[](const auto& control){return control->resourcesReleased();});const bool ambientSame=s.ambientInitialized&&s.ambientSame();const auto dbmodAfter=s.ambientInitialized?dbmod():0;
        const bool ambientStable=s.ambientInitialized&&ambientSame&&s.watch.untouched()&&s.dbmodBefore==dbmodAfter;bool diagnosticProtected=true;
        if(s.zeroWorldDrawObserved&&s.callbacks==0&&s.frames.empty()&&cleaned&&ambientStable){try{diagnosticProtected=physicalPaletteComparison(s);}catch(const std::exception& e){diagnosticProtected=false;s.cleanupLog("nonpersistentControl.failed",{{"message",quote(std::string(e.what()))}});}catch(...){diagnosticProtected=false;s.cleanupLog("nonpersistentControl.failedUnknown");}}
        cleaned=cleaned&&std::all_of(diagnosticSessions.begin(),diagnosticSessions.end(),[](const auto& diagnostic){return diagnostic->resourcesReleased();});const bool passed=failure.empty()&&cleaned&&ambientStable&&diagnosticProtected;
        s.log("ambient.after",{{"observed",s.ambientInitialized?"true":"false"},{"observation",s.ambientInitialized?q("observed"):q("not-observed-main-not-entered")},{"current",s.ambientInitialized?address(acDocManager->curDocument()):"null"},{"active",s.ambientInitialized?address(acDocManager->mdiActiveDocument()):"null"},{"workingDatabase",s.ambientInitialized?address(acdbHostApplicationServices()->workingDatabase()):"null"},{"dbmod",s.ambientInitialized?std::to_string(dbmodAfter):"null"},{"writeEvents",s.ambientInitialized?s.watch.json():"null"}});
        save(s.config.output+L"\\gs-result.json",object({{"status",passed?q("native-buffer-complete"):q("failed")},{"qualification",passed?"true":"false"},{"error",quote(failure)},{"cleanupComplete",cleaned?"true":"false"},{"nonpersistentControlAttempted",diagnosticSessions.empty()?"false":"true"},{"nonpersistentControlProtectionVerified",diagnosticSessions.empty()?"null":(diagnosticProtected?"true":"false")},{"ambientObserved",s.ambientInitialized?"true":"false"},{"ambientObservation",s.ambientInitialized?q("observed"):q("not-observed-main-not-entered")},{"ambientPointersUnchanged",s.ambientInitialized?(ambientSame?"true":"false"):"null"},{"ambientDBMODBefore",s.ambientInitialized?std::to_string(s.dbmodBefore):"null"},{"ambientDBMODAfter",s.ambientInitialized?std::to_string(dbmodAfter):"null"},{"ambientWriteEvents",s.ambientInitialized?s.watch.json():"null"},{"frames",array(s.frames)},{"glyphComplete","false"},{"capabilityUpgradeAllowed","false"},{"layoutIntegrated","false"}}));
        if(!failure.empty())s.evidence->error(failure);
        if(!cleaned)s.evidence->error("cleanup incomplete; module must remain loaded");
        if(!diagnosticProtected)s.evidence->error("nonpersistent diagnostic protection or evidence failed");
        if(s.ambientInitialized&&!ambientStable)s.evidence->error("observed ambient identities, DBMOD or write events changed");
        if(!s.ambientInitialized&&failure.empty())s.evidence->error("main ambient observation unavailable");
        if(passed){s.evidence->step("nativeGSBuffer","passed","two independent resolutions, five controls each; same TextOccupancy.cpp; independent nontext calibration");s.evidence->capability(object({{"finiteNativeBuffer","true"},{"glyphComplete","false"},{"capabilityUpgradeAllowed","false"}}));}
        s.evidence->finish(s.config,passed?"passed":"failed");s.evidence.reset();completed=true;
    }catch(const std::exception& e){if(session&&session->evidence){auto terminal=std::move(session->evidence);try{if(!session->executionFailure.empty())terminal->error(session->executionFailure);terminal->error(e.what());terminal->finish(session->config,"failed");}catch(...){}}completed=true;}
    catch(...){if(session&&session->evidence){auto terminal=std::move(session->evidence);try{if(!session->executionFailure.empty())terminal->error(session->executionFailure);terminal->error("unknown native command exception");terminal->finish(session->config,"failed");}catch(...){}}completed=true;}
    running=false;
}
struct CommandState{
    AcApDocument* current{};AcApDocument* active{};AcDbDatabase* currentDb{};AcDbDatabase* activeDb{};AcDbDatabase* working{};
    DWORD pid{},thread{};int cmdActive{},inputPending{};std::wstring cmdNames;bool quiescent{},applicationContext{};
};
CommandState commandState(){
    require(acDocManager!=nullptr,"dispatch document manager unavailable");CommandState s;s.pid=GetCurrentProcessId();s.thread=GetCurrentThreadId();s.current=acDocManager->curDocument();s.active=acDocManager->mdiActiveDocument();s.currentDb=s.current?s.current->database():nullptr;s.activeDb=s.active?s.active->database():nullptr;s.working=acdbHostApplicationServices()->workingDatabase();s.applicationContext=acDocManager->isApplicationContext();
    resbuf active{},names{};struct FreeString{resbuf& value;~FreeString(){if(value.restype==RTSTR&&value.resval.rstring)acutDelString(value.resval.rstring);}} freeNames{names};
    require(acedGetVar(L"CMDACTIVE",&active)==RTNORM&&active.restype==RTSHORT,"dispatch CMDACTIVE observation failed");s.cmdActive=active.resval.rint;
    require(acedGetVar(L"CMDNAMES",&names)==RTNORM&&names.restype==RTSTR&&names.resval.rstring,"dispatch CMDNAMES observation failed");s.cmdNames=names.resval.rstring;s.inputPending=s.current?acDocManager->inputPending(s.current):-1;s.quiescent=s.current&&s.current->isQuiescent();return s;
}
std::string commandJson(const CommandState& s){return object({{"pid",std::to_string(s.pid)},{"thread",std::to_string(s.thread)},{"cmdActive",std::to_string(s.cmdActive)},{"scriptBitSet",(s.cmdActive&4)?"true":"false"},{"cmdNames",quote(s.cmdNames)},{"inputPending",std::to_string(s.inputPending)},{"quiescent",s.quiescent?"true":"false"},{"applicationContext",s.applicationContext?"true":"false"},{"current",address(s.current)},{"active",address(s.active)},{"currentDatabase",address(s.currentDb)},{"activeDatabase",address(s.activeDb)},{"workingDatabase",address(s.working)}});}
bool sameConfig(const Config& a,const Config& b){
    auto identity=[](const Config& c){return std::tie(c.nonce,c.session,c.scheme,c.reference,c.output,c.caseName,c.modulePath,c.iniPath,c.desktop,c.expectedModuleSHA256,c.moduleSHA256,c.iniSHA256,c.schemeSHA256,c.referenceSHA256);};return identity(a)==identity(b);
}
struct Dispatch{
    Config config;CommandState entry;File trace,state;bool hookAttached{},idleReady{},queued{},queueAccepted{},captureStarted{},captureFinished{},failed{};unsigned entries{},captures{};int queueStatus{-1};std::string phase="waiting-idle",error;
    bool sameHost(const CommandState& s)const{return s.pid==entry.pid&&s.thread==entry.thread&&s.current==entry.current&&s.active==entry.active&&s.currentDb==entry.currentDb&&s.activeDb==entry.activeDb&&s.working==entry.working;}
    void record(const char* event,const CommandState* observed=nullptr){
        const auto json=object({{"contract",q("bce.native-gs-dispatch.v1")},{"event",q(event)},{"phase",quote(phase)},{"nonce",quote(config.nonce)},{"tickMs",std::to_string(GetTickCount64())},{"entryCalls",std::to_string(entries)},{"captureCalls",std::to_string(captures)},{"hookAttached",hookAttached?"true":"false"},{"idleReady",idleReady?"true":"false"},{"queued",queued?"true":"false"},{"queueAccepted",queueAccepted?"true":"false"},{"queueStatus",std::to_string(queueStatus)},{"captureStarted",captureStarted?"true":"false"},{"captureFinished",captureFinished?"true":"false"},{"gsCoreCompleted",completed?"true":"false"},{"error",quote(error)},{"observed",observed?commandJson(*observed):"null"}});
        write_flushed(trace.get(),json+"\n");replace_owned_report(state.get(),json);
    }
};
std::unique_ptr<Dispatch> dispatch;
void dispatchIdle()noexcept;
bool removeDispatchIdle(Dispatch& d)noexcept{
    if(!d.hookAttached)return true;try{const bool removed=acedRemoveOnIdleWinMsg(dispatchIdle);if(removed)d.hookAttached=false;
        try{d.record(removed?"idle-hook-removed":"idle-hook-removal-failed");}catch(...){}return removed;
    }catch(...){try{d.record("idle-hook-removal-exception");}catch(...){}return false;}
}
void dispatchFailure(const char* message)noexcept{
    if(!dispatch)return;auto& d=*dispatch;d.failed=true;try{d.phase="dispatch-failed";d.error=message;d.record("dispatch-failed");}catch(...){}removeDispatchIdle(d);
}
void dispatchIdle()noexcept{
    if(!dispatch)return;auto& d=*dispatch;if(d.failed||d.queued||d.captureStarted||d.captureFinished)return;
    try{
        const auto observed=commandState();require(d.sameHost(observed),"idle host/thread/document/database identity changed");require(observed.inputPending>=0,"idle inputPending observation failed");
        const bool ready=observed.cmdActive==0&&observed.cmdNames.empty()&&observed.quiescent&&observed.inputPending==0;
        if(!ready)return;
        d.idleReady=true;d.record("idle-ready",&observed);d.queued=true;d.phase="queue-requested";d.record("capture-queue-before",&observed);
        const auto status=acDocManager->sendStringToExecute(d.entry.current,L"BCE_GS_CAPTURE\n",false,false,false);d.queueStatus=static_cast<int>(status);d.queueAccepted=status==Acad::eOk;d.phase=d.queueAccepted?"queued":"queue-failed";d.record("capture-queue-after");require(d.queueAccepted,"capture command queue rejected; no GS execution");
    }catch(const std::exception& e){dispatchFailure(e.what());}catch(...){dispatchFailure("unknown idle callback exception");}
}
void prepareDispatch()noexcept{
    try{
        auto config=load_config(moduleHandle);validate_identity(config);require(config.caseName==L"NativeGsTextFootprint","dispatch case mismatch");const auto observed=commandState();
        if(dispatch){auto& d=*dispatch;++d.entries;require(sameConfig(config,d.config)&&d.sameHost(observed)&&!d.failed,"dispatch replay identity/state differs");d.record("entry-exact-noop",&observed);return;}
        require(!running&&!completed,"GS core already active/completed without dispatch");require(observed.current&&observed.current==observed.active&&observed.currentDb==observed.working&&observed.activeDb==observed.working,"entry document/database identities inconsistent");
        dispatch=std::make_unique<Dispatch>();auto& d=*dispatch;d.config=std::move(config);d.entry=observed;d.entries=1;
        d.trace=File(CreateFileW((d.config.output+L"\\dispatch-trace.jsonl").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));require(d.trace.valid(),"dispatch trace unavailable");
        d.state=File(CreateFileW((d.config.output+L"\\dispatch-state.json").c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));require(d.state.valid(),"dispatch state unavailable");d.record("entry",&observed);
        d.hookAttached=acedRegisterOnIdleWinMsg(dispatchIdle);d.record("idle-hook-register");require(d.hookAttached,"idle hook registration failed; no GS execution");
    }catch(const std::exception& e){dispatchFailure(e.what());}catch(...){dispatchFailure("unknown dispatch entry exception");}
}
void captureDispatch()noexcept{
    try{
        require(dispatch!=nullptr,"capture invoked without prepared dispatch");auto& d=*dispatch;auto config=load_config(moduleHandle);validate_identity(config);const auto observed=commandState();
        require(sameConfig(config,d.config)&&d.sameHost(observed),"capture nonce/host/thread/document/database identity differs");require(d.idleReady&&d.queued&&d.queueAccepted&&!d.failed&&!d.captureStarted&&!d.captureFinished&&!running&&!completed,"capture is not the single queued command");
        ++d.captures;d.captureStarted=true;d.phase="capture-guard";d.record("capture",&observed);require((observed.cmdActive&4)==0&&observed.inputPending==0&&!observed.applicationContext,"capture must be after SCRIPT, in document command context, without extra pending input");require(removeDispatchIdle(d),"capture cannot remove idle hook; no GS execution");
        d.phase="capture-running";d.record("gs-core-before");runGsCore();d.captureFinished=true;d.phase="capture-finished";d.record("gs-core-after");
    }catch(const std::exception& e){dispatchFailure(e.what());}catch(...){dispatchFailure("unknown capture command exception");}
}
}
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){moduleHandle=h;DisableThreadLibraryCalls(h);}return TRUE;}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message,void* id){try{
    if(message==AcRx::kInitAppMsg){acrxDynamicLinker->unlockApplication(id);acrxRegisterAppMDIAware(id);ok(acedRegCmds->addCommand(L"BCE_NATIVE_GS_TEXT",L"BCE_TEST_RUN",L"BCE_TEST_RUN",ACRX_CMD_MODAL,prepareDispatch),"register deferred entry");ok(acedRegCmds->addCommand(L"BCE_NATIVE_GS_TEXT",L"BCE_GS_CAPTURE",L"BCE_GS_CAPTURE",ACRX_CMD_MODAL,captureDispatch),"register capture command");}
    if(message==AcRx::kUnloadAppMsg){if(providerBlocksUnload()||running||(dispatch&&(dispatch->hookAttached||(dispatch->queued&&!dispatch->captureFinished)||(dispatch->captureStarted&&!dispatch->captureFinished))))return AcRx::kRetError;for(const auto& s:lifecycleSessions)if(!s->cleanup())return AcRx::kRetError;for(const auto& s:diagnosticSessions)if(!s->cleanup())return AcRx::kRetError;if(session&&!session->cleanup())return AcRx::kRetError;lifecycleSessions.clear();diagnosticSessions.clear();session.reset();dispatch.reset();ok(acedRegCmds->removeGroup(L"BCE_NATIVE_GS_TEXT"),"remove commands");}
    return AcRx::kRetOK;
}catch(...){return AcRx::kRetError;}}
