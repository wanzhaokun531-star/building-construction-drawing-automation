#include "ProjectStoreCodec.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace bce {
namespace {
struct Invalid final:std::runtime_error { using std::runtime_error::runtime_error; };
void need(bool condition,const std::string& message) { if(!condition) throw Invalid(message); }
Issue issue(const std::string& code,const std::string& message,bool blocking=true) { return {code,message,{},blocking}; }
void utf8(const std::string& s) {
    need(s.size()<=262144,"string exceeds 262144 UTF8 bytes");
    std::size_t i=0;
    while(i<s.size()) {
        const auto c=static_cast<unsigned char>(s[i++]);
        need(c!=0,"embedded NUL"); if(c<128) continue;
        unsigned value{}, count{}, minimum{};
        if(c>=0xC2&&c<=0xDF) { value=c&31;count=1;minimum=128; }
        else if(c>=0xE0&&c<=0xEF) {value=c&15;count=2;minimum=2048;}
        else if(c>=0xF0&&c<=0xF4) {value=c&7;count=3;minimum=65536;}
        else throw Invalid("invalid UTF8 leading byte");
        need(s.size()-i>=count,"truncated UTF8");
        for(unsigned j=0;j<count;++j) {const auto n=static_cast<unsigned char>(s[i++]); need((n&0xC0)==0x80,"invalid UTF8 continuation"); value=(value<<6)|(n&63);}
        need(value>=minimum&&value<=0x10FFFF&&!(value>=0xD800&&value<=0xDFFF),"invalid UTF8 codepoint");
    }
}
std::string canonical(const std::string& h) {
    need(!h.empty()&&h.size()<=16&&h.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"invalid entity handle");
    const auto first=h.find_first_not_of('0'); need(first!=std::string::npos,"zero entity handle"); auto s=h.substr(first);
    for(auto& c:s) if(c>='a'&&c<='f') c=static_cast<char>(c-'a'+'A'); return s;
}
template<class A> void fields(A& a,Point2& v) {a(v.x,v.y);}
template<class A> void fields(A& a,EntityKey& v) {a(v.databaseIdentity,v.handle,v.revision);}
template<class A> void fields(A& a,EntityFingerprint& v) {a(v.key,v.geometry,v.text,v.resources,v.nativeClass);}
template<class A> void fields(A& a,UnitDeclaration& v) {a(v.insunits,v.trustedInsunits,v.confirmedMmPerInputUnit,v.labelHint);}
template<class A> void fields(A& a,Confirmation& v) {a(v.value,v.source,v.confirmed);}
template<class A> void fields(A& a,ViewConfig& v) {a(v.viewIdentity,v.geometrySignature,v.material,v.axisMapping,v.axisDirection);}
template<class A> void fields(A& a,ProjectConfig& v) {a(v.databaseIdentity,v.units,v.unitConfirmationSource,v.zeroLevel,v.views);}
template<class A> void fields(A& a,SourcePoint& v) {a(v.position,v.source);}
template<class A> void fields(A& a,ClosedRing& v) {a(v.vertices,v.closed);}
template<class A> void fields(A& a,PolygonRegion& v) {a(v.outer,v.holes);}
template<class A> void fields(A& a,SemanticFeature& v) {a(v.identity,v.viewIdentity,v.role,v.confirmed,v.confirmationSource,v.points,v.confirmedRectangle,v.fromConfirmedPlan,v.region,v.sources,v.materialKey,v.materialPrototypeId,v.tableViewIdentity,v.sharedTableIdentity,v.requestConstructionNote,v.construction,v.detailId);}
template<class A> void fields(A& a,RuleView& v) {a(v.viewIdentity,v.familyId,v.scaleDenominator,v.datum,v.datumConfirmationSource,v.datumMeters,v.heightDirection,v.requiredRoles,v.requirePlanAxes);}
template<class A> void fields(A& a,MaterialOverride& v) {a(v.key,v.number,v.name,v.prototypeId);}
template<class A> void fields(A& a,RuleInputs& v) {a(v.views,v.semanticFeatures,v.materialOverrides);}
template<class A> void fields(A& a,GeneratedViewState& v) {a(v.viewIdentity,v.moduleIdentity,v.moduleVersion,v.generationId,v.sourceSignature,v.configSignature,v.committedRevision,v.sourceEntities,v.outputs,v.requiresReconfirmation,v.commitEvidence,v.postCommitEvidence);}
template<class A> void fields(A& a,GenerationState& v) {a(v.documentIdentity,v.databaseIdentity,v.views);}
template<class A> void fields(A& a,StoredProject& v) {a(v.persistentProjectId,v.config,v.rules,v.ruleInputsConfirmationSource,v.generation,v.confirmedEntities);}
class Writer {
public:
    std::string bytes;
    template<class... T> void operator()(T&... values) {(one(values),...);}
    void integer(std::uint64_t v) { need(bytes.size()<=projectStoreMaxBytes-8,"archive size limit"); for(unsigned i=0;i<8;++i) bytes.push_back(static_cast<char>((v>>(i*8))&255)); }
private:
    void one(std::string& v) {utf8(v);integer(v.size());need(bytes.size()+v.size()<=projectStoreMaxBytes,"archive size limit");bytes+=v;}
    void one(bool& v) {integer(v?1:0);}
    void one(int& v) {integer(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));}
    void one(std::uint64_t& v) {integer(v);}
    void one(double& v) {need(std::isfinite(v),"nonfinite double");std::uint64_t bits{};static_assert(sizeof(bits)==sizeof(v),"IEEE binary64 required");std::memcpy(&bits,&v,8);integer(bits);}
    template<class T> void one(std::vector<T>& v) {need(v.size()<=projectStoreMaxItems,"collection size limit");integer(v.size());for(auto& e:v) one(e);}
    template<class T> void one(std::optional<T>& v) {bool present=v.has_value();one(present);if(v) one(*v);}
    template<class T> void one(T& v) {if constexpr(std::is_enum_v<T>) integer(static_cast<std::uint64_t>(v));else fields(*this,v);}
};
class Reader {
public:
    explicit Reader(const std::string& bytes):bytes_(bytes) {need(bytes.size()<=projectStoreMaxBytes,"archive size limit");}
    template<class... T> void operator()(T&... values) {(one(values),...);}
    std::uint64_t integer() {need(bytes_.size()-position_>=8,"truncated integer at "+std::to_string(position_));std::uint64_t v{};for(unsigned i=0;i<8;++i) v|=static_cast<std::uint64_t>(static_cast<unsigned char>(bytes_[position_++]))<<(8*i);return v;}
    bool done() const {return position_==bytes_.size();}
private:
    void one(std::string& v) {const auto n=integer();need(n<=262144&&n<=bytes_.size()-position_,"invalid string length at "+std::to_string(position_));v=bytes_.substr(position_,static_cast<std::size_t>(n));position_+=static_cast<std::size_t>(n);utf8(v);}
    void one(bool& v) {const auto n=integer();need(n<=1,"invalid bool");v=n!=0;}
    void one(int& v) {const auto u=integer();std::int64_t n{};std::memcpy(&n,&u,8);need(n>=(std::numeric_limits<int>::min)()&&n<=(std::numeric_limits<int>::max)(),"invalid int range");v=static_cast<int>(n);}
    void one(std::uint64_t& v) {v=integer();}
    void one(double& v) {const auto bits=integer();std::memcpy(&v,&bits,8);need(std::isfinite(v),"nonfinite double");}
    template<class T> void one(std::vector<T>& v) {const auto n=integer();need(n<=projectStoreMaxItems&&n<=(bytes_.size()-position_)/8,"invalid collection count");v.resize(static_cast<std::size_t>(n));for(auto& e:v) one(e);}
    template<class T> void one(std::optional<T>& v) {bool present{};one(present);if(present){v.emplace();one(*v);}else v.reset();}
    template<class T> void one(T& v) {if constexpr(std::is_enum_v<T>){const auto n=integer();need(n<=static_cast<std::uint64_t>((std::numeric_limits<int>::max)()),"invalid enum range");v=static_cast<T>(n);}else fields(*this,v);}
    const std::string& bytes_;std::size_t position_{};
};
void required(const std::string& s,const char* name) {need(!s.empty(),std::string("missing ")+name);}
void key(const EntityKey& k,const std::string& database) {need(k.databaseIdentity==database,"cross database entity key");canonical(k.handle);}
void fingerprint(const EntityFingerprint& f,const std::string& db) {key(f.key,db);required(f.geometry,"geometry fingerprint");required(f.text,"text fingerprint");required(f.resources,"resource fingerprint");required(f.nativeClass,"native class");}
void confirmation(const Confirmation& c) {if(c.confirmed){required(c.value,"confirmed value");required(c.source,"confirmation source");}else need(c.value.empty()&&c.source.empty(),"unconfirmed value in archive");}
void role(SemanticRole r) {need(r>=SemanticRole::bodyContour&&r<=SemanticRole::materialRegion,"invalid SemanticRole");}
void validate(const StoredProject& p) {
    required(p.persistentProjectId,"persistent project ID");required(p.config.databaseIdentity,"archive database identity");
    const auto& db=p.config.databaseIdentity;
    need(p.config.units.insunits>=0&&p.config.units.insunits<=24,"invalid INSUNITS");
    if(p.config.units.confirmedMmPerInputUnit||p.config.units.trustedInsunits) required(p.config.unitConfirmationSource,"unit confirmation source");
    if(p.config.units.confirmedMmPerInputUnit) need(*p.config.units.confirmedMmPerInputUnit>0,"invalid unit scale");
    confirmation(p.config.zeroLevel);
    std::set<std::string> views,rules,features,materials,outputs;
    for(const auto& v:p.config.views){required(v.viewIdentity,"view identity");required(v.geometrySignature,"view geometry signature");need(views.insert(v.viewIdentity).second,"duplicate config view");confirmation(v.material);confirmation(v.axisMapping);if(v.axisDirection) need(v.axisMapping.confirmed&&std::hypot(v.axisDirection->x,v.axisDirection->y)>0,"unconfirmed/zero axis direction");}
    if(!p.rules.views.empty()||!p.rules.semanticFeatures.empty()||!p.rules.materialOverrides.empty()) required(p.ruleInputsConfirmationSource,"rule selection confirmation source");
    for(const auto& v:p.rules.views){need(views.count(v.viewIdentity)!=0&&rules.insert(v.viewIdentity).second,"missing/duplicate rule view");required(v.familyId,"selected family");need(v.scaleDenominator>0,"invalid selected scale");need(std::hypot(v.heightDirection.x,v.heightDirection.y)>0,"zero height direction");for(const auto r:v.requiredRoles)role(r);if(v.datum){required(v.datumConfirmationSource,"datum confirmation source");key(v.datum->source,db);}else need(v.datumConfirmationSource.empty(),"datum provenance without datum");}
    for(const auto& f:p.rules.semanticFeatures){required(f.identity,"feature identity");need(features.insert(f.identity).second&&views.count(f.viewIdentity)!=0,"duplicate feature/missing view");need(f.confirmed,"unconfirmed semantic in archive");required(f.confirmationSource,"semantic confirmation source");role(f.role);for(const auto& q:f.points)key(q.source,db);for(const auto& k:f.sources)key(k,db);need(!f.points.empty()||!f.sources.empty(),"semantic sources missing");}
    for(const auto& m:p.rules.materialOverrides){required(m.key,"material override key");need(materials.insert(m.key).second,"duplicate material override");required(m.number,"material number");required(m.name,"material name");if(m.prototypeId)required(*m.prototypeId,"material prototype");}
    std::map<std::string,EntityFingerprint> all;
    const auto insert=[&](const EntityFingerprint& f){fingerprint(f,db);const auto h=canonical(f.key.handle);const auto it=all.find(h);if(it!=all.end()) need(it->second.geometry==f.geometry&&it->second.text==f.text&&it->second.resources==f.resources&&it->second.nativeClass==f.nativeClass,"inconsistent historical fingerprint");else all.emplace(h,f);};
    std::set<std::string> confirmed;
    for(const auto& f:p.confirmedEntities){need(confirmed.insert(canonical(f.key.handle)).second,"duplicate confirmed entity");insert(f);}
    std::set<std::string> generatedViews,generationIds;
    if(!p.generation.views.empty()){required(p.generation.documentIdentity,"historical document identity");need(p.generation.databaseIdentity==db,"generation cross database");}
    else need(p.generation.databaseIdentity.empty()||p.generation.databaseIdentity==db,"generation cross database");
    for(const auto& g:p.generation.views){need(views.count(g.viewIdentity)!=0&&generatedViews.insert(g.viewIdentity).second,"duplicate/missing generated view");required(g.moduleIdentity,"module identity");required(g.moduleVersion,"module version");required(g.generationId,"generation ID");need(generationIds.insert(g.generationId).second,"duplicate generation ID");required(g.sourceSignature,"source signature");required(g.configSignature,"config signature");required(g.commitEvidence,"commit evidence");required(g.postCommitEvidence,"postcommit evidence");need(!g.outputs.empty()&&!g.sourceEntities.empty(),"missing historical source/output");std::set<std::string> sources;for(const auto& f:g.sourceEntities){need(sources.insert(canonical(f.key.handle)).second,"duplicate source");insert(f);}for(const auto& f:g.outputs){const auto h=canonical(f.key.handle);need(!sources.count(h)&&outputs.insert(h).second,"duplicate/overlapping output");insert(f);}}
    for(const auto& v:p.rules.views)if(v.datum)need(all.count(canonical(v.datum->source.handle))!=0,"datum full fingerprint missing");
    for(const auto& f:p.rules.semanticFeatures){for(const auto& q:f.points)need(all.count(canonical(q.source.handle))!=0,"point full fingerprint missing");for(const auto& k:f.sources)need(all.count(canonical(k.handle))!=0,"semantic full fingerprint missing");}
}
std::uint32_t crc(const std::string& s) {std::uint32_t c=0xFFFFFFFF;for(const auto b:s){c^=static_cast<unsigned char>(b);for(int n=0;n<8;++n)c=(c>>1)^(0xEDB88320u&static_cast<std::uint32_t>(-static_cast<std::int32_t>(c&1)));}return ~c;}
bool same(const EntityFingerprint& a,const EntityFingerprint& b) {return a.geometry==b.geometry&&a.text==b.text&&a.resources==b.resources&&a.nativeClass==b.nativeClass;}
}
Result<StoredProject> confirmedProject(const StoredProject& input) {
    Result<StoredProject> result{input,{}};
    try {
        auto& p=*result.value;
        const auto filter=[&](Confirmation& c,const std::string& field){if(!c.confirmed&&(!c.value.empty()||!c.source.empty())){c={};result.issues.push_back(issue("UNCONFIRMED_FILTERED",field,false));}};
        filter(p.config.zeroLevel,"zeroLevel");
        for(auto& v:p.config.views){filter(v.material,v.viewIdentity+" material");filter(v.axisMapping,v.viewIdentity+" axisMapping");if(!v.axisMapping.confirmed&&v.axisDirection){v.axisDirection.reset();result.issues.push_back(issue("UNCONFIRMED_FILTERED",v.viewIdentity+" axisDirection",false));}}
        auto& features=p.rules.semanticFeatures;
        features.erase(std::remove_if(features.begin(),features.end(),[&](const SemanticFeature& f){if(f.confirmed)return false;result.issues.push_back(issue("UNCONFIRMED_FILTERED","semantic "+f.identity,false));return true;}),features.end());
        validate(p);Writer validator;validator(p);
    }catch(const std::exception& e){result.value.reset();result.issues.push_back(issue("PROJECT_INVALID",e.what()));}
    return result;
}
Result<std::string> encodeProject(const StoredProject& input) {
    auto checked=confirmedProject(input);if(!checked.canCommit())return {std::nullopt,checked.issues};
    try {Writer payload;payload(*checked.value);Writer header;std::uint64_t version=1;header(version);header.integer(payload.bytes.size());std::string bytes="BCEPROJ1"+header.bytes+payload.bytes;Writer checksum;checksum.integer(crc(bytes));bytes+=checksum.bytes;need(bytes.size()<=projectStoreMaxBytes,"archive total size limit");return {std::move(bytes),checked.issues};}
    catch(const std::exception& e){return {std::nullopt,{issue("PROJECT_ENCODE_FAILED",e.what())}};}
}
Result<ProjectArchive> decodeProject(const std::string& bytes) {
    try {need(bytes.size()>=32&&bytes.size()<=projectStoreMaxBytes,"invalid archive size");need(bytes.substr(0,8)=="BCEPROJ1","foreign archive magic");const auto hs=bytes.substr(8,16);Reader header(hs);need(header.integer()==1,"unsupported project schema");const auto size=header.integer();need(size==bytes.size()-32,"payload length mismatch");const auto cs=bytes.substr(bytes.size()-8);Reader checksum(cs);need(checksum.integer()==crc(bytes.substr(0,bytes.size()-8)),"checksum mismatch");const auto data=bytes.substr(24,static_cast<std::size_t>(size));Reader reader(data);ProjectArchive archive;reader(archive.project);need(reader.done(),"trailing payload bytes");validate(archive.project);return {std::move(archive),{}};}
    catch(const std::exception& e){return {std::nullopt,{issue("PROJECT_DECODE_FAILED",e.what())}};}
}
Result<ProjectArchive> bindProjectArchive(const ProjectArchive& archive,const std::string& currentProjectId,const ObservedScene& observed,const std::vector<View>& currentViews) {
    Result<ProjectArchive> result{archive,{}};result.value->bound=false;
    try {
        const auto& p=archive.project;validate(p);
        need(!archive.bound,"bind requires an unbound archive");need(currentProjectId==p.persistentProjectId,"persistent project mismatch");
        const auto& scene=observed.scene;required(scene.documentIdentity,"current document identity");required(scene.databaseIdentity,"current database identity");
        std::set<std::string> roots;for(const auto* list:{&scene.targets,&scene.preserved})for(const auto& e:*list){key(e.key,scene.databaseIdentity);need(e.key.revision==scene.revision&&roots.insert(canonical(e.key.handle)).second,"duplicate/stale root");}
        std::map<std::string,EntityFingerprint> actual;for(const auto& f:observed.fingerprints){fingerprint(f,scene.databaseIdentity);need(f.key.revision==scene.revision&&actual.emplace(canonical(f.key.handle),f).second,"duplicate/stale observed fingerprint");}
        need(roots.size()==actual.size(),"incomplete observed root inventory");for(const auto& h:roots)need(actual.count(h)!=0,"missing root fingerprint");
        std::set<std::string> changedSources;
        std::map<std::string,std::string> viewSignatures;
        for(const auto& v:currentViews){required(v.identity,"observed view identity");required(v.geometrySignature,"observed view signature");need(viewSignatures.emplace(v.identity,v.geometrySignature).second,"duplicate observed view");need(!v.members.empty(),"observed view has no root members");std::set<std::string> members;for(const auto& k:v.members){key(k,scene.databaseIdentity);const auto h=canonical(k.handle);need(k.revision==scene.revision&&roots.count(h)!=0&&members.insert(h).second,"missing/stale/duplicate observed view root");}}
        for(const auto& v:p.config.views){const auto found=viewSignatures.find(v.viewIdentity);if(found==viewSignatures.end()||found->second!=v.geometrySignature)changedSources.insert(v.viewIdentity);}
        const auto matches=[&](const EntityFingerprint& f){const auto it=actual.find(canonical(f.key.handle));return it!=actual.end()&&same(f,it->second);};
        for(auto& g:result.value->project.generation.views){for(const auto& f:g.outputs)if(!matches(f)) result.issues.push_back({"MANUAL_CHANGE","Stored output missing or edited",{scene.documentIdentity,g.viewIdentity,f.key},true});for(const auto& f:g.sourceEntities)if(!matches(f)){g.requiresReconfirmation=true;changedSources.insert(g.viewIdentity);}}
        for(const auto& f:p.confirmedEntities)if(!matches(f)){for(const auto& v:p.rules.views)if(v.datum&&canonical(v.datum->source.handle)==canonical(f.key.handle))changedSources.insert(v.viewIdentity);for(const auto& feature:p.rules.semanticFeatures){for(const auto& q:feature.points)if(canonical(q.source.handle)==canonical(f.key.handle))changedSources.insert(feature.viewIdentity);for(const auto& k:feature.sources)if(canonical(k.handle)==canonical(f.key.handle))changedSources.insert(feature.viewIdentity);}result.issues.push_back({"SOURCE_CHANGED","Confirmed source fingerprint changed",{scene.documentIdentity,"",f.key},true});}
        for(auto& g:result.value->project.generation.views)if(changedSources.count(g.viewIdentity))g.requiresReconfirmation=true;
        for(auto& v:result.value->project.config.views)if(changedSources.count(v.viewIdentity)){v.material={};v.axisMapping={};v.axisDirection.reset();result.issues.push_back({"VIEW_CONFIG_CHANGED","Reconfirmation required",{scene.documentIdentity,v.viewIdentity,{}},true});}
        if(!result.issues.empty())return result;
        auto& bound=result.value->project;const auto rekey=[&](EntityKey& k){k.databaseIdentity=scene.databaseIdentity;k.revision=scene.revision;};
        bound.config.databaseIdentity=scene.databaseIdentity;bound.generation.documentIdentity=scene.documentIdentity;bound.generation.databaseIdentity=scene.databaseIdentity;
        for(auto& v:bound.rules.views)if(v.datum)rekey(v.datum->source);
        for(auto& f:bound.rules.semanticFeatures){for(auto& q:f.points)rekey(q.source);for(auto& k:f.sources)rekey(k);}
        for(auto& f:bound.confirmedEntities)rekey(f.key);
        for(auto& g:bound.generation.views){for(auto& f:g.sourceEntities)rekey(f.key);for(auto& f:g.outputs)rekey(f.key);}
        result.value->bound=true;
    }catch(const std::exception& e){result.issues.push_back(issue("PROJECT_BINDING_REJECTED",e.what()));}
    return result;
}
}
