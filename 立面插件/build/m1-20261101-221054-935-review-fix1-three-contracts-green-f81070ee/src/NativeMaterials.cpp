#include "NativeMaterials.h"
#include "MaterialRules.h"
#include <Windows.h>
#include <wincrypt.h>
#include <dbHatch.h>
#include <dbapserv.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <acstring.h>
#include <acdocman.h>
#include <cmath>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
namespace bce {
namespace {
void need(bool v,const std::string& m){if(!v)throw std::runtime_error(m);}
void check(Acad::ErrorStatus s,const char* m){need(s==Acad::eOk,std::string(m)+":"+std::to_string(s));}
template<class T> class Open final {
public:
    explicit Open(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead){check(acdbOpenObject(p_,id,mode),"open");}
    ~Open(){if(p_)p_->close();}
    Open(const Open&)=delete;Open& operator=(const Open&)=delete;
    T* operator->() const{return p_;} T& operator*() const{return *p_;}
private:T* p_{};
};
class Working final {
public:
    explicit Working(AcDbDatabase& db):services_(acdbHostApplicationServices()),old_(services_->workingDatabase()){services_->setWorkingDatabase(&db);}
    ~Working() noexcept {services_->setWorkingDatabase(old_);}
    Working(const Working&)=delete;Working& operator=(const Working&)=delete;
private:AcDbHostApplicationServices* services_;AcDbDatabase* old_;
};
bool finite(Point2 p){return std::isfinite(p.x)&&std::isfinite(p.y);}
bool positive(double n){return std::isfinite(n)&&n>0;}
std::string fileSha(HANDLE file){
    struct Crypto {HCRYPTPROV provider{};HCRYPTHASH hash{};~Crypto(){if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);}} c;
    need(CryptAcquireContextW(&c.provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=FALSE,"hash provider");
    need(CryptCreateHash(c.provider,CALG_SHA_256,0,0,&c.hash)!=FALSE,"hash create");
    BYTE buffer[65536];DWORD size{};
    do{need(ReadFile(file,buffer,sizeof(buffer),&size,nullptr)!=FALSE,"reference hash read");if(size)need(CryptHashData(c.hash,buffer,size,0)!=FALSE,"hash data");}while(size);
    BYTE digest[32];size=sizeof(digest);need(CryptGetHashParam(c.hash,HP_HASHVAL,digest,&size,0)!=FALSE&&size==32,"hash result");
    const char* hex="0123456789ABCDEF";std::string result;for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
std::string upper(std::string s){for(auto& c:s)if(c>='a'&&c<='f')c=static_cast<char>(c-'a'+'A');return s;}
std::wstring wide(const std::string& s){
    need(!s.empty()&&s.find('\0')==std::string::npos,"invalid UTF8 token");
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),-1,nullptr,0);need(n>1,"invalid UTF8");
    std::wstring out(static_cast<std::size_t>(n),L'\0');need(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.c_str(),-1,&out[0],n)==n,"UTF8 decode");out.pop_back();return out;
}
AcDbObjectId objectId(const ReferenceMaterialSeed& seed){
    need(!seed.handle.empty()&&seed.handle.size()<=16&&std::all_of(seed.handle.begin(),seed.handle.end(),[](char c){return (c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f');}),"invalid prototype handle");
    AcDbObjectId id;check(seed.database->getAcDbObjectId(id,false,AcDbHandle(wide(seed.handle).c_str())),"prototype handle");return id;
}
AcDbObjectIdArray roots(AcDbDatabase& db){
    Open<AcDbBlockTableRecord> model(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};check(model->newIterator(raw),"roots iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);AcDbObjectIdArray ids;
    for(;!it->done();it->step()){AcDbObjectId id;check(it->getEntityId(id),"root id");ids.append(id);}return ids;
}
AcDbObjectId clone(AcDbDatabase& from,AcDbObjectId id,AcDbDatabase& to){
    AcDbObjectIdArray ids;ids.append(id);AcDbIdMapping map;check(from.wblockCloneObjects(ids,acdbSymUtil()->blockModelSpaceId(&to),map,AcDb::kDrcMangleName),"clone hatch dependencies");
    AcDbIdPair pair;pair.setKey(id);need(map.compute(pair)&&pair.isCloned()&&!pair.value().isNull(),"clone mapping");return pair.value();
}
AcDbObjectId cloneVerifiedReference(const ReferenceMaterialSeed& seed,const std::string& expectedSha,AcDbDatabase& target){
    const ACHAR* filename{};check(seed.database->getFilename(filename),"reference filename");need(filename&&*filename,"REFERENCE_FILE_REQUIRED");
    const std::wstring path(filename);
    // Pin the exact file against writes/deletion throughout hashing, loading and dependency cloning.
    struct File {HANDLE h;~File(){if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}} file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
    need(file.h!=INVALID_HANDLE_VALUE,"reference hash open");need(fileSha(file.h)==expectedSha,"REFERENCE_SHA_MISMATCH");
    AcDbDatabase reference(false,true);AcDbObjectId sourceId;
    {
        // Loading fonts/symbols must resolve against this owned file database, never ambient.
        Working work(reference);check(reference.readDwgFile(path.c_str(),AcDbDatabase::kForReadAndReadShare,false),"read verified reference");check(reference.closeInput(true),"close verified reference input");
        auto fileSeed=seed;fileSeed.database=&reference;sourceId=objectId(fileSeed);
        Open<AcDbEntity> entity(sourceId);need(entity->isA()==AcDbHatch::desc(),"UNSUPPORTED_MATERIAL_NATIVE_TYPE");
    }
    // The prototype and all wblockCloneObjects dependencies come from these verified file bytes.
    return clone(reference,sourceId,target);
}
struct Canonical {
    std::ostringstream out;
    Canonical(){out.imbue(std::locale::classic());out<<std::hexfloat;}
    void number(double v){need(std::isfinite(v),"nonfinite native expression");out<<v<<';';}
    void text(const ACHAR* s){need(s!=nullptr,"null native string");out<<'[';while(*s)out<<static_cast<unsigned>(*s++)<<',';out<<']';}
    void id(AcDbObjectId v){if(v.isNull()){out<<"null;";return;}AcDbHandle h=v.handle();ACHAR chars[32]{};h.getIntoAsciiBuffer(chars);text(chars);}
    void point(AcGePoint2d p){number(p.x);number(p.y);}
    void vector(AcGeVector3d p){number(p.x);number(p.y);number(p.z);}
};
void linetype(Canonical& c,AcDbObjectId id){
    c.id(id);Open<AcDbLinetypeTableRecord> r(id);AcString name;check(r->getName(name),"linetype name");c.text(name.kACharPtr());c.number(r->patternLength());c.number(r->numDashes());
    for(int i=0;i<r->numDashes();++i){c.number(r->dashLengthAt(i));c.id(r->shapeStyleAt(i));need(r->shapeStyleAt(i).isNull(),"UNSUPPORTED_COMPLEX_LINETYPE");c.number(r->shapeNumberAt(i));c.number(r->shapeScaleAt(i));c.number(r->shapeRotationAt(i));const auto offset=r->shapeOffsetAt(i);c.number(offset.x);c.number(offset.y);c.number(r->shapeIsUcsOrientedAt(i));c.number(r->shapeIsUprightAt(i));AcString text;check(r->textAt(i,text),"dash text");c.text(text.kACharPtr());}
}
void appearance(Canonical& c,AcDbHatch& h){
    c.id(h.layerId());c.number(h.color().color());c.number(h.color().colorMethod());c.number(h.transparency().serializeOut());c.number(h.lineWeight());c.number(h.linetypeScale());c.number(h.visibility());c.id(h.materialId());AcDbObjectId plotStyle;c.number(h.getPlotStyleNameId(plotStyle));c.id(plotStyle);
    linetype(c,h.linetypeId());Open<AcDbLayerTableRecord> layer(h.layerId());AcString name;check(layer->getName(name),"layer name");c.text(name.kACharPtr());c.number(layer->color().color());c.number(layer->color().colorMethod());c.number(layer->transparency().serializeOut());c.number(layer->lineWeight());c.number(layer->isOff());c.number(layer->isFrozen());c.number(layer->isLocked());c.number(layer->isPlottable());c.id(layer->materialId());c.id(layer->plotStyleNameId());linetype(c,layer->linetypeObjectId());
}
void pattern(Canonical& c,AcDbHatch& h){
    need(h.isA()==AcDbHatch::desc()&&!h.isGradient()&&h.isHatch(),"UNSUPPORTED_MATERIAL_NATIVE_TYPE");
    need(h.normal()==AcGeVector3d::kZAxis&&h.elevation()==0,"UNSUPPORTED_HATCH_PLANE");
    need(h.patternType()==AcDbHatch::kUserDefined||h.patternType()==AcDbHatch::kPreDefined||h.patternType()==AcDbHatch::kCustomDefined,"UNKNOWN_PATTERN_TYPE");
    c.vector(h.normal());c.number(h.elevation());c.text(h.patternName());c.number(h.patternType());c.number(h.patternAngle());c.number(h.patternScale());c.number(h.patternSpace());c.number(h.patternDouble());c.number(h.backgroundColor().color());c.number(h.backgroundColor().colorMethod());c.point(h.originPoint());c.number(h.hatchStyle());c.number(h.isSolidFill());c.number(h.numPatternDefinitions());
    need(positive(h.patternScale())&&(h.isSolidFill()||h.numPatternDefinitions()>0),"MISSING_PATTERN_DEFINITION");
    need(h.numPatternDefinitions()<=4096,"PATTERN_BUDGET");
    for(int i=0;i<h.numPatternDefinitions();++i){double a{},x{},y{},dx{},dy{};AcGeDoubleArray d;check(h.getPatternDefinitionAt(i,a,x,y,dx,dy,d),"read pattern");c.number(a);c.number(x);c.number(y);c.number(dx);c.number(dy);need(d.length()<=4096,"DASH_BUDGET");c.number(d.length());for(int j=0;j<d.length();++j)c.number(d[j]);}
}
std::string snapshot(AcDbDatabase& db,const AcDbObjectIdArray& ids){
    const auto actual=roots(db);need(actual.length()==ids.length(),"PREPARED_ROOT_SET_CHANGED");Canonical c;
    for(int n=0;n<ids.length();++n){need(actual.contains(ids[n])&&!ids[n].isErased(),"PREPARED_ROOT_CHANGED");Open<AcDbHatch> h(ids[n]);pattern(c,*h);appearance(c,*h);c.id(ids[n]);c.id(h->ownerId());c.id(h->extensionDictionary());need(h->extensionDictionary().isNull(),"UNSUPPORTED_HATCH_EXTENSION");c.number(h->associative());need(!h->associative(),"OLD_ASSOCIATION");AcDbObjectIdArray assoc;check(h->getAssocObjIds(assoc),"read associations");need(assoc.isEmpty(),"OLD_BOUNDARY_ASSOCIATION");c.number(h->numLoops());
        for(int j=0;j<h->numLoops();++j){Adesk::Int32 type=h->loopTypeAt(j);need((type&AcDbHatch::kPolyline)!=0,"PREPARED_LOOP_TYPE_CHANGED");AcGePoint2dArray p;AcGeDoubleArray b;check(h->getLoopAt(j,type,p,b),"read prepared loops");c.number(type);c.number(p.length());c.number(b.length());for(int k=0;k<p.length();++k)c.point(p[k]);for(int k=0;k<b.length();++k)c.number(b[k]);}
    }return c.out.str();
}
void validateRegion(const MaterialPlan& plan,const MaterialRegionPlan& region){
    need(positive(region.areaSquareMm),"INVALID_MATERIAL_PLAN_AREA");
    need(!region.confirmationSource.empty()&&!region.prototypeApprovalSource.empty()&&!region.familyId.empty()&&!region.sources.empty(),"MATERIAL_CONFIRMATION_REQUIRED");
    FeatureModel model;SemanticFeature f;f.identity=region.identity;f.viewIdentity=region.viewIdentity;f.role=SemanticRole::materialRegion;f.confirmed=true;f.confirmationSource=region.confirmationSource;f.region=region.geometry;f.sources=region.sources;f.tableViewIdentity=region.tableViewIdentity;f.sharedTableIdentity=region.sharedTableIdentity;f.materialKey="adapter-explicit";f.materialPrototypeId=region.expression.prototypeId;model.rules.semanticFeatures.push_back(f);
    RuleView view;view.viewIdentity=region.viewIdentity;view.familyId=region.familyId;model.rules.views.push_back(view);model.rules.materialOverrides.push_back({"adapter-explicit",region.materialNumber,region.materialName,region.expression.prototypeId});
    ReferenceStandard standard;standard.referenceSha256=plan.referenceSha256;ReferenceFamily family;family.identity=region.familyId;family.approved=true;family.approvalSource=region.prototypeApprovalSource;family.roles={SemanticRole::materialRegion};standard.families.push_back(family);standard.materialPrototypes.push_back({region.expression,true,region.prototypeApprovalSource});
    ProjectConfig config;config.databaseIdentity=region.sources.front().databaseIdentity;config.units=plan.unitDeclaration;config.unitConfirmationSource=plan.unitConfirmationSource;
    auto validated=planMaterials(model,standard,config);need(validated.canCommit()&&validated.value->regions.size()==1,validated.issues.empty()?"INVALID_MATERIAL_PLAN":validated.issues.front().code);
    const auto recomputedArea=validated.value->regions[0].areaSquareMm;need(positive(recomputedArea),"INVALID_DERIVED_MATERIAL_AREA");
    need(validated.value->mmPerInputUnit==plan.mmPerInputUnit&&std::abs(recomputedArea-region.areaSquareMm)<=1e-8*(std::max)(1.0,region.areaSquareMm),"MATERIAL_PLAN_FACTS_CHANGED");
}
void appendRing(AcDbHatch& hatch,const ClosedRing& ring,bool outer){
    AcGePoint2dArray points;AcGeDoubleArray bulges;
    for(const auto p:ring.vertices){points.append({p.x,p.y});bulges.append(0);}
    if(points.first()!=points.last()){points.append(points.first());bulges.append(0);}
    check(hatch.appendLoop(AcDbHatch::kPolyline|(outer?AcDbHatch::kExternal:AcDbHatch::kDefault),points,bulges),"append closed material loop");
}
}
struct PreparedMaterialBatch::State : AcDbDatabaseReactor {
    std::unique_ptr<AcDbDatabase> database;
    AcDbObjectIdArray roots;
    std::string baseline;
    std::set<AcDbDatabase*> sources;
    bool armed{}, changed{};
    ~State(){if(armed&&database)database->removeReactor(this);}
    void objectAppended(const AcDbDatabase*,const AcDbObject*) override {changed=true;}
    void objectUnAppended(const AcDbDatabase*,const AcDbObject*) override {changed=true;}
    void objectReAppended(const AcDbDatabase*,const AcDbObject*) override {changed=true;}
    void objectOpenedForModify(const AcDbDatabase*,const AcDbObject*) override {changed=true;}
    void objectModified(const AcDbDatabase*,const AcDbObject*) override {changed=true;}
    void objectErased(const AcDbDatabase*,const AcDbObject*,Adesk::Boolean) override {changed=true;}
    void headerSysVarWillChange(const AcDbDatabase*,const ACHAR*) override {changed=true;}
};
PreparedMaterialBatch::PreparedMaterialBatch(std::unique_ptr<State> s):state_(std::move(s)){}
PreparedMaterialBatch::~PreparedMaterialBatch()=default;
PreparedMaterialBatch::PreparedMaterialBatch(PreparedMaterialBatch&&) noexcept=default;
PreparedMaterialBatch& PreparedMaterialBatch::operator=(PreparedMaterialBatch&&) noexcept=default;
AcDbDatabase& PreparedMaterialBatch::database() const {need(state_!=nullptr,"moved material batch");return *state_->database;}
const AcDbObjectIdArray& PreparedMaterialBatch::roots() const {need(state_!=nullptr,"moved material batch");return state_->roots;}
bool PreparedMaterialBatch::intact() const noexcept {try{if(!state_||state_->changed)return false;Working work(*state_->database);return snapshot(*state_->database,state_->roots)==state_->baseline;}catch(...){return false;}}
Result<std::vector<AcDbObjectId>> PreparedMaterialBatch::cloneToEmpty(AcDbDatabase& target) const {
    try{
        need(state_&&&target!=state_->database.get()&&state_->sources.count(&target)==0&&&target!=acdbHostApplicationServices()->workingDatabase(),"INDEPENDENT_TARGET_REQUIRED");
        need(acDocManager&&acDocManager->document(&target)==nullptr,"INDEPENDENT_TARGET_REQUIRED");
        need(intact(),"PREPARED_MATERIAL_CHANGED");Working work(target);need(::bce::roots(target).isEmpty(),"EMPTY_TARGET_REQUIRED");
        AcDbIdMapping mapping;check(state_->database->wblockCloneObjects(state_->roots,acdbSymUtil()->blockModelSpaceId(&target),mapping,AcDb::kDrcMangleName),"clone prepared roots");
        std::vector<AcDbObjectId> ids;for(int i=0;i<state_->roots.length();++i){AcDbIdPair pair;pair.setKey(state_->roots[i]);need(mapping.compute(pair)&&pair.isCloned(),"output mapping");ids.push_back(pair.value());}
        need(::bce::roots(target).length()==static_cast<int>(ids.size()),"OLD_REFERENCE_ROOT_LEAK");return {std::move(ids),{}};
    }catch(const std::exception& e){return {std::nullopt,{{"MATERIAL_CLONE_REJECTED",e.what(),{},true}}};}
}
Result<PreparedMaterialBatch> prepareMaterials(const MaterialPlan& plan,const std::vector<ReferenceMaterialSeed>& seeds,const MaterialPreparationOptions& options){
    std::string location;
    try {
        need(positive(plan.mmPerInputUnit)&&!plan.unitConfirmationSource.empty()&&!plan.regions.empty(),"MATERIAL_FACTS_REQUIRED");
        const auto squareUnit=plan.mmPerInputUnit*plan.mmPerInputUnit;need(positive(squareUnit),"INVALID_MATERIAL_AREA_UNIT_SQUARE");
        need(options.placements.size()==plan.regions.size(),"EXPLICIT_MATERIAL_PLACEMENT_REQUIRED");
        auto state=std::make_unique<PreparedMaterialBatch::State>();state->database=std::make_unique<AcDbDatabase>(true,true);Working work(*state->database);
        std::set<std::pair<std::string,std::string>> seen;
        for(const auto& region:plan.regions){
            location=region.viewIdentity+"/"+region.identity;need(seen.emplace(region.viewIdentity,region.identity).second,"DUPLICATE_MATERIAL_REGION");
            need(region.expression.kind==MaterialExpressionKind::hatch,"UNSUPPORTED_MATERIAL_EXPRESSION");validateRegion(plan,region);
            const auto expectedArea=region.areaSquareMm/squareUnit;need(positive(expectedArea),"INVALID_DERIVED_NATIVE_MATERIAL_AREA");
            const ReferenceMaterialSeed* seed{};for(const auto& s:seeds)if(s.prototypeId==region.expression.prototypeId){need(!seed,"DUPLICATE_MATERIAL_PROTOTYPE");seed=&s;}
            need(seed&&seed->database&&!seed->prototypeConfirmationSource.empty()&&!seed->unitConfirmationSource.empty()&&!seed->scaleConfirmationSource.empty()&&!seed->phaseConfirmationSource.empty(),"REFERENCE_MATERIAL_FACTS_REQUIRED");
            need(positive(seed->mmPerSourceUnit)&&positive(seed->sourcePaperScale)&&finite(seed->referenceAnchor),"INVALID_REFERENCE_UNITS_SCALE_PHASE");
            need(upper(seed->referenceSha256)==upper(plan.referenceSha256),"REFERENCE_SHA_MISMATCH");
            const MaterialPlacement* placement{};for(const auto& p:options.placements)if(p.regionIdentity==region.identity&&p.viewIdentity==region.viewIdentity){need(!placement,"DUPLICATE_PLACEMENT");placement=&p;}
            need(placement&&positive(placement->targetPaperScale)&&finite(placement->targetAnchor)&&!placement->scaleConfirmationSource.empty()&&!placement->phaseConfirmationSource.empty(),"TARGET_PLACEMENT_REQUIRED");
            state->sources.insert(seed->database);
            const auto targetId=cloneVerifiedReference(*seed,upper(plan.referenceSha256),*state->database);
            {
                Open<AcDbHatch> h(targetId,AcDb::kForWrite);Canonical original;pattern(original,*h);appearance(original,*h);
                need(region.expression.patternName.empty()||wide(region.expression.patternName)==h->patternName(),"PROTOTYPE_PATTERN_MISMATCH");
                const auto factor=seed->mmPerSourceUnit/plan.mmPerInputUnit*placement->targetPaperScale/seed->sourcePaperScale*region.expression.scale;
                need(positive(factor),"INVALID_MATERIAL_PATTERN_SCALE");const auto angle=std::atan2(region.expression.direction.y,region.expression.direction.x);
                const auto matrix=AcGeMatrix3d::translation({placement->targetAnchor.x,placement->targetAnchor.y,0})*AcGeMatrix3d::rotation(angle,AcGeVector3d::kZAxis)*AcGeMatrix3d::scaling(factor)*AcGeMatrix3d::translation({-seed->referenceAnchor.x,-seed->referenceAnchor.y,0});
                check(h->setAssociative(false),"detach clone association");check(h->removeAssocObjIds(),"remove clone boundary association");
                // AutoCAD 2020 transforms embedded pattern definitions but leaves originPoint metadata unchanged.
                // The explicit public setter updates that WCS metadata without reloading any external PAT.
                const auto oldOrigin=h->originPoint();
                const auto mappedOrigin=matrix*AcGePoint3d(oldOrigin.x,oldOrigin.y,0);
                check(h->transformBy(matrix),"map native pattern units scale and phase");
                check(h->setOriginPoint({mappedOrigin.x,mappedOrigin.y}),"set mapped native pattern origin metadata");
                while(h->numLoops())check(h->removeLoopAt(h->numLoops()-1),"remove clone old loop");
                check(h->setHatchStyle(AcDbHatch::kNormal),"holes normal style");appendRing(*h,region.geometry.outer,true);for(const auto& hole:region.geometry.holes)appendRing(*h,hole,false);
                h->setLineGenerationEnabled(true);check(h->evaluateHatch(),"evaluate native material");
                if(!h->isSolidFill()) {
                    // eOk alone is not output evidence: AutoCAD 2020 can return eOk with an empty pattern.
                    AcGePoint2dArray starts,ends;check(h->getHatchLinesData(starts,ends),"native material pattern output");
                    need(h->numHatchLines()>0&&starts.length()>0&&starts.length()==ends.length()&&starts.length()==h->numHatchLines(),"EMPTY_NATIVE_PATTERN_OUTPUT: BLOCKED_CAPABILITY; no committable material batch");
                }
            }
            {
                // Close and reopen after changing loops. A previously queried area may be cached on an open Hatch.
                Open<AcDbHatch> h(targetId);double area{};check(h->getArea(area),"reopened native material area");
                need(positive(area)&&std::abs(area-expectedArea)<=1e-8*(std::max)(1.0,expectedArea),"NATIVE_MATERIAL_AREA_MISMATCH");
            }
            state->roots.append(targetId);
        }
        const auto all=::bce::roots(*state->database);for(int i=0;i<all.length();++i)if(!state->roots.contains(all[i])){Open<AcDbEntity> old(all[i],AcDb::kForWrite);check(old->erase(),"remove reference association auxiliary root");}
        state->baseline=snapshot(*state->database,state->roots);check(state->database->addReactor(state.get()),"arm immutable batch");state->armed=true;return {PreparedMaterialBatch(std::move(state)),{}};
    }catch(const std::exception& e){return {std::nullopt,{{"MATERIAL_PREPARATION_REJECTED",location+": "+e.what(),{"",location,std::nullopt},true}}};}
}
}




