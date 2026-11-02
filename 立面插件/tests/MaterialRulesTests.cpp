#include "TestSupport.h"
#include "../src/MaterialRules.h"
#include <cmath>
#include <limits>

namespace bce::test {
namespace {
ClosedRing rectangle(double x0,double y0,double x1,double y1) { return {{{x0,y0},{x1,y0},{x1,y1},{x0,y1}},true}; }
ReferenceStandard materialStandard() {
    ReferenceStandard s;s.referenceSha256="109BDCFEA4C55219793F2B6F9D8C8B619344B51A4C73E9129378C38532B2AF4D";
    ReferenceFamily f;f.identity="wall-family";f.approved=true;f.approvalSource="review";f.roles={SemanticRole::materialRegion};f.laneOffsetsPaperMm={8};s.families={f};
    s.materialPrototypes={{{MaterialExpressionKind::hatch,"wall-hatch",{1,0},1,"ANSI31"},true,"reference-prototype-review"},
        {{MaterialExpressionKind::lineDrawing,"roof-lines",{0,1},2,""},true,"reference-lines-review"},
        {{MaterialExpressionKind::glassBlock,"glass-block",{1,0},0.5,""},true,"reference-glass-review"}};return s;
}
ProjectConfig materialConfig() {ProjectConfig c;c.databaseIdentity="rules-db";c.units.confirmedMmPerInputUnit=1;c.unitConfirmationSource="project-mm";return c;}
FeatureModel materialModel() {
    FeatureModel m;RuleView v;v.viewIdentity="front";v.familyId="wall-family";m.rules.views={v};
    SemanticFeature f;f.identity="wall-1";f.viewIdentity="front";f.role=SemanticRole::materialRegion;f.confirmed=true;
    f.confirmationSource="project-region-review";f.sources={{"rules-db","100",1}};
    f.region=PolygonRegion{rectangle(0,0,10000,6000),{rectangle(1200,900,2400,2400)}};
    f.materialKey="dark-stone";f.materialPrototypeId="wall-hatch";f.tableViewIdentity="front";m.rules.semanticFeatures={f};return m;
}
void rejected(const FeatureModel& m,const char* code) {const auto r=planMaterials(m,materialStandard(),materialConfig());require(!r.canCommit()&&hasCode(r,code),"invalid region must give expected diagnostic");}
}
void wallMaterialExcludesWindowHole() {
    const auto r=planMaterials(materialModel(),materialStandard(),materialConfig());require(r.canCommit(),"confirmed wall with opening hole");
    const auto& wall=r.value->regions.at(0);require(wall.areaSquareMm==58200000,"actual net area excludes 1800000 opening");
    require(materialContains(wall,{500,500}),"wall point has material");require(!materialContains(wall,{1800,1500}),"window hole stays empty");
    require(!materialContains(wall,{1200,900}),"hole boundary never covered");require(!materialContains(wall,{11000,500}),"outside stays empty");
    require(wall.geometry.holes[0].vertices[0].x==1200&&wall.sources[0].handle=="100","region geometry and source retained");
}
void invalidOpenRegionNeedsCorrection() {
    auto m=materialModel();m.rules.semanticFeatures[0].region->outer.closed=false;rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    m=materialModel();m.rules.semanticFeatures[0].region->outer.vertices={{0,0},{10000,6000},{0,6000},{10000,0}};rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    m=materialModel();m.rules.semanticFeatures[0].region->holes={rectangle(11000,100,12000,200)};rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    m=materialModel();m.rules.semanticFeatures[0].region->holes.push_back(rectangle(2000,1000,3000,2000));rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    m=materialModel();m.rules.semanticFeatures[0].region->holes={rectangle(0,900,2400,2400)};rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    m=materialModel();m.rules.semanticFeatures[0].region->holes.push_back(rectangle(1400,1000,1600,1200));rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    m=materialModel();m.rules.semanticFeatures[0].region->outer.vertices[1].x=std::numeric_limits<double>::infinity();rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    m=materialModel();m.rules.semanticFeatures[0].region->outer=rectangle(0,0,1e200,1e200);rejected(m,"INVALID_MATERIAL_TOPOLOGY");
    auto c=materialConfig();c.units={};require(hasCode(planMaterials(materialModel(),materialStandard(),c),"UNIT_REQUIRED"),"unknown units rejected");
    m=materialModel();m.rules.semanticFeatures.clear();rejected(m,"MATERIAL_REGION_REQUIRED");
    m=materialModel();m.rules.semanticFeatures[0].confirmed=false;rejected(m,"SEMANTIC_CONFIRMATION_REQUIRED");
}
void missingConstructionAndDetailIdBlocksItsAnnotation() {
    auto m=materialModel();m.rules.semanticFeatures[0].requestConstructionNote=true;
    auto r=planMaterials(m,materialStandard(),materialConfig());require(!r.canCommit()&&hasCode(r,"CONSTRUCTION_DETAIL_REQUIRED"),"missing target construction data blocked");
    require(r.value.has_value()&&r.value->regions[0].constructionNote==std::nullopt,"no reference-project proprietary note fabricated");
    m.rules.semanticFeatures[0].construction="project wall coating";r=planMaterials(m,materialStandard(),materialConfig());require(!r.canCommit(),"detail id also required");
    m.rules.semanticFeatures[0].detailId="project-A17";r=planMaterials(m,materialStandard(),materialConfig());require(r.canCommit(),"explicit target note");
    require(*r.value->regions[0].constructionNote=="project wall coating / project-A17","only actual project text emitted");
}
void materialDefaultsAreProjectOverridable() {
    auto m=materialModel();auto second=m.rules.semanticFeatures[0];second.identity="wall-2";second.materialKey="ivory-stone";second.materialPrototypeId="glass-block";
    second.region=PolygonRegion{rectangle(20000,0,30000,6000),{}};second.sources[0].handle="101";m.rules.semanticFeatures.push_back(second);
    second.identity="roof";second.materialKey="dark-roof";second.materialPrototypeId="roof-lines";second.region=PolygonRegion{rectangle(0,10000,10000,12000),{}};
    second.sources[0].handle="102";second.tableViewIdentity="";second.sharedTableIdentity="shared-material-table";m.rules.semanticFeatures.push_back(second);
    auto r=planMaterials(m,materialStandard(),materialConfig());require(r.canCommit(),"three explicit materials");
    require(r.value->regions[0].materialName=="深灰色仿石漆"&&r.value->regions[1].materialName=="米白色仿石漆"&&r.value->regions[2].materialName=="深灰色双波陶瓷瓦","three project defaults");
    require(r.value->regions[0].expression.kind==MaterialExpressionKind::hatch&&r.value->regions[1].expression.kind==MaterialExpressionKind::glassBlock&&r.value->regions[2].expression.kind==MaterialExpressionKind::lineDrawing,"expression roles distinct");
    require(r.value->regions[2].expression.direction.y==1&&r.value->regions[2].expression.scale==2&&r.value->regions[2].sharedTableIdentity=="shared-material-table","prototype orientation scale association traced");
    m.rules.materialOverrides={{"dark-stone","W9","project-white",std::string("glass-block")},{"ivory-stone","W10","project-beige",std::nullopt},{"dark-roof","R9","project-roof",std::nullopt}};
    r=planMaterials(m,materialStandard(),materialConfig());require(r.canCommit(),"project overrides");
    require(r.value->regions[0].materialNumber=="W9"&&r.value->regions[0].materialName=="project-white"&&r.value->regions[0].expression.kind==MaterialExpressionKind::glassBlock,"number name and expression override");
    require(r.value->regions[1].materialName=="project-beige"&&r.value->regions[2].materialName=="project-roof","all names overridable");
}
void materialRegionsRequireUniqueExplicitAssociation() {
    auto m=materialModel();m.rules.semanticFeatures[0].tableViewIdentity="";rejected(m,"MATERIAL_ASSOCIATION_REQUIRED");
    m=materialModel();m.rules.semanticFeatures.push_back(m.rules.semanticFeatures[0]);rejected(m,"DUPLICATE_MATERIAL_REGION");
    m=materialModel();auto duplicate=m.rules.semanticFeatures[0];duplicate.identity="other-id";
    std::rotate(duplicate.region->outer.vertices.begin(),duplicate.region->outer.vertices.begin()+1,duplicate.region->outer.vertices.end());
    m.rules.semanticFeatures.push_back(duplicate);rejected(m,"DUPLICATE_MATERIAL_REGION");
    m=materialModel();m.rules.semanticFeatures[0].tableViewIdentity="wrong-view";rejected(m,"MATERIAL_ASSOCIATION_REQUIRED");
}
void referenceAndCapabilityEvidenceRemainsUnchanged() {
    auto s=materialStandard();s.nativeCapabilities={CapabilityState::unverified,CapabilityState::unsupported};
    auto r=planMaterials(materialModel(),s,materialConfig());require(r.canCommit(),"pure material plan valid");
    require(r.value->referenceSha256==s.referenceSha256&&r.value->nativeCapabilities.glyphPreparation==CapabilityState::unsupported&&r.value->nativeCapabilities.fieldSetter==CapabilityState::unverified,"source and unsupported native evidence retained");
    s.materialPrototypes[0].approved=false;require(!planMaterials(materialModel(),s,materialConfig()).canCommit(),"unapproved prototype blocked");
    s=materialStandard();s.referenceSha256="";require(hasCode(planMaterials(materialModel(),s,materialConfig()),"REFERENCE_STANDARD_REQUIRED"),"missing reference binding blocked");
    s=materialStandard();s.materialPrototypes[0].expression.scale=std::numeric_limits<double>::quiet_NaN();require(!planMaterials(materialModel(),s,materialConfig()).canCommit(),"invalid prototype scale blocked");
    auto m=materialModel();m.rules.views[0].familyId="";require(hasCode(planMaterials(m,materialStandard(),materialConfig()),"REFERENCE_FAMILY_REQUIRED"),"material family must be explicit");
}
void materialDuplicateTopologyIgnoresRedundantVertices() {
    auto m=materialModel();auto duplicate=m.rules.semanticFeatures[0];duplicate.identity="same-physical-region";
    duplicate.region->outer.vertices.insert(duplicate.region->outer.vertices.begin()+1,{5000,0});
    duplicate.region->holes[0].vertices.push_back(duplicate.region->holes[0].vertices.front());
    m.rules.semanticFeatures.push_back(duplicate);rejected(m,"DUPLICATE_MATERIAL_REGION");
}
void emptyMaterialFamilyIsNotAnExplicitSelection() {
    auto m=materialModel();m.rules.views[0].familyId="";auto s=materialStandard();s.families[0].identity="";
    const auto r=planMaterials(m,s,materialConfig());require(!r.canCommit()&&hasCode(r,"REFERENCE_FAMILY_REQUIRED"),"empty approved identity cannot impersonate explicit selection");
}
void materialPlanRetainsFamilyAndUnitEvidence() {
    auto m=materialModel();for(auto& p:m.rules.semanticFeatures[0].region->outer.vertices) {p.x/=1000;p.y/=1000;}
    for(auto& h:m.rules.semanticFeatures[0].region->holes) {for(auto& p:h.vertices) {p.x/=1000;p.y/=1000;}}
    auto c=materialConfig();c.units.confirmedMmPerInputUnit=1000;
    const auto r=planMaterials(m,materialStandard(),c);require(r.canCommit(),"meter-input region");
    require(r.value->mmPerInputUnit==1000,"material unit evidence retained");
    require(std::abs(r.value->regions[0].areaSquareMm-58200000)<1e-6,"actual region converted to square millimeters");
    require(r.value->regions[0].familyId=="wall-family"&&r.value->regions[0].prototypeApprovalSource=="reference-prototype-review","family and prototype approval trace retained");
    require(!materialContains(r.value->regions[0],{1.8,1.5}),"meter-input window remains empty");
}
}
