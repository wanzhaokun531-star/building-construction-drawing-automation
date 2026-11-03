#include "ProjectFactsFixture.h"
#include "AuditedFactLocations.h"
#include <acutads.h>
#include <rxregsvc.h>
#include <cctype>
#include <map>
namespace {
HMODULE moduleHandle{};
void run_case() noexcept {
    using namespace bce;
    using namespace bce::native;
    using namespace bce::facts_test;
    Config config;
    std::unique_ptr<Evidence> evidence;
    try {
        config=load_config(moduleHandle); evidence=std::make_unique<Evidence>(config); validate_identity(config);
        require(config.caseName==L"CurrentProjectFactSources","unsupported case");
        bool passed=true;
        auto expect=[&](const std::string& name,bool condition,const std::string& details) { evidence->step(name,condition?"passed":"failed",details); passed=passed && condition; };
        {
            require(acDocManager->curDocument()!=nullptr,"no current document");
            auto* db=acDocManager->curDocument()->database(); require(db!=nullptr,"no current database");
            const auto model=acdbSymUtil()->blockModelSpaceId(db);
            const auto modelHandle=native_scene::handle(model);
            evidence->checkpoint("install-owned-unsaved-current-fixture");
            std::map<std::string,AcDbObjectId> mapped;
            std::vector<std::string> auditedHandles;
            {
                AcDbDatabase source(false,true);
                ok(source.readDwgFile(config.scheme.c_str(),AcDbDatabase::kForReadAndReadShare,false),"read frozen scheme"); source.closeInput(true);
                AcDbObjectIdArray ids;
                for(const auto& view:auditedViews()) for(const auto& h:{view.datum,view.title,view.lower,view.upper}) ids.append(native_scene::objectId(source,h));
                AcDbIdMapping mapping; ok(source.wblockCloneObjects(ids,model,mapping,AcDb::kDrcIgnore),"clone selected audited sources");
                for(int i=0;i<ids.length();++i) {
                    AcDbIdPair pair; pair.setKey(ids[i]); require(mapping.compute(pair) && pair.isCloned() && !pair.value().isNull(),"source current clone mapping");
                    const auto oldHandle=native_scene::handle(ids[i]),newHandle=native_scene::handle(pair.value()); mapped.emplace(oldHandle,pair.value()); auditedHandles.push_back(newHandle);
                    evidence->capability(object({{"fixtureMapping",quote(oldHandle)},{"currentHandle",quote(newHandle)}}));
                }
            }
            auto text=std::make_unique<AcDbText>(AcGePoint3d(120,240,0),L"未保存中文立面",db->textstyle(),3.25);
            ok(text->setWidthFactor(0.73),"text width"); ok(text->setRotation(0.21),"text rotation"); ok(text->setOblique(0.12),"text oblique");
            ok(text->mirrorInX(true),"text mirror X"); ok(text->setThickness(0.5),"text thickness");
            const auto chinese=append(model,std::move(text));
            auto line=std::make_unique<AcDbLine>(AcGePoint3d(12,34,0),AcGePoint3d(56,78,0)); ok(line->setThickness(2.0),"line thickness");
            const auto lineId=append(model,std::move(line));
            auto aligned=std::make_unique<AcDbAlignedDimension>(); ok(aligned->setXLine1Point({10,20,0}),"dimension point 1"); ok(aligned->setXLine2Point({30,70,0}),"dimension point 2");
            ok(aligned->setDimLinePoint({90,90,0}),"dimension display point"); ok(aligned->setDimensionText(L"文字覆盖不是定义点"),"dimension override");
            const auto dimId=append(model,std::move(aligned));
            const auto off=layer(*db,L"I3bOff",true,false),frozen=layer(*db,L"I3bFrozen",false,true);
            auto hiddenLine=std::make_unique<AcDbLine>(AcGePoint3d(1,2,0),AcGePoint3d(3,4,0)); ok(hiddenLine->setVisibility(AcDb::kInvisible),"hide entity"); const auto hidden=append(model,std::move(hiddenLine));
            auto offLine=std::make_unique<AcDbLine>(AcGePoint3d(1,2,0),AcGePoint3d(3,4,0)); ok(offLine->setLayer(off),"off layer assignment"); const auto offId=append(model,std::move(offLine));
            auto frozenLine=std::make_unique<AcDbLine>(AcGePoint3d(1,2,0),AcGePoint3d(3,4,0)); ok(frozenLine->setLayer(frozen),"frozen layer assignment"); const auto frozenId=append(model,std::move(frozenLine));
            const auto nestedOwner=block(*db); const auto nested=append(nestedOwner,std::make_unique<AcDbLine>(AcGePoint3d(1,2,0),AcGePoint3d(3,4,0)));
            const auto blockRef=append(model,std::make_unique<AcDbBlockReference>(AcGePoint3d(200,300,0),nestedOwner));
            const auto attribute=append(model,std::make_unique<AcDbAttributeDefinition>(AcGePoint3d(5,6,0),L"exact-class fixture",L"I3B_TAG",L"I3B_PROMPT",db->textstyle()));
            const auto mtext=append(model,std::make_unique<AcDbMText>());
            const auto fieldText=append(model,std::make_unique<AcDbText>(AcGePoint3d(0,0,0),L"field",db->textstyle(),2));
            {
                Open<AcDbText> e(fieldText,AcDb::kForWrite); auto field=std::make_unique<AcDbField>(L"%<\\AcVar Date \\f \"%tc1\">%",true); AcDbObjectId fieldId;
                ok(e->setField(L"TEXT",field.get(),fieldId),"attach unevaluated field"); field.release()->close(); require(e->hasFields(),"field fixture did not attach");
            }
            const auto erased=append(model,std::make_unique<AcDbLine>(AcGePoint3d(1,1,0),AcGePoint3d(2,2,0)));
            const auto erasedHandle=native_scene::handle(erased);
            { Open<AcDbEntity> e(erased,AcDb::kForWrite); ok(e->erase(),"erase owned fixture"); }
            NativeSceneContext context;
            Monitor monitor(*db);
            auto provenance=[&] { return FactReadProvenance{context.documentIdentity(),context.documentIdentity()+"/database",context.revision()}; };
            auto read=[&](const std::string& label,const FactReadProvenance& expected,const std::vector<std::string>& handles) {
                Boundary before(context,*db,monitor); evidence->checkpoint(label);
                auto result=readCurrentProjectFacts({context,expected,handles});
                expect(label+"ReadOnly",before.unchanged(),"current/active/working/context, revision, DBMOD, roots, resources, write-open/modify/append/erase/header events unchanged");
                std::vector<std::string> rows,diagnostics;
                if(result.value) for(const auto& s:result.value->sources) rows.push_back(sourceJson(s));
                for(const auto& i:result.issues) diagnostics.push_back(object({{"code",quote(i.code)},{"message",quote(i.message)},{"handle",quote(i.source.entity?i.source.entity->handle:std::string{})}}));
                evidence->capability(object({{"read",quote(label)},{"expectedRevision",std::to_string(expected.revision)},{"currentRevision",std::to_string(context.revision())},{"dbmodBefore",std::to_string(before.mod)},
                    {"dbmodAfter",std::to_string(dbmod())},{"readCompleted",result.value && result.value->readCompleted?"true":"false"},{"sources",array(rows)},{"issues",array(diagnostics)}}));
                return result;
            };
            const auto audited=read("AuditedCurrentClone",provenance(),auditedHandles);
            bool auditedExact=audited.value && audited.value->readCompleted && audited.value->modelSpaceOwnerHandle==modelHandle;
            if(audited.value) for(const auto& s:audited.value->sources)
                auditedExact=auditedExact && s.read.documentIdentity==context.documentIdentity() && s.read.databaseIdentity==context.documentIdentity()+"/database" && s.read.revision==context.revision();
            for(const auto& view:auditedViews()) {
                const auto* datum=find(audited,native_scene::handle(mapped.at(view.datum)));
                const auto* definition=datum?std::get_if<FactAlignedDimension>(&datum->definition):nullptr;
                bool correct=definition && point(definition->xLine1Point,view.x,view.y) && point(definition->xLine2Point,view.x,view.upperY);
                for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{view.title,view.titleText},{view.lower,u8"一层平面图 +0 mm"},{view.upper,u8"二层平面图 +3000 mm"}}) {
                    const auto* s=find(audited,native_scene::handle(mapped.at(pair.first))); const auto* t=s?std::get_if<FactText>(&s->definition):nullptr;
                    correct=correct && t && t->text==pair.second && s->directRoot && s->effectiveVisible && s->ownerHandle==modelHandle && s->exactClass=="AcDbText";
                }
                expect("AuditedDatumTitleFloorText_"+view.role,correct,"actual cloned current handles; frozen audit definition points and independently frozen TSV text"); auditedExact=auditedExact && correct;
            }
            expect("AuditedBatchProvenance",auditedExact && audited.value->read.documentIdentity==context.documentIdentity() && audited.value->read.databaseIdentity==context.documentIdentity()+"/database" && audited.value->read.revision==context.revision(),"batch preserves actual current context identity/revision and model owner");
            const auto ch=native_scene::handle(chinese),lh=native_scene::handle(lineId),dh=native_scene::handle(dimId);
            std::string normalized=ch; for(auto& c:normalized) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c))); normalized="000"+normalized;
            const auto raw=read("UnsavedTypedGetters",provenance(),{normalized,lh,dh});
            const auto* cs=find(raw,ch); const auto* ct=cs?std::get_if<FactText>(&cs->definition):nullptr;
            expect("UnsavedChineseAndLayout",ct && ct->text==u8"未保存中文立面" && point(ct->position,120,240) && std::abs(ct->height-3.25)<1e-8 && std::abs(ct->widthFactor-0.73)<1e-8 && std::abs(ct->rotation-0.21)<1e-8 && std::abs(ct->oblique-0.12)<1e-8 && ct->mirroredX && !ct->mirroredY && std::abs(ct->thickness-0.5)<1e-8 && !ct->hasFields && point(ct->normal,0,0,1) && ct->horizontalMode==static_cast<int>(AcDb::kTextLeft) && ct->verticalMode==static_cast<int>(AcDb::kTextBase),"raw text, public layout scalars, normalized actual handle");
            const auto* ls=find(raw,lh); const auto* lp=ls?std::get_if<FactLine>(&ls->definition):nullptr;
            expect("LineActualDefinition",lp && point(lp->start,12,34) && point(lp->end,56,78) && point(lp->normal,0,0,1) && lp->thickness==2,"actual SDK endpoints and thickness");
            const auto* ds=find(raw,dh); const auto* dp=ds?std::get_if<FactAlignedDimension>(&ds->definition):nullptr;
            expect("AlignedDefinitionIgnoresDisplayOverride",dp && point(dp->xLine1Point,10,20) && point(dp->xLine2Point,30,70),"xLine points, independent of custom display text and dim-line point");
            const auto old=provenance();
            { Open<AcDbLine> e(lineId,AcDb::kForWrite); ok(e->setStartPoint({123,456,0}),"edit unsaved line point"); }
            { Open<AcDbText> e(chinese,AcDb::kForWrite); ok(e->setTextString(L"未保存修改中文"),"edit unsaved text"); }
            const auto changed=read("ReadUnsavedEdits",provenance(),{ch,lh});
            const auto* es=find(changed,ch); const auto* et=es?std::get_if<FactText>(&es->definition):nullptr; const auto* el=find(changed,lh); const auto* ep=el?std::get_if<FactLine>(&el->definition):nullptr;
            expect("CurrentEditsReplaceDiskState",context.revision()>old.revision && et && et->text==u8"未保存修改中文" && ep && point(ep->start,123,456),"native mutation before read; fresh expected revision sees actual unsaved point/text");
            auto invalid=provenance(); invalid.revision=0; const auto zero=read("ZeroRevision",invalid,{lh}); expect("RejectZeroRevision",!zero.value && issue(zero,"FACT_STALE_CONTEXT"),"zero expected revision cannot complete");
            const auto stale=read("StaleRevision",old,{lh}); expect("RejectStaleRevision",!stale.value && issue(stale,"FACT_STALE_CONTEXT"),"earlier explicit revision never refreshed by reader");
            invalid=provenance(); invalid.documentIdentity+="-other"; const auto foreign=read("WrongDocumentIdentity",invalid,{lh}); expect("RejectWrongDocumentIdentity",!foreign.value && issue(foreign,"FACT_STALE_CONTEXT"),"wrong expected document identity rejected");
            invalid=provenance(); invalid.databaseIdentity+="-other"; const auto foreignDb=read("WrongDatabaseIdentity",invalid,{lh}); expect("RejectWrongDatabaseIdentity",!foreignDb.value && issue(foreignDb,"FACT_STALE_CONTEXT"),"wrong expected database identity rejected");
            std::vector<std::string> special;
            for(const auto id:{hidden,offId,frozenId,nested,blockRef,attribute,mtext,fieldText}) special.push_back(native_scene::handle(id));
            special.push_back(erasedHandle); special.push_back("FFFFFFFFFFFFFFFE");
            const auto boundaries=read("SourceBoundaries",provenance(),special);
            for(const auto id:{hidden,offId,frozenId}) { const auto* s=find(boundaries,native_scene::handle(id)); expect("EffectiveHidden_"+native_scene::handle(id),s && s->directRoot && !s->effectiveVisible && std::holds_alternative<FactLine>(s->definition),"entity hidden or own layer off/frozen retains typed source but is effectively hidden"); }
            const auto nh=native_scene::handle(nested); const auto* ns=find(boundaries,nh);
            expect("NonDirectOwnerLocated",ns && !ns->directRoot && ns->ownerHandle==native_scene::handle(nestedOwner) && ns->exactClass=="AcDbLine" && std::holds_alternative<std::monostate>(ns->definition) && issue(boundaries,"FACT_SOURCE_NON_DIRECT",nh),"actual block-definition member remains non-direct");
            for(const auto id:{blockRef,attribute,mtext}) { const auto h=native_scene::handle(id); const auto* s=find(boundaries,h); Open<AcDbEntity> actual(id); expect("UnsupportedExactClass_"+h,s && s->exactClass==native_scene::utf8(actual->isA()->name()) && std::holds_alternative<std::monostate>(s->definition) && issue(boundaries,"FACT_SOURCE_UNSUPPORTED",h),"real exact class retained; AttributeDefinition must not pass Text cast"); }
            const auto fh=native_scene::handle(fieldText); const auto* fs=find(boundaries,fh); const auto* ft=fs?std::get_if<FactText>(&fs->definition):nullptr;
            expect("FieldPresencePreserved",ft && ft->hasFields && issue(boundaries,"FACT_SOURCE_FIELD",fh),"public hasFields preserved without evaluation; core remains responsible for unsupported field semantics");
            expect("ProvenMissingLocated",boundaries.value && boundaries.value->readCompleted && !find(boundaries,erasedHandle) && !find(boundaries,"FFFFFFFFFFFFFFFE") && issue(boundaries,"FACT_SOURCE_MISSING",erasedHandle) && issue(boundaries,"FACT_SOURCE_MISSING","FFFFFFFFFFFFFFFE"),"known erased and unresolved valid handle are proven missing, never substituted");
            const auto malformed=read("MalformedUnavailable",provenance(),{"not-a-handle"});
            expect("UnavailableIsNotMissing",!malformed.value && issue(malformed,"FACT_SOURCE_UNAVAILABLE") && !issue(malformed,"FACT_SOURCE_MISSING"),"malformed request cannot masquerade as proven orphan or completed batch");
            evidence->capability(object({{"observedEventsIncludingFixtureEdits",array(monitor.events)},{"eventLoggingFailed",monitor.loggingFailed?"true":"false"},{"actualSwitchCoverage",quote(std::string("not exercised; root accepted deferred document scheduling"))}}));
        }
        require(sha256(config.scheme)==config.schemeSHA256 && sha256(config.reference)==config.referenceSHA256,"frozen input changed");
        evidence->checkpoint("current-fact-reader-tests-complete"); evidence->finish(config,passed?"passed":"failed");
        evidence.reset(); // NativeEvidence uses share0; release terminal report for unchanged watcher.
        acutPrintf(passed?L"\nBCE_NATIVE_CASE_PASSED\n":L"\nBCE_NATIVE_CASE_FAILED\n");
    } catch(const std::exception& failure) {
        try { if(evidence) { evidence->error(failure.what()); evidence->finish(config,"failed"); evidence.reset(); } } catch(...) {}
    } catch(...) {
        try { if(evidence) { evidence->error("unknown exception"); evidence->finish(config,"failed"); evidence.reset(); } } catch(...) {}
    }
}
}
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID) { if(reason==DLL_PROCESS_ATTACH) { moduleHandle=module; DisableThreadLibraryCalls(module); } return TRUE; }
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message,void* id) {
    try {
        if(message==AcRx::kInitAppMsg) { acrxDynamicLinker->unlockApplication(id); acrxRegisterAppMDIAware(id); return acedRegCmds->addCommand(L"BCE_FACT_SOURCE_CASES",L"BCE_TEST_RUN",L"BCE_TEST_RUN",ACRX_CMD_MODAL,run_case)==Acad::eOk?AcRx::kRetOK:AcRx::kRetError; }
        if(message==AcRx::kUnloadAppMsg) return acedRegCmds->removeGroup(L"BCE_FACT_SOURCE_CASES")==Acad::eOk?AcRx::kRetOK:AcRx::kRetError;
        return AcRx::kRetOK;
    } catch(...) { return AcRx::kRetError; }
}

