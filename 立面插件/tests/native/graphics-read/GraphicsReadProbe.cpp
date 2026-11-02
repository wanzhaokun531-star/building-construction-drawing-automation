#include "GiCapture.h"
#include <aced.h>
#include <acutads.h>
#include <rxregsvc.h>
#include <dbents.h>
#include <dbmtext.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <dbidmap.h>
#include <dbapserv.h>
#include <dbproxy.h>
#include <memory>
#include <exception>
namespace {
using namespace bce::native;
using namespace bce::graphics;
HMODULE moduleHandle{};
void ok(Acad::ErrorStatus status,const char* operation){require(status==Acad::eOk,std::string(operation)+" status="+std::to_string(status));}
template<class T>class Open final {
public:
    explicit Open(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead){ok(acdbOpenObject(p_,id,mode),"open");}
    ~Open(){if(p_)p_->close();}
    T* operator->() const{return p_;} T& operator*() const{return *p_;}
    Open(const Open&)=delete;Open& operator=(const Open&)=delete;
private:T* p_{};
};
struct Monitor final:AcDbDatabaseReactor {
    AcDbDatabase& db;std::vector<std::string> events;bool loggingFailed{};std::string role,phase{"monitor-attached"};
    std::function<void(const std::string&)> log;
    explicit Monitor(AcDbDatabase& d,const char* dbRole,std::function<void(const std::string&)> sink):db(d),role(dbRole),log(std::move(sink)){ok(db.addReactor(this),"attach source monitor");}
    ~Monitor(){db.removeReactor(this);}
    void begin(const std::string& operation){check();phase=operation;log(bce::native::object({{"kind",quote(std::string("protectedDbPhaseBegin"))},{"dbRole",quote(role)},{"phase",quote(phase)},{"workingDatabaseIsMonitored",acdbHostApplicationServices()->workingDatabase()==&db?"true":"false"}}));}
    void record(const AcDbDatabase* eventDb,const char* kind,const AcDbObject* object) noexcept {try{events.push_back(bce::native::object({{"kind",quote(std::string("protectedDbEvent"))},{"dbRole",quote(role)},{"event",quote(std::string(kind))},{"phase",quote(phase)},
        {"eventDatabaseMatchesMonitor",eventDb==&db?"true":"false"},{"eventObjectOwnedByMonitor",object&&object->database()==&db?"true":"false"},
        {"workingDatabaseIsMonitored",acdbHostApplicationServices()->workingDatabase()==&db?"true":"false"},
        {"handle",quote(object?handle(object->objectId()):std::string{})},{"class",quote(object?std::wstring(object->isA()->name()):std::wstring{})}}));log(events.back());}catch(...){loggingFailed=true;}}
    void objectModified(const AcDbDatabase* d,const AcDbObject* p) override{record(d,"modified",p);}
    void objectOpenedForModify(const AcDbDatabase* d,const AcDbObject* p) override{record(d,"openedForModify",p);}
    void objectAppended(const AcDbDatabase* d,const AcDbObject* p) override{record(d,"appended",p);}
    void objectErased(const AcDbDatabase* d,const AcDbObject* p,Adesk::Boolean) override{record(d,"erased",p);}
    void headerSysVarChanged(const AcDbDatabase* d,const ACHAR*,Adesk::Boolean success) override{if(success)record(d,"header",nullptr);}
    void check() const {
        log(bce::native::object({{"kind",quote(std::string("protectedDbGuard"))},{"dbRole",quote(role)},{"phase",quote(phase)},{"eventCount",std::to_string(events.size())},{"loggingFailed",loggingFailed?"true":"false"}}));
        require(events.empty()&&!loggingFailed,"protected database changed: role="+role+" phase="+phase+" events="+std::to_string(events.size())+" loggingFailed="+(loggingFailed?"true":"false")+(events.empty()?"":" first="+events.front()));
    }
};
class WorkingDatabase final {
public:
    explicit WorkingDatabase(AcDbDatabase& target):old_(acdbHostApplicationServices()->workingDatabase()) {acdbHostApplicationServices()->setWorkingDatabase(&target);}
    ~WorkingDatabase() noexcept {acdbHostApplicationServices()->setWorkingDatabase(old_);}
    WorkingDatabase(const WorkingDatabase&)=delete;WorkingDatabase& operator=(const WorkingDatabase&)=delete;
private:AcDbDatabase* old_;
};
// Only public identity/render traits: intentionally no source extents, explode, draw or font loads.
std::string fingerprint(AcDbDatabase& db){
    Open<AcDbBlockTableRecord> ms(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};
    ok(ms->newIterator(raw),"source iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);std::vector<std::string> roots;
    for(;!it->done();it->step()){AcDbObjectId id;ok(it->getEntityId(id),"source id");Open<AcDbEntity> e(id);
        roots.push_back(object({{"handle",quote(handle(id))},{"class",quote(std::wstring(e->isA()->name()))},
          {"owner",quote(handle(e->ownerId()))},{"layer",quote(handle(e->layerId()))},
          {"color",std::to_string(e->colorIndex())},{"linetype",quote(handle(e->linetypeId()))},
          {"visible",std::to_string(e->visibility())}}));}
    return array(roots);
}
AcDbObjectId append(AcDbObjectId owner,std::unique_ptr<AcDbEntity> entity){
    Open<AcDbBlockTableRecord> ms(owner,AcDb::kForWrite);AcDbObjectId id;ok(ms->appendAcDbEntity(id,entity.get()),"append control");entity.release()->close();return id;
}
AcDbObjectId newBlock(AcDbDatabase& db,const wchar_t* name){
    Open<AcDbBlockTable> table(db.blockTableId(),AcDb::kForWrite);auto block=std::make_unique<AcDbBlockTableRecord>();
    ok(block->setName(name),"control block name");AcDbObjectId id;ok(table->add(id,block.get()),"control block");block.release()->close();return id;
}
std::vector<AcDbObjectId> controls(AcDbDatabase& db){
    const auto ms=acdbSymUtil()->blockModelSpaceId(&db);std::vector<AcDbObjectId> ids;
    AcDbObjectId styleId;
    {Open<AcDbTextStyleTable> table(db.textStyleTableId(),AcDb::kForWrite);auto style=std::make_unique<AcDbTextStyleTableRecord>();
     ok(style->setName(L"N5_SIMPLEX_GBCBIG"),"style name");ok(style->setFileName(L"simplex.shx"),"font");ok(style->setBigFontFileName(L"gbcbig.shx"),"bigfont");
     ok(table->add(styleId,style.get()),"style append");style.release()->close();}
    ids.push_back(append(ms,std::make_unique<AcDbLine>(AcGePoint3d(1,2,0),AcGePoint3d(4,6,0))));
    ids.push_back(append(ms,std::make_unique<AcDbArc>(AcGePoint3d(10,10,0),5,0,1.5707963267948966)));
    auto numeric=std::make_unique<AcDbText>(AcGePoint3d(0,0,0),L"123.45",styleId,3);
    ok(numeric->setWidthFactor(0.75),"numeric control width");ids.push_back(append(ms,std::move(numeric)));
    ids.push_back(append(ms,std::make_unique<AcDbText>(AcGePoint3d(0,5,0),L"中文立面",styleId,3)));
    auto rotated=std::make_unique<AcDbText>(AcGePoint3d(10,5,0),L"270",styleId,3);
    ok(rotated->setRotation(1.5707963267948966),"rotation");ok(rotated->mirrorInX(true),"mirror");ids.push_back(append(ms,std::move(rotated)));
    auto mtext=std::make_unique<AcDbMText>();ok(mtext->setTextStyle(styleId),"mtext style");ok(mtext->setTextHeight(3),"mtext height");
    mtext->setContents(L"AB\\P{\\H0.5x;2}\\S7/9;");ids.push_back(append(ms,std::move(mtext)));
    auto inner=newBlock(db,L"N5_INNER"),outer=newBlock(db,L"N5_OUTER");
    append(inner,std::make_unique<AcDbText>(AcGePoint3d(1,2,0),L"nested",styleId,2));
    auto ref=std::make_unique<AcDbBlockReference>(AcGePoint3d(10,20,0),inner);ok(ref->setRotation(0.5),"block rotation");ok(ref->setScaleFactors(AcGeScale3d(2,1,1)),"nonuniform scale");append(outer,std::move(ref));
    ids.push_back(append(ms,std::make_unique<AcDbBlockReference>(AcGePoint3d(30,40,0),outer)));return ids;
}
void run_case() noexcept {
    Config config;std::unique_ptr<Evidence> evidence;File graphicsReport;std::vector<std::string> records;
    AcDbDatabase* entryWorkingDatabase{};
    try{
        config=load_config(moduleHandle);evidence=std::make_unique<Evidence>(config);evidence->checkpoint("identity");validate_identity(config);
        require(config.caseName==L"PublicGraphicsRead","unsupported case");
        graphicsReport=create_new(config.output+L"\\graphics-read.json");auto& report=graphicsReport;
        auto callbacks=create_new(config.output+L"\\graphics-callbacks.jsonl");
        replace_owned_report(report.get(),object({{"status",quote(std::string("incomplete"))}}));
        auto log=[&](const std::string& line){write_flushed(callbacks.get(),line+"\n");};
        auto* ambient=acdbHostApplicationServices()->workingDatabase();require(ambient,"ambient db missing");entryWorkingDatabase=ambient;Monitor ambientMonitor(*ambient,"ambient",log);
        ambientMonitor.begin("reference-empty-db-constructor");evidence->checkpoint("before-reference-empty-db-constructor");
        AcDbDatabase source(false,true);ambientMonitor.check();
        ambientMonitor.begin("reference-workingDatabase-enter");
        {
            // readDwgFile also consults the working database while loading
            // resources. Keep the new SideDB current until closeInput finishes,
            // then restore before any source snapshot or possible destruction.
            WorkingDatabase referenceWorking(source);
            require(acdbHostApplicationServices()->workingDatabase()==&source,"reference working database not installed");ambientMonitor.check();
            ambientMonitor.begin("reference-readDwgFile");evidence->checkpoint("before-reference-read");
            ok(source.readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false,L""),"reference read");ambientMonitor.check();
            ambientMonitor.begin("reference-closeInput");evidence->checkpoint("before-reference-closeInput");
            ok(source.closeInput(true),"source closeInput");ambientMonitor.check();
            ambientMonitor.begin("reference-workingDatabase-restore");
        }
        require(acdbHostApplicationServices()->workingDatabase()==ambient,"reference working database restore failed");ambientMonitor.check();
        log(object({{"kind",quote(std::string("referenceReadWorkingDatabaseScopeRestored"))},{"restored","true"}}));
        Monitor sourceMonitor(source,"reference",log);
        auto begin=[&](const std::string& operation){ambientMonitor.begin(operation);sourceMonitor.begin(operation);evidence->checkpoint(operation);};
        auto guarded=[&](){sourceMonitor.check();ambientMonitor.check();};
        begin("cold-source-fingerprint");const auto before=fingerprint(source);guarded();
        bool controlsComplete=true,controlsValuesMatch=true;
        {
            begin("standard-target-db-constructor");AcDbDatabase target(true,true);guarded();
            begin("standard-target-workingDatabase-enter");WorkingDatabase working(target);guarded();
            begin("standard-protocol-controls");auto p=controlProtocol(target,log);guarded();
            records.push_back(object({{"control",quote(std::string("explicit-length-recursion-matrix-viewport"))},{"result",p.json()}}));
            for(bool nested:{false,true}){begin(nested?"viewport-protocol-child":"viewport-protocol-root");const auto viewport=controlViewportProtocol(target,log,nested);guarded();
                records.push_back(object({{"control",quote(std::string(nested?"viewport-only-attribute-child":"viewport-only-attribute-root"))},{"protocolAssertionsPassed","true"},{"result",viewport.json()}}));}
            begin("create-standard-control-entities");const auto ids=controls(target);guarded();
            for(std::size_t i=0;i<ids.size();++i){begin("standard-control-"+std::to_string(i));Open<AcDbEntity> entity(ids[i]);
                const auto a=capture(*entity,target,log),second=capture(*entity,target,log);
                guarded();
                require(a.callbacks==second.callbacks,"standard repeated callbacks unstable");
                const auto validation=validateControl(i,a.observation);
                controlsComplete=controlsComplete&&validation.complete;controlsValuesMatch=controlsValuesMatch&&validation.valuesMatch;
                std::vector<std::string> reasons;for(const auto& reason:validation.reasons)reasons.push_back(quote(reason));
                records.push_back(object({{"controlIndex",std::to_string(i)},{"class",quote(std::wstring(entity->isA()->name()))},{"stable","true"},
                    {"fixtureValuesMatch",validation.valuesMatch?"true":"false"},{"controlComplete",validation.complete?"true":"false"},
                    {"controlStatus",quote(std::string(validation.complete?"passed":"partial"))},{"controlReasons",array(reasons)},{"result",a.json()}}));
                evidence->step("actualStandardControl-"+std::to_string(i),validation.complete?"passed":"partial",array(reasons));
                replace_owned_report(report.get(),object({{"status",quote(std::string("incomplete"))},{"standardControlsComplete",controlsComplete?"true":"false"},{"records",array(records)}}));
            }
            begin("standard-target-scope-release");
        }
        guarded();
        for(const wchar_t* sourceHandle:{L"ACB0",L"10837",L"AC83",L"CAF3",L"11B93"}){
            begin("lookup-source-"+utf8(sourceHandle));
            AcDbObjectId sourceId;ok(source.getAcDbObjectId(sourceId,false,AcDbHandle(sourceHandle)),"source handle");
            const std::wstring expectedClass=std::wstring(sourceHandle)==L"11B93"?L"AcDbBlockReference":L"TDbDimension2";
            {Open<AcDbEntity> e(sourceId);require(std::wstring(e->isA()->name())==expectedClass,"unexpected fixture root class");}
            guarded();begin("clone-target-db-constructor-"+utf8(sourceHandle));AcDbDatabase target(true,true);guarded();
            AcDbObjectIdArray ids;ids.append(sourceId);AcDbIdMapping map;
            const auto owner=acdbSymUtil()->blockModelSpaceId(&target);begin("wblockCloneObjects-"+utf8(sourceHandle));ok(source.wblockCloneObjects(ids,owner,map,AcDb::kDrcMangleName),"clone root");
            guarded();AcDbIdPair pair;pair.setKey(sourceId);require(map.compute(pair)&&pair.isCloned(),"root mapping");
            std::vector<std::string> mappings;AcDbIdMappingIter iterator(map);
            for(;!iterator.done();iterator.next()){AcDbIdPair item;require(iterator.getMap(item),"mapping iterate");
                require(!item.value().isNull()&&item.value().database()==&target,"mapped dependency not target owned");
                mappings.push_back(object({{"source",quote(handle(item.key()))},{"target",quote(handle(item.value()))},{"cloned",item.isCloned()?"true":"false"}}));}
            {Open<AcDbEntity> e(pair.value());require(e->database()==&target&&e->ownerId()==owner&&std::wstring(e->isA()->name())==expectedClass&&!AcDbProxyEntity::cast(&*e),"clone class/owner/proxy");}
            Result first,second;
            begin("target-workingDatabase-enter-"+utf8(sourceHandle));
            {WorkingDatabase working(target);guarded();begin("target-draw-"+utf8(sourceHandle));Open<AcDbEntity> e(pair.value());first=capture(*e,target,log);guarded();begin("repeat-target-draw-"+utf8(sourceHandle));second=capture(*e,target,log);guarded();begin("target-workingDatabase-restore-"+utf8(sourceHandle));}
            guarded();require(first.callbacks==second.callbacks,"TCH callback instability");
            const auto dwg=config.output+L"\\clone-"+sourceHandle+L".dwg";
            require(GetFileAttributesW(dwg.c_str())==INVALID_FILE_ATTRIBUTES,"clone output already exists");
            begin("save-independent-natural-render-clone-"+utf8(sourceHandle));ok(target.saveAs(dwg.c_str()),"save clone");guarded();
            records.push_back(object({{"sourceHandle",quote(std::wstring(sourceHandle))},{"sourcePath",quote(config.reference)},
              {"targetHandle",quote(handle(pair.value()))},{"mapping",array(mappings)},{"stable","true"},
              {"standardControlGatePassed",controlsComplete?"true":"false"},
              {"capabilityUpgradeAllowed",controlsComplete&&first.coverage.requestComplete()?"true":"false"},
              {"cloneDwg",quote(dwg)},{"cloneSHA256",quote(sha256(dwg))},{"result",first.json()}}));
            replace_owned_report(report.get(),object({{"status",quote(std::string("incomplete"))},{"records",array(records)}}));
            begin("clone-target-scope-release-"+utf8(sourceHandle));
        }
        guarded();begin("final-source-fingerprint");require(fingerprint(source)==before,"source public identity/traits changed");guarded();
        require(acdbHostApplicationServices()->workingDatabase()==ambient,"workingDatabase changed");
        require(sha256(config.reference)==config.referenceSHA256&&sha256(config.scheme)==config.schemeSHA256,"input disk SHA changed");
        replace_owned_report(report.get(),object({{"status",quote(std::string("partial"))},{"phase",quote(std::string("A-world-only"))},
          {"standardControlsValuesMatch",controlsValuesMatch?"true":"false"},{"standardControlsComplete",controlsComplete?"true":"false"},
          {"capabilityUpgradeAllowed",controlsComplete?"true":"false"},
          {"context",object({{"regenType",quote(std::string("kAcGiStandardDisplay"))},{"isDragging","false"},{"deviation","0.01"},{"isolines","4"},
            {"plotGeneration","false"},{"supportsTrueTypeText","false"},{"supportsOwnerDraw","false"},
            {"viewport",quote(std::string("not available in phase A"))},{"annotationScale",quote(std::string("unknown; not inferred from dimensions"))}})},
          {"sourceEvents",array(sourceMonitor.events)},{"ambientEvents",array(ambientMonitor.events)},
          {"sourcePublicFingerprintUnchanged","true"},{"sourceDiskSHAUnchanged","true"},{"workingDatabaseUnchanged","true"},
          {"fontResolutionPathsVerified","false"},{"independentNaturalRenderingVerified","false"},{"glyphComplete","false"},
          {"phaseB",quote(std::string("not implemented: requires real host WorldDraw/ViewportDraw wrapper and view context"))},
          {"missing13SegmentPrototype","true"},{"records",array(records)}}));
        evidence->step("publicGraphicsRead","partial","World callback evidence only; glyph and independent natural rendering not verified; see graphics-read.json");
        evidence->finish(config,"partial");acutPrintf(L"\nBCE_GRAPHICS_PARTIAL\n");
    }catch(const std::exception& error){try{if(graphicsReport.valid())replace_owned_report(graphicsReport.get(),object({{"status",quote(std::string("failed"))},{"error",quote(std::string(error.what()))},
        {"entryWorkingDatabaseRestored",entryWorkingDatabase?(acdbHostApplicationServices()->workingDatabase()==entryWorkingDatabase?"true":"false"):"null"},
        {"records",array(records)},{"partialEvidence",quote(std::string("graphics-callbacks.jsonl and checkpoint files"))}}));if(evidence){evidence->error(error.what());evidence->finish(config,"failed");}}catch(...){}acutPrintf(L"\nBCE_GRAPHICS_FAILED\n");}
    catch(...){try{if(graphicsReport.valid())replace_owned_report(graphicsReport.get(),object({{"status",quote(std::string("failed"))},{"error",quote(std::string("unknown exception"))},
        {"entryWorkingDatabaseRestored",entryWorkingDatabase?(acdbHostApplicationServices()->workingDatabase()==entryWorkingDatabase?"true":"false"):"null"},
        {"records",array(records)}}));if(evidence){evidence->error("unknown exception");evidence->finish(config,"failed");}}catch(...){}acutPrintf(L"\nBCE_GRAPHICS_FAILED_UNKNOWN\n");}
}
}
BOOL APIENTRY DllMain(HMODULE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){moduleHandle=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message,void* id){
    try{if(message==AcRx::kInitAppMsg){acrxDynamicLinker->unlockApplication(id);acrxRegisterAppMDIAware(id);return acedRegCmds->addCommand(L"BCE_GRAPHICS_CASES",L"BCE_TEST_RUN",L"BCE_TEST_RUN",ACRX_CMD_MODAL,run_case)==Acad::eOk?AcRx::kRetOK:AcRx::kRetError;}
      if(message==AcRx::kUnloadAppMsg)return acedRegCmds->removeGroup(L"BCE_GRAPHICS_CASES")==Acad::eOk?AcRx::kRetOK:AcRx::kRetError;return AcRx::kRetOK;
    }catch(...){return AcRx::kRetError;}
}
