#pragma once
#include "ProjectStoreCodec.h"
namespace bce::store_test {
inline StoredProject fixture(const std::string& database="fixture-db",const std::string& document="fixture-doc",std::uint64_t revision=7) {
    const EntityKey source{database,"A1",revision}, output{database,"B2",revision};
    StoredProject p; p.persistentProjectId="SDK夹具-项目-001";
    p.config.databaseIdentity=database; p.config.units={4,true,1.0,"毫米"};
    p.config.unitConfirmationSource="测试明确单位"; p.config.zeroLevel={"一层+0","实际测点确认",true};
    p.config.views.push_back({"东立面","actual-view-signature",{"白色涂料","确认材料",true},{"1→4","确认平面",true},Point2{-1.0,0.0}});
    p.ruleInputsConfirmationSource="SDK测试：所选family/比例/材料覆盖均明确确认";
    RuleView view; view.viewIdentity="东立面"; view.familyId="夹具-立面"; view.scaleDenominator=87.125;
    view.datum=SourcePoint{{-123.45678901234567,-0.0},source}; view.datumConfirmationSource="真实基准夹具";
    view.datumMeters=-0.12345678901234567; view.heightDirection={0.6,0.8};
    view.requiredRoles={SemanticRole::bodyContour,SemanticRole::level,SemanticRole::planAxis}; view.requirePlanAxes=false;
    p.rules.views.push_back(view);
    SemanticFeature f; f.identity="外墙区域"; f.viewIdentity="东立面"; f.role=SemanticRole::materialRegion;
    f.confirmed=true; f.confirmationSource="夹具明确区域"; f.points={{{0.1,-0.0},source},{{3000.125,4000.25},source}};
    f.confirmedRectangle=true; f.fromConfirmedPlan=true; f.region=PolygonRegion{{{{-0.0,0.0},{3000.125,0.0},{3000.125,4000.25},{0.0,4000.25}},true},{{{{20,20},{20,30},{30,30},{30,20}},true}}};
    f.sources={source}; f.materialKey="墙漆"; f.materialPrototypeId="涂料图例"; f.tableViewIdentity="东立面"; f.sharedTableIdentity="共享表";
    f.requestConstructionNote=true; f.construction="水泥砂浆基层"; f.detailId="项目详图A"; p.rules.semanticFeatures.push_back(f);
    p.rules.materialOverrides.push_back({"墙漆","材一","白色涂料",std::string("涂料图例")});
    p.generation.documentIdentity=document; p.generation.databaseIdentity=database;
    EntityFingerprint a{source,"geometry-source","text-source","resources-source","AcDbLine"};
    EntityFingerprint b{output,"geometry-output","无文字","resources-output","AcDbLine"};
    GeneratedViewState g; g.viewIdentity="东立面"; g.moduleIdentity="SDK-fixture"; g.moduleVersion="test-only";
    g.generationId="fixture-generation"; g.sourceSignature="source-sig"; g.configSignature="config-sig"; g.committedRevision=revision;
    g.sourceEntities={a}; g.outputs={b}; g.requiresReconfirmation=true; g.commitEvidence="仅测试提交"; g.postCommitEvidence="仅测试复核";
    p.generation.views={g}; p.confirmedEntities={a}; return p;
}
inline std::vector<View> views(const StoredProject& p,const std::string& db="reopened-db",std::uint64_t revision=42) {std::vector<View> result;for(const auto& c:p.config.views){View v;v.identity=c.viewIdentity;v.geometrySignature=c.geometrySignature;for(const auto& f:p.confirmedEntities){auto k=f.key;k.databaseIdentity=db;k.revision=revision;v.members.push_back(k);}if(!v.members.empty())v.titleKey=v.members[0];result.push_back(v);}return result;}
inline ObservedScene observed(const StoredProject& p,const std::string& database="reopened-db") {
    ObservedScene o; o.scene.documentIdentity="reopened-doc"; o.scene.databaseIdentity=database; o.scene.revision=42;
    for(auto f:p.confirmedEntities) { f.key.databaseIdentity=database; f.key.revision=42; o.fingerprints.push_back(f); o.scene.targets.push_back({f.key,"0",{},"东立面"}); }
    for(const auto& g:p.generation.views) for(auto f:g.outputs) { f.key.databaseIdentity=database; f.key.revision=42; o.fingerprints.push_back(f); o.scene.preserved.push_back({f.key,"0",{},"东立面"}); }
    return o;
}
}
