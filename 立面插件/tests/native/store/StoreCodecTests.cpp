#include "StoreFixture.h"
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace bce;
using namespace bce::store_test;
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
std::string bytes(const StoredProject& p) { const auto r=encodeProject(p); check(r.canCommit(),"encode failed"); return *r.value; }
StoredProject twoViewFixture() {
    auto p=fixture();auto view=p.config.views[0];view.viewIdentity="西立面";p.config.views.push_back(view);
    auto history=p.generation.views[0];history.viewIdentity="西立面";history.generationId="fixture-generatiom";
    history.outputs[0].key.handle="B3";p.generation.views.push_back(history);return p;
}
void refreshTestChecksum(std::string& bytes) {
    std::uint32_t crc=0xFFFFFFFF;
    for(std::size_t i=0;i<bytes.size()-8;++i){crc^=static_cast<unsigned char>(bytes[i]);for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xEDB88320u:0u);}
    const std::uint32_t finalCrc=~crc; const auto checksum=static_cast<std::uint64_t>(finalCrc);
    for(unsigned i=0;i<8;++i)bytes[bytes.size()-8+i]=static_cast<char>((checksum>>(i*8))&255);
}
int main() {
    int failures=0;
    const auto run=[&](const char* name,const std::function<void()>& f) { try { f(); std::cout<<"PASS "<<name<<'\n'; } catch(const std::exception& e) { ++failures; std::cout<<"FAIL "<<name<<": "<<e.what()<<'\n'; } };
    run("AllFieldsChineseExactDoubleRoundtrip",[]{const auto p=fixture(); const auto b=bytes(p); const auto d=decodeProject(b); check(d.readable()&&!d.canCommit()&&!d.value->bound,"decode must be unbound"); check(bytes(d.value->project)==b,"all field byte equality"); check(d.value->project.rules.semanticFeatures[0].construction=="水泥砂浆基层","Chinese lost");});
    run("ConfirmedFactsOnly",[]{auto p=fixture(); p.config.zeroLevel.confirmed=false; p.config.views[0].material.confirmed=false; auto f=p.rules.semanticFeatures[0]; f.identity="候选"; f.confirmed=false; p.rules.semanticFeatures.push_back(f); const auto r=confirmedProject(p); check(r.canCommit()&&r.issues.size()>=3,"filter diagnostics"); check(r.value->rules.semanticFeatures.size()==1&&r.value->config.zeroLevel.value.empty()&&r.value->config.views[0].material.value.empty(),"candidate leaked");});
    run("CorruptNewSchemaAndTruncationRejected",[]{auto b=bytes(fixture()); auto changed=b; changed.back()^=1; check(!decodeProject(changed).readable(),"checksum accepted"); changed=b; changed[8]=99; check(!decodeProject(changed).readable(),"schema accepted"); for(std::size_t n=0;n<b.size();n+=17) check(!decodeProject(b.substr(0,n)).readable(),"truncated accepted"); check(!decodeProject(b+"x").readable(),"trailing accepted");});
    run("MalformedConfirmedRecordsRejected",[]{auto p=fixture(); p.rules.semanticFeatures[0].confirmationSource.clear(); check(!encodeProject(p).canCommit(),"missing source"); p=fixture(); p.rules.views[0].heightDirection.x=std::numeric_limits<double>::infinity(); check(!encodeProject(p).canCommit(),"nonfinite"); p=fixture(); p.generation.views[0].outputs[0].key.handle="0"; check(!encodeProject(p).canCommit(),"invalid handle"); p=fixture(); p.config.views.push_back(p.config.views[0]); check(!encodeProject(p).canCommit(),"duplicate view"); p=fixture(); p.generation.views[0].outputs.push_back(p.generation.views[0].outputs[0]); check(!encodeProject(p).canCommit(),"duplicate output"); p=fixture(); p.ruleInputsConfirmationSource.clear(); check(!encodeProject(p).canCommit(),"unconfirmed selection"); p=fixture(); p.persistentProjectId=std::string("bad\xC0\x80",5); check(!encodeProject(p).canCommit(),"invalid UTF8");});
    run("HistoricalOwnershipAndExplicitRebind",[]{const auto p=fixture(); const auto d=decodeProject(bytes(p)); check(d.readable(),"decode"); check(d.value->project.generation.databaseIdentity=="fixture-db"&&d.value->project.generation.views[0].requiresReconfirmation,"history lost"); auto o=observed(p); auto bound=bindProjectArchive(*d.value,p.persistentProjectId,o,views(p)); check(bound.canCommit()&&bound.value->bound,"exact rebind"); check(bound.value->project.generation.databaseIdentity==o.scene.databaseIdentity,"new runtime binding"); check(bound.value->project.generation.views[0].requiresReconfirmation,"reconfirmation cleared"); check(!bindProjectArchive(*d.value,"foreign-project",o,views(p)).canCommit(),"foreign project bound"); o.fingerprints[0].key.databaseIdentity="foreign-db"; check(!bindProjectArchive(*d.value,p.persistentProjectId,o,views(p)).canCommit(),"foreign key bound");});
    run("ManualChangeRetainsArchive",[]{auto p=fixture(); auto d=decodeProject(bytes(p)); check(d.readable(),"decode"); auto o=observed(p); o.fingerprints.back().geometry="manual edit"; const auto r=bindProjectArchive(*d.value,p.persistentProjectId,o,views(p)); check(!r.canCommit()&&r.value&&!r.value->bound,"manual edit authorized"); check(r.value->project.generation.views[0].outputs.size()==1,"ownership discarded"); bool found=false; for(const auto& i:r.issues) if(i.code=="MANUAL_CHANGE") found=true; check(found,"missing manual diagnostic");});
    run("EveryPersistedFieldAffectsBytes",[]{
        const auto baseline=bytes(fixture());
        const std::vector<std::function<void(StoredProject&)>> edits={
            [](StoredProject& p){p.persistentProjectId+="2";},
            [](StoredProject& p){p.config.units.insunits=6;},
            [](StoredProject& p){p.config.units.trustedInsunits=false;},
            [](StoredProject& p){p.config.units.confirmedMmPerInputUnit=1000.125;},
            [](StoredProject& p){p.config.units.labelHint+="2";},
            [](StoredProject& p){p.config.unitConfirmationSource+="2";},
            [](StoredProject& p){p.config.zeroLevel.value+="2";},
            [](StoredProject& p){p.config.zeroLevel.source+="2";},
            [](StoredProject& p){p.config.views[0].geometrySignature+="2";},
            [](StoredProject& p){p.config.views[0].material.value+="2";},
            [](StoredProject& p){p.config.views[0].material.source+="2";},
            [](StoredProject& p){p.config.views[0].axisMapping.value+="2";},
            [](StoredProject& p){p.config.views[0].axisMapping.source+="2";},
            [](StoredProject& p){p.config.views[0].axisDirection->x=-0.5;},
            [](StoredProject& p){p.ruleInputsConfirmationSource+="2";},
            [](StoredProject& p){p.rules.views[0].familyId+="2";},
            [](StoredProject& p){p.rules.views[0].scaleDenominator=101.25;},
            [](StoredProject& p){p.rules.views[0].datum->position.x=1.0;},
            [](StoredProject& p){p.rules.views[0].datum->position.y=0.0;},
            [](StoredProject& p){p.rules.views[0].datum->source.revision=8;},
            [](StoredProject& p){p.rules.views[0].datumConfirmationSource+="2";},
            [](StoredProject& p){p.rules.views[0].datumMeters=0.12345678901234567;},
            [](StoredProject& p){p.rules.views[0].heightDirection={0.8,0.6};},
            [](StoredProject& p){p.rules.views[0].requiredRoles.push_back(SemanticRole::ridge);},
            [](StoredProject& p){p.rules.views[0].requirePlanAxes=true;},
            [](StoredProject& p){p.rules.semanticFeatures[0].identity+="2";},
            [](StoredProject& p){p.rules.semanticFeatures[0].role=SemanticRole::opening;},
            [](StoredProject& p){p.rules.semanticFeatures[0].confirmationSource+="2";},
            [](StoredProject& p){p.rules.semanticFeatures[0].points[0].position.x=0.2;},
            [](StoredProject& p){p.rules.semanticFeatures[0].points[0].source.revision=8;},
            [](StoredProject& p){p.rules.semanticFeatures[0].confirmedRectangle=false;},
            [](StoredProject& p){p.rules.semanticFeatures[0].fromConfirmedPlan=false;},
            [](StoredProject& p){p.rules.semanticFeatures[0].region->outer.vertices[0].x=0.0;},
            [](StoredProject& p){p.rules.semanticFeatures[0].region->holes[0].vertices[0].y=20.125;},
            [](StoredProject& p){p.rules.semanticFeatures[0].region->holes[0].closed=false;},
            [](StoredProject& p){p.rules.semanticFeatures[0].sources[0].revision=8;},
            [](StoredProject& p){p.rules.semanticFeatures[0].materialKey+="2";},
            [](StoredProject& p){p.rules.semanticFeatures[0].materialPrototypeId+="2";},
            [](StoredProject& p){p.rules.semanticFeatures[0].tableViewIdentity+="2";},
            [](StoredProject& p){p.rules.semanticFeatures[0].sharedTableIdentity+="2";},
            [](StoredProject& p){p.rules.semanticFeatures[0].requestConstructionNote=false;},
            [](StoredProject& p){p.rules.semanticFeatures[0].construction+="2";},
            [](StoredProject& p){p.rules.semanticFeatures[0].detailId+="2";},
            [](StoredProject& p){p.rules.materialOverrides[0].key+="2";},
            [](StoredProject& p){p.rules.materialOverrides[0].number+="2";},
            [](StoredProject& p){p.rules.materialOverrides[0].name+="2";},
            [](StoredProject& p){p.rules.materialOverrides[0].prototypeId=std::nullopt;},
            [](StoredProject& p){p.generation.documentIdentity+="2";},
            [](StoredProject& p){p.generation.views[0].moduleIdentity+="2";},
            [](StoredProject& p){p.generation.views[0].moduleVersion+="2";},
            [](StoredProject& p){p.generation.views[0].generationId+="2";},
            [](StoredProject& p){p.generation.views[0].sourceSignature+="2";},
            [](StoredProject& p){p.generation.views[0].configSignature+="2";},
            [](StoredProject& p){p.generation.views[0].committedRevision=8;},
            [](StoredProject& p){p.generation.views[0].requiresReconfirmation=false;},
            [](StoredProject& p){p.generation.views[0].commitEvidence+="2";},
            [](StoredProject& p){p.generation.views[0].postCommitEvidence+="2";},
            [](StoredProject& p){p.generation.views[0].outputs[0].geometry+="2";},
            [](StoredProject& p){p.generation.views[0].outputs[0].text+="2";},
            [](StoredProject& p){p.generation.views[0].outputs[0].resources+="2";},
            [](StoredProject& p){p.generation.views[0].outputs[0].nativeClass+="2";},
            [](StoredProject& p){p.generation.views[0].outputs[0].key.revision=8;}
        };
        for(const auto& edit:edits){auto p=fixture();edit(p);const auto encoded=bytes(p);check(encoded!=baseline,"field omitted from codec");const auto decoded=decodeProject(encoded);check(decoded.readable()&&bytes(decoded.value->project)==encoded,"changed field roundtrip");}
    });
    run("ChangedSourceInvalidatesOnlyRelatedView",[]{auto p=fixture();p.config.views.push_back({"西立面","unchanged",{"砖","确认",true},{"轴","确认",true},Point2{1,0}});auto d=decodeProject(bytes(p));check(d.readable(),"decode");auto o=observed(p);o.fingerprints[0].geometry+="changed";const auto r=bindProjectArchive(*d.value,p.persistentProjectId,o,views(p));check(!r.canCommit()&&r.value,"changed source rejection");check(!r.value->project.config.views[0].material.confirmed&&r.value->project.config.views[1].material.confirmed,"unrelated view invalidated");check(r.value->project.generation.views.size()==1&&r.value->project.generation.views[0].requiresReconfirmation,"history lost");});
    run("MissingCurrentViewEvidenceStaysUnbound",[]{auto p=fixture();auto d=decodeProject(bytes(p));check(d.readable(),"decode");const auto r=bindProjectArchive(*d.value,p.persistentProjectId,observed(p),{});check(!r.canCommit()&&r.value&&!r.value->bound,"missing actual views authorized");});
    run("ViewMembershipCannotCrossDatabase",[]{const auto p=fixture();const auto d=decodeProject(bytes(p));check(d.readable(),"decode");auto v=views(p);v[0].members[0].databaseIdentity="foreign";check(!bindProjectArchive(*d.value,p.persistentProjectId,observed(p),v).canCommit(),"foreign view member authorized");v=views(p);v[0].members.clear();check(!bindProjectArchive(*d.value,p.persistentProjectId,observed(p),v).canCommit(),"empty observed view authorized");});
    run("DuplicateGenerationIdCannotEncode",[]{auto p=twoViewFixture();check(encodeProject(p).canCommit(),"valid two-view control");p.generation.views[1].generationId=p.generation.views[0].generationId;check(!encodeProject(p).canCommit(),"duplicate generationId encoded");});
    run("DuplicateGenerationIdCannotDecode",[]{const auto p=twoViewFixture();auto encoded=bytes(p);const auto& oldId=p.generation.views[1].generationId;const auto& duplicate=p.generation.views[0].generationId;check(oldId.size()==duplicate.size(),"same length test IDs required");const auto offset=encoded.find(oldId);check(offset!=std::string::npos&&encoded.find(oldId,offset+1)==std::string::npos,"unique second ID payload");encoded.replace(offset,oldId.size(),duplicate);refreshTestChecksum(encoded);check(!decodeProject(encoded).readable(),"valid-checksum duplicate generationId decoded");});
    run("DuplicateGenerationIdCannotBind",[]{auto p=twoViewFixture();const auto good=bindProjectArchive({p,false},p.persistentProjectId,observed(p),views(p));check(good.canCommit(),"valid two-view bind control");p.generation.views[1].generationId=p.generation.views[0].generationId;const auto bad=bindProjectArchive({p,false},p.persistentProjectId,observed(p),views(p));check(!bad.canCommit()&&bad.value&&!bad.value->bound,"duplicate generationId bound");});
    run("BoundsAndEnumRejected",[]{auto p=fixture(); p.rules.semanticFeatures[0].role=static_cast<SemanticRole>(99); check(!encodeProject(p).canCommit(),"enum"); p=fixture(); p.persistentProjectId=std::string(projectStoreMaxBytes+1,'a'); check(!encodeProject(p).canCommit(),"bounds");});
    std::cout<<"TOTAL_FAILURES="<<failures<<'\n'; return failures?1:0;
}

