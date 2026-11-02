#include "MaterialFixtures.h"
#include "NativeEvidence.h"
#include <aced.h>
#include <acutads.h>
#include <rxregsvc.h>
#include <memory>
namespace {
HMODULE moduleHandle{};
void runCase() noexcept {
    bce::native::Config config; std::unique_ptr<bce::native::Evidence> evidence;
    try {
        using namespace bce; using namespace bce::native; using namespace bce::material_test;
        config=load_config(moduleHandle); evidence=std::make_unique<Evidence>(config); validate_identity(config);
        require(config.caseName==L"NativeMaterialPreparation", "unsupported case");
        require(acDocManager->isApplicationContext(),"M1 test command application context");
        DocumentLock originalDocumentLock(acDocManager->curDocument());
        auto report=create_new(config.output+L"/material-case.json");
        AcDbDatabase source(false,true); require(source.readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndReadShare,false)==Acad::eOk,"read reference"); source.closeInput(true);
        MaterialPlan plan; plan.referenceSha256=config.referenceSHA256; plan.mmPerInputUnit=1; plan.unitDeclaration.confirmedMmPerInputUnit=1; plan.unitConfirmationSource="M1 explicit mm fixture";
        MaterialRegionPlan region; region.identity="wall";region.viewIdentity="fixture-front";region.familyId="explicit-material";region.tableViewIdentity=region.viewIdentity;region.confirmationSource="M1 rectangular wall with window fixture";region.prototypeApprovalSource="N1 bound 114CF selected for adapter test only";region.sources={{"fixture","1",1}};region.materialNumber="M1";region.materialName="fixture material";
        region.geometry.outer={{{0,0},{10000,0},{10000,6000},{0,6000}},true};region.geometry.holes={{{{1200,900},{2400,900},{2400,2400},{1200,2400}},true}};region.areaSquareMm=58200000;region.expression.prototypeId="reference-114CF";region.expression.patternName=u8"大理石";plan.regions.push_back(region);
        ReferenceMaterialSeed seed; seed.database=&source;seed.referenceSha256=config.referenceSHA256;seed.prototypeId="reference-114CF";seed.handle="114CF";seed.mmPerSourceUnit=1;seed.sourcePaperScale=100;seed.unitConfirmationSource="test explicit mm";seed.scaleConfirmationSource="test explicit 1:100";seed.prototypeConfirmationSource=region.prototypeApprovalSource;seed.phaseConfirmationSource="test source origin anchor policy";
        MaterialPreparationOptions options; options.placements.push_back({"wall","fixture-front",100,{0,0},"test 1:100","test phase mapping"});
        // Explicit test phase: the existing prototype origin is the reference anchor, target origin 12.25, 37.5.
        AcDbObjectId sourceId;ok(source.getAcDbObjectId(sourceId,false,AcDbHandle(L"114CF")),"known reference prototype");
        double sourceScale{},sourceAngle{},sourceSpacing{};Adesk::UInt32 sourceColor{};std::wstring sourceLayer,sourceLinetype;bool sourceDouble{};int sourceDefinitions{};
        {Open<AcDbHatch> h(sourceId);seed.referenceAnchor={h->originPoint().x,h->originPoint().y};sourceScale=h->patternScale();sourceAngle=h->patternAngle();sourceSpacing=spacing(*h.p);sourceColor=h->color().color();sourceLayer=layer(*h.p);sourceLinetype=linetype(*h.p);sourceDouble=h->patternDouble();sourceDefinitions=h->numPatternDefinitions();}
        options.placements[0].targetAnchor={12.25,37.5};
        auto* ambient=acDocManager->curDocument()->database();const auto textId=appendText(*ambient);
        Monitor ambientMonitor(*ambient),sourceMonitor(source);const auto beforeGlobals=globals();const auto workingBefore=acdbHostApplicationServices()->workingDatabase();const auto documentBefore=acDocManager->curDocument();
        bool passed=true;std::vector<std::string> observations;
        auto expect=[&](const std::string& name,bool pass,const std::string& detail){evidence->step(name,pass?"passed":"failed",detail);passed=passed&&pass;observations.push_back(object({{"name",quote(name)},{"passed",pass?"true":"false"},{"detail",quote(detail)}}));};
        evidence->checkpoint("prepare-bound-native-reference-material");
        auto wholeWall=prepareMaterials(plan,{seed},options);
        const bool wholeBlocked=!wholeWall.canCommit()&&!wholeWall.value&&!wholeWall.issues.empty()&&wholeWall.issues.front().message.find("EMPTY_NATIVE_PATTERN_OUTPUT")!=std::string::npos;
        expect("EmptyPatternCannotAuthorizeCommittableBatch",wholeBlocked,wholeWall.issues.empty()?"REGRESSION: whole-wall blank preparation was accepted":wholeWall.issues.front().message);
        evidence->capability(object({{"wholeWallCapability",quote(std::string("BLOCKED_CAPABILITY"))},{"requestedNetAreaSquareMm","58200000"},{"supportedWholeWall","false"},{"reason",quote(std::string(wholeBlocked?"actual requested full-wall pattern output is empty; no committable batch returned":"whole-wall rejection contract FAILED; inspect assertions"))}}));
        // A separate, explicitly named 2 square metre fixture does not replace the full-wall requirement.
        plan.regions[0].identity="small-panel";options.placements[0].regionIdentity="small-panel";
        plan.regions[0].geometry.outer={{{0,0},{2000,0},{2000,1000},{0,1000}},true};plan.regions[0].geometry.holes.clear();plan.regions[0].areaSquareMm=2000000;
        auto result=prepareMaterials(plan,{seed},options);
        expect("PrepareSeparateSmallPanelHatch",result.canCommit(),result.issues.empty()?"actual native material batch":result.issues.front().message);
        if(result.canCommit()) {
            auto& batch=*result.value;expect("ImmutableBatchInitiallyIntact",batch.intact(),"actual Hatch loops, patterns, appearance/resources and permanent mutation guard");
            {std::vector<std::string> properties;{Open<AcDbHatch> sourceHatch(sourceId);properties.push_back(displayProperties(*sourceHatch.p,source,"reference-original-read-only"));}{Working work(batch.database());Open<AcDbHatch> targetHatch(batch.roots()[0]);properties.push_back(displayProperties(*targetHatch.p,batch.database(),"prepared-target-read-only"));}auto diagnostic=create_new(config.output+L"/material-visibility-diagnostic.json");const auto diagnosticJson=object({{"properties",array(properties)},{"ambientReadOnlyGlobals",displayGlobals()},{"scope",quote(std::string("read-only actual entity/layer/database flags; no visibility/global overrides"))}});write_flushed(diagnostic.get(),diagnosticJson);evidence->capability(diagnosticJson);}            expect("TargetRootListIsOnlyHatch",batch.roots().length()==1&&modelRoots(batch.database()).length()==1,"no cloned old boundary or reference building root");
            {
                Working work(batch.database());Open<AcDbHatch> h(batch.roots()[0]);double area{};ok(h->getArea(area),"SDK getArea");
                expect("SmallPanelNativeAreaAndExactLoop",nearValue(area,2000000)&&panelLoops(*h.p),"nativeArea="+number(area));
                expect("SmallPanelHasActualNativePatternOutput",h->numHatchLines()>0,"native generated segments="+std::to_string(h->numHatchLines()));                expect("ReferencePatternPhysicalExpression",nearValue(h->patternScale(),sourceScale)&&nearValue(h->patternAngle(),sourceAngle)&&nearValue(spacing(*h.p),sourceSpacing)&&h->patternDouble()==sourceDouble&&h->numPatternDefinitions()==sourceDefinitions&&h->color().color()==sourceColor&&layer(*h.p).find(sourceLayer)!=std::wstring::npos&&linetype(*h.p).find(sourceLinetype)!=std::wstring::npos,"scale="+number(h->patternScale())+" angle="+number(h->patternAngle())+" spacing="+number(spacing(*h.p))+" sourceSpacing="+number(sourceSpacing)+" definitions="+std::to_string(h->numPatternDefinitions()));
                expect("ExplicitPatternPhaseMapping",nearValue(h->originPoint().x,12.25)&&nearValue(h->originPoint().y,37.5),"origin="+number(h->originPoint().x)+","+number(h->originPoint().y));
            }
            AcDbDatabase output(true,true);auto cloned=batch.cloneToEmpty(output);expect("OnlyPreparedHatchClonesToEmpty",cloned.canCommit()&&modelRoots(output).length()==1,"root-list cloning excludes old source geometry");
            if(cloned.canCommit()) {
                const auto savedId=cloned.value->front();const auto outputPath=config.output+L"/material-roundtrip.dwg";require(GetFileAttributesW(outputPath.c_str())==INVALID_FILE_ATTRIBUTES,"exclusive material DWG");
                {Working work(output);ok(output.saveAs(outputPath.c_str(),false,AcDb::kDHL_CURRENT),"save test-only material DWG");}
                AcDbDatabase reopened(false,true);ok(reopened.readDwgFile(outputPath.c_str(),AcDbDatabase::kForReadAndReadShare,false),"independent material reopen");reopened.closeInput(true);AcDbObjectId reopenedId;ok(reopened.getAcDbObjectId(reopenedId,false,savedId.handle()),"reopen hatch handle");
                {Working work(reopened);Open<AcDbHatch> h(reopenedId);double area{};ok(h->getArea(area),"reopened area");expect("SmallPanelIndependentDwgReopen",modelRoots(reopened).length()==1&&panelLoops(*h.p)&&nearValue(area,2000000)&&nearValue(h->patternScale(),sourceScale)&&h->color().color()==sourceColor&&nearValue(h->originPoint().x,12.25)&&nearValue(h->originPoint().y,37.5),"roots="+std::to_string(modelRoots(reopened).length())+" loops="+(panelLoops(*h.p)?"true":"false")+" area="+number(area)+" scale="+number(h->patternScale())+" color="+std::to_string(h->color().color())+" origin="+number(h->originPoint().x)+","+number(h->originPoint().y));}
            }
            // A translated noninteger region must retain each actual input coordinate.
            auto shiftedPlan=plan;for(auto& p:shiftedPlan.regions[0].geometry.outer.vertices){p.x+=100.125;p.y-=20.375;}for(auto& hole:shiftedPlan.regions[0].geometry.holes)for(auto& p:hole.vertices){p.x+=100.125;p.y-=20.375;}
            auto shiftedOptions=options;shiftedOptions.placements[0].targetAnchor={112.375,17.125};auto shifted=prepareMaterials(shiftedPlan,{seed},shiftedOptions);
            std::string shiftedDetail;bool shiftedOk=false;if(shifted.canCommit()){Open<AcDbHatch> h(shifted.value->roots()[0]);shiftedOk=panelLoops(*h.p,100.125,-20.375)&&nearValue(h->originPoint().x,112.375)&&nearValue(h->originPoint().y,17.125);shiftedDetail="loops="+std::string(panelLoops(*h.p,100.125,-20.375)?"true":"false")+" origin="+number(h->originPoint().x)+","+number(h->originPoint().y);}
            expect("NonintegerCoordinatesAndTranslation",shiftedOk,shifted.issues.empty()?shiftedDetail:shifted.issues.front().message);
            auto halfOptions=options;halfOptions.placements[0].targetPaperScale=50;auto half=prepareMaterials(plan,{seed},halfOptions);bool halfOk=false;
            if(half.canCommit()){Open<AcDbHatch> h(half.value->roots()[0]);halfOk=panelLoops(*h.p)&&nearValue(h->patternScale(),sourceScale*.5)&&nearValue(spacing(*h.p),sourceSpacing*.5);}
            expect("PaperScale50ChangesPatternOnly",halfOk,half.issues.empty()?"1:100 to 1:50 halves physical pattern spacing; building loops unchanged":half.issues.front().message);
            auto metersPlan=plan;metersPlan.mmPerInputUnit=1000;metersPlan.unitDeclaration.confirmedMmPerInputUnit=1000;for(auto& p:metersPlan.regions[0].geometry.outer.vertices){p.x/=1000;p.y/=1000;}for(auto& hole:metersPlan.regions[0].geometry.holes)for(auto& p:hole.vertices){p.x/=1000;p.y/=1000;}
            auto metersOptions=halfOptions;metersOptions.placements[0].targetAnchor={.01225,.0375};auto meters=prepareMaterials(metersPlan,{seed},metersOptions);bool metersOk=false;
            if(meters.canCommit()){Open<AcDbHatch> h(meters.value->roots()[0]);double area{};ok(h->getArea(area),"meters area");Adesk::Int32 type{};AcGePoint2dArray points;AcGeDoubleArray bulges;ok(h->getLoopAt(0,type,points,bulges),"meters loop");metersOk=nearValue(area,2)&&nearValue(points[1].x,2)&&nearValue(points[2].y,1)&&nearValue(h->patternScale(),sourceScale*.0005)&&nearValue(spacing(*h.p),sourceSpacing*.0005);}
            expect("NonMmUnitsPreservePhysicalSpacing",metersOk,meters.issues.empty()?"separate small panel vertices 2 x 1 metres, native area2, paper50 physical spacing":meters.issues.front().message);
            auto holePlan=plan;holePlan.regions[0].geometry.holes={{{{200,200},{600,200},{600,600},{200,600}},true}};holePlan.regions[0].areaSquareMm=1840000;auto holeResult=prepareMaterials(holePlan,{seed},options);bool holeOk=false;std::string holeDetail=holeResult.issues.empty()?"":holeResult.issues.front().message;
            if(holeResult.canCommit()){Working work(holeResult.value->database());Open<AcDbHatch> h(holeResult.value->roots()[0]);double area{};ok(h->getArea(area),"small-hole area");int segments{};holeOk=panelLoops(*h.p,0,0,true)&&nearValue(area,1840000)&&smallWindowVoid(*h.p,segments);holeDetail="small-panel nativeArea="+number(area)+" loops="+std::to_string(h->numLoops())+" generatedSegments="+std::to_string(segments);}
            expect("SeparateSmallHoleAreaAndNativeVoid",holeOk,holeDetail);            rejectDocumentTargets(batch,expect);
            for(const auto value:{std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}){
                auto nonfinite=plan;nonfinite.regions[0].areaSquareMm=value;auto denied=prepareMaterials(nonfinite,{seed},options);
                expect(std::isnan(value)?"NaNPlanAreaRejected":"InfinitePlanAreaRejected",!denied.canCommit()&&!denied.value,denied.issues.empty()?"REGRESSION: nonfinite plan area accepted":denied.issues.front().message);
            }
            {auto overflow=plan;overflow.mmPerInputUnit=1e200;overflow.unitDeclaration.confirmedMmPerInputUnit=1e200;auto denied=prepareMaterials(overflow,{seed},options);expect("AreaUnitSquareOverflowRejected",!denied.canCommit()&&!denied.value,denied.issues.empty()?"REGRESSION: overflowing area conversion accepted":denied.issues.front().message);}
            {AcDbDatabase changedReference(false,true);{Working work(changedReference);ok(changedReference.readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndReadShare,false),"load independently modified memory reference");changedReference.closeInput(true);}
                AcDbObjectId changedId;ok(changedReference.getAcDbObjectId(changedId,false,AcDbHandle(L"114CF")),"memory reference handle");AcDbObjectId changedLayer;
                {Working work(changedReference);Open<AcDbHatch> h(changedId,AcDb::kForWrite);changedLayer=h->layerId();ok(h->setColorIndex(1),"unsaved reference color mutation");ok(h->setPatternScale(sourceScale*2),"unsaved reference pattern mutation");}
                {Open<AcDbLayerTableRecord> l(changedLayer,AcDb::kForWrite);AcCmColor color;color.setColorIndex(2);l->setColor(color);}
                Monitor changedMonitor(changedReference);auto changedSeed=seed;changedSeed.database=&changedReference;auto anchored=prepareMaterials(plan,{changedSeed},options);bool anchoredToFile=false;
                if(anchored.canCommit()){Open<AcDbHatch> h(anchored.value->roots()[0]);Open<AcDbLayerTableRecord> l(h->layerId());anchoredToFile=h->color().color()==sourceColor&&nearValue(h->patternScale(),sourceScale)&&l->color().colorIndex()==249;}
                expect("UnsavedReferenceChangesCannotBypassFileShaBinding",anchoredToFile&&sha256(config.reference)==config.referenceSHA256,anchored.issues.empty()?"output must use disk-bound color159,scale50,layer249 despite caller memory color1,scale100,layer2":anchored.issues.front().message);
                {Open<AcDbHatch> h(changedId);Open<AcDbLayerTableRecord> l(changedLayer);expect("ModifiedReferenceMemoryRemainsUntouched",changedMonitor.changes==0&&h->colorIndex()==1&&nearValue(h->patternScale(),sourceScale*2)&&l->color().colorIndex()==2,"caller unsaved color/pattern/layer changes preserved; events="+std::to_string(changedMonitor.changes));}
            }            auto rejected=[&](const std::string& name,const MaterialPlan& p,const std::vector<ReferenceMaterialSeed>& ss,const MaterialPreparationOptions& o){auto r=prepareMaterials(p,ss,o);expect(name,!r.canCommit()&&!r.value,r.issues.empty()?"no diagnosis":r.issues.front().message);};
            auto wrongKind=plan;wrongKind.regions[0].expression.kind=MaterialExpressionKind::lineDrawing;rejected("LineDrawingExplicitlyUnsupported",wrongKind,{seed},options);wrongKind.regions[0].expression.kind=MaterialExpressionKind::glassBlock;rejected("GlassBlockExplicitlyUnsupported",wrongKind,{seed},options);
            auto missing=seed;missing.unitConfirmationSource.clear();rejected("MissingSourceUnitFactRejected",plan,{missing},options);missing=seed;missing.referenceSha256=std::string(64,'0');rejected("WrongReferenceShaRejected",plan,{missing},options);
            auto invalid=plan;invalid.regions[0].geometry.outer.closed=false;rejected("OpenRegionRejected",invalid,{seed},options);invalid=plan;std::swap(invalid.regions[0].geometry.outer.vertices[1],invalid.regions[0].geometry.outer.vertices[2]);rejected("SelfIntersectingRegionRejected",invalid,{seed},options);invalid=plan;invalid.regions[0].geometry.holes={{{{-1,200},{600,200},{600,600},{200,600}},true}};rejected("InvalidHoleRejected",invalid,{seed},options);invalid=plan;invalid.regions[0].expression.scale=0;rejected("ZeroPatternScaleRejected",invalid,{seed},options);
            missing=seed;missing.handle="1F";{AcDbObjectId nonEntityId;ok(source.getAcDbObjectId(nonEntityId,false,AcDbHandle(L"1F")),"nonentity known fixture");Open<AcDbObject> record(nonEntityId);require(record->isA()==AcDbBlockTableRecord::desc(),"1F must be verified BlockTableRecord, not a wrong entity-class example");}rejected("NonEntityBlockRecordRejectedAtOpen",plan,{missing},options);
            {
                AcDbDatabase gradientDb(true,true);AcDbObjectId gradientId;
                {Working work(gradientDb);Open<AcDbBlockTableRecord> model(acdbSymUtil()->blockModelSpaceId(&gradientDb),AcDb::kForWrite);auto gradient=std::make_unique<AcDbHatch>();gradient->setDatabaseDefaults(&gradientDb);ok(gradient->setHatchObjectType(AcDbHatch::kGradientObject),"gradient object fixture");ok(gradient->setGradient(AcDbHatch::kPreDefinedGradient,L"LINEAR"),"gradient style fixture");AcGePoint2dArray points;AcGeDoubleArray bulges;for(const auto p:{AcGePoint2d(0,0),AcGePoint2d(100,0),AcGePoint2d(100,100),AcGePoint2d(0,100),AcGePoint2d(0,0)}){points.append(p);bulges.append(0);}ok(gradient->appendLoop(AcDbHatch::kExternal|AcDbHatch::kPolyline,points,bulges),"gradient loop");ok(model->appendAcDbEntity(gradientId,gradient.get()),"gradient append");gradient.release()->close();}
                const auto gradientPath=config.output+L"/gradient-rejection-fixture.dwg";require(GetFileAttributesW(gradientPath.c_str())==INVALID_FILE_ATTRIBUTES,"exclusive gradient fixture");{Working work(gradientDb);ok(gradientDb.saveAs(gradientPath.c_str(),false,AcDb::kDHL_CURRENT),"save gradient fixture");}
                AcDbDatabase gradientRead(false,true);ok(gradientRead.readDwgFile(gradientPath.c_str(),AcDbDatabase::kForReadAndReadShare,false),"reopen gradient fixture provenance");gradientRead.closeInput(true);auto gradientSeed=seed;gradientSeed.database=&gradientRead;gradientSeed.referenceSha256=sha256(gradientPath);ACHAR handle[32]{};gradientId.handle().getIntoAsciiBuffer(handle);gradientSeed.handle=utf8(handle);auto gradientPlan=plan;gradientPlan.referenceSha256=gradientSeed.referenceSha256;Monitor gradientMonitor(gradientRead);
                auto gradientResult=prepareMaterials(gradientPlan,{gradientSeed},options);expect("GradientExplicitlyUnsupported",!gradientResult.canCommit()&&!gradientResult.value&&!gradientResult.issues.empty()&&gradientResult.issues.front().message.find("UNSUPPORTED_MATERIAL_NATIVE_TYPE")!=std::string::npos,gradientResult.issues.empty()?"missing gradient diagnostic":gradientResult.issues.front().message);expect("GradientSourceUntouched",gradientMonitor.changes==0,"unsupported expression read only");
            }
            auto rotatedPlan=plan;rotatedPlan.regions[0].expression.direction={0,1};auto rotated=prepareMaterials(rotatedPlan,{seed},options);std::string rotatedDetail;bool rotatedOk=false;
            if(rotated.canCommit()){Open<AcDbHatch> h(rotated.value->roots()[0]);rotatedOk=panelLoops(*h.p)&&nearValue(std::sin(h->patternAngle()-sourceAngle),1)&&nearValue(spacing(*h.p),sourceSpacing)&&nearValue(h->originPoint().x,12.25)&&nearValue(h->originPoint().y,37.5);rotatedDetail="loops="+std::string(panelLoops(*h.p)?"true":"false")+" angle="+number(h->patternAngle())+" spacing="+number(spacing(*h.p))+" origin="+number(h->originPoint().x)+","+number(h->originPoint().y);}
            expect("ExplicitDirectionRotatesPatternOnly",rotatedOk,rotated.issues.empty()?rotatedDetail:rotated.issues.front().message);            for(const auto mutation:{0,1,2,3}) {
                auto altered=prepareMaterials(plan,{seed},options);require(altered.canCommit(),"mutation fixture prepare");
                {Open<AcDbHatch> h(altered.value->roots()[0],AcDb::kForWrite);if(mutation==0)ok(h->setPatternScale(h->patternScale()*2),"mutate pattern");else if(mutation==1)ok(h->removeLoopAt(0),"mutate loop");else if(mutation==2)ok(h->setColorIndex(1),"mutate appearance");else{const auto id=h->layerId();h.p->close();h.p=nullptr;Open<AcDbLayerTableRecord> layerRecord(id,AcDb::kForWrite);layerRecord->setIsOff(true);}}
                AcDbDatabase rejectedTarget(true,true);auto copied=altered.value->cloneToEmpty(rejectedTarget);
                expect("ModifiedPreparationRejected"+std::to_string(mutation),!altered.value->intact()&&!copied.canCommit()&&modelRoots(rejectedTarget).isEmpty(),"loop/pattern/entity/layer mutation invalidates immutable native batch before copying");
            }
            expect("OriginalBatchStillIntact",batch.intact(),"read and clone leave immutable original prepared expression unchanged");
        }
        {Open<AcDbText> text(textId);expect("CurrentUnsavedChineseUnchanged",std::wstring(text->textString())==L"M1 未保存中文材料源保持"&&text->position()==AcGePoint3d(12.25,77.125,0),"current-memory Chinese fixture remains without saving current document");}
        expect("SourceAndAmbientZeroMutation",sourceMonitor.changes==0&&ambientMonitor.changes==0,"source events="+std::to_string(sourceMonitor.changes)+" ambient events="+std::to_string(ambientMonitor.changes));
        expect("GlobalsAndWorkingDatabaseRestored",globals()==beforeGlobals&&acdbHostApplicationServices()->workingDatabase()==workingBefore&&acDocManager->curDocument()==documentBefore,"system variables, working DB pointer and current document restored after success/rejection");
        write_flushed(report.get(),object({{"status",quote(std::string(passed?"partial":"failed"))},{"safetyAndSmallPanelAssertionsPassed",passed?"true":"false"},{"wholeWallCapability",quote(std::string("BLOCKED_CAPABILITY"))},{"wholeWallRejected",wholeBlocked?"true":"false"},{"assertions",array(observations)},{"scope",quote(std::string("PARTIAL M1: 58.2M whole-wall remains BLOCKED_CAPABILITY; separate 2M panel only; no project region inference; no InPlaceWriter commit"))}}));
        require(sha256(config.reference)==config.referenceSHA256&&sha256(config.scheme)==config.schemeSHA256,"source changed");
        evidence->finish(config,passed?"partial":"failed");    } catch(const std::exception& e) {try{if(evidence){evidence->error(e.what());evidence->finish(config,"failed");}}catch(...) {}}
    catch(...) {try{if(evidence){evidence->error("unknown ABI failure");evidence->finish(config,"failed");}}catch(...) {}}
}
}
BOOL APIENTRY DllMain(HMODULE h,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){moduleHandle=h;DisableThreadLibraryCalls(h);}return TRUE;}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message,void* id){try{if(message==AcRx::kInitAppMsg){acrxDynamicLinker->unlockApplication(id);acrxRegisterAppMDIAware(id);return acedRegCmds->addCommand(L"BCE_MATERIAL_CASES",L"BCE_TEST_RUN",L"BCE_TEST_RUN",ACRX_CMD_SESSION,runCase)==Acad::eOk?AcRx::kRetOK:AcRx::kRetError;}if(message==AcRx::kUnloadAppMsg)return acedRegCmds->removeGroup(L"BCE_MATERIAL_CASES")==Acad::eOk?AcRx::kRetOK:AcRx::kRetError;return AcRx::kRetOK;}catch(...){return AcRx::kRetError;}}











