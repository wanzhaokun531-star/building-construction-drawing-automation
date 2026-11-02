#include "NativeMaterialTiles.h"
#include <Windows.h>
#include <acdocman.h>
#include <dbapserv.h>
#include <dbHatch.h>
#include <dbfiler.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <dbdict.h>
#include <dbmaterial.h>
#include <dbplhldr.h>
#include <dbxrecrd.h>
#include <dbtrans.h>
#include <acstring.h>
#include <gescl3d.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
namespace bce {
namespace {
void need(bool value,const std::string& reason){if(!value)throw std::runtime_error(reason);}
void check(Acad::ErrorStatus s,const char* reason){need(s==Acad::eOk,std::string(reason)+":"+std::to_string(s));}
bool positive(double v){return std::isfinite(v)&&v>0;}
bool close(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-8*(std::max)(1.0,std::abs(b));}
template<class T> class Open final {
public:explicit Open(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead){check(acdbOpenObject(p_,id,mode),"open");}~Open(){if(p_)p_->close();}Open(const Open&)=delete;Open& operator=(const Open&)=delete;T* operator->()const{return p_;}T& operator*()const{return *p_;}T* release(){auto* p=p_;p_=nullptr;return p;}
private:T* p_{};
};
class Working final {
public:explicit Working(AcDbDatabase& db):old_(acdbHostApplicationServices()->workingDatabase()){acdbHostApplicationServices()->setWorkingDatabase(&db);}~Working(){acdbHostApplicationServices()->setWorkingDatabase(old_);}Working(const Working&)=delete;Working& operator=(const Working&)=delete;
private:AcDbDatabase* old_;
};
bool documentDatabase(AcDbDatabase& db){need(acDocManager!=nullptr,"DOCUMENT_MANAGER_REQUIRED");std::unique_ptr<AcApDocumentIterator> it(acDocManager->newAcApDocumentIterator());need(it!=nullptr,"DOCUMENT_ITERATOR_REQUIRED");for(;!it->done();it->step())if(it->document()&&it->document()->database()==&db)return true;return false;}
AcDbObjectIdArray modelRoots(AcDbDatabase& db){Open<AcDbBlockTableRecord> b(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};check(b->newIterator(raw),"root iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);AcDbObjectIdArray ids;for(;!it->done();it->step()){AcDbObjectId id;check(it->getEntityId(id),"root id");ids.append(id);}return ids;}
struct Rect {double x0,y0,x1,y1;};
Rect rectangle(const ClosedRing& ring){
    need(ring.closed,"OPEN_MATERIAL_RING");auto p=ring.vertices;if(p.size()==5&&p.front().x==p.back().x&&p.front().y==p.back().y)p.pop_back();need(p.size()==4,"UNSUPPORTED_NONRECTANGULAR_REGION");
    Rect r{p[0].x,p[0].y,p[0].x,p[0].y};for(auto v:p){need(std::isfinite(v.x)&&std::isfinite(v.y),"NONFINITE_REGION_POINT");r.x0=(std::min)(r.x0,v.x);r.x1=(std::max)(r.x1,v.x);r.y0=(std::min)(r.y0,v.y);r.y1=(std::max)(r.y1,v.y);}need(positive(r.x1-r.x0)&&positive(r.y1-r.y0),"DEGENERATE_RECTANGLE");std::set<std::pair<double,double>> corners;
    for(std::size_t i=0;i<4;++i){auto a=p[i],b=p[(i+1)%4];need((a.x==r.x0||a.x==r.x1)&&(a.y==r.y0||a.y==r.y1),"UNSUPPORTED_NONRECTANGULAR_REGION");need((a.x==b.x)!=(a.y==b.y),"UNSUPPORTED_NONRECTANGULAR_REGION");corners.emplace(a.x,a.y);}need(corners.size()==4,"INVALID_RECTANGLE_CORNERS");return r;
}
PolygonRegion geometry(Rect r){return {{{{r.x0,r.y0},{r.x1,r.y0},{r.x1,r.y1},{r.x0,r.y1}},true},{}};}
double area(Rect r){const double v=(r.x1-r.x0)*(r.y1-r.y0);need(positive(v),"NONFINITE_OR_ZERO_TILE_AREA");return v;}
std::vector<Rect> partition(const MaterialRegionPlan& parent,double units,const MaterialTileLimits& limits){
    const auto outer=rectangle(parent.geometry.outer);std::vector<Rect> holes;std::vector<double> xs{outer.x0,outer.x1},ys{outer.y0,outer.y1};double net=area(outer);
    need(parent.geometry.holes.size()<=limits.maxTiles,"TILE_BUDGET");for(const auto& ring:parent.geometry.holes){auto h=rectangle(ring);need(h.x0>outer.x0&&h.y0>outer.y0&&h.x1<outer.x1&&h.y1<outer.y1,"HOLE_NOT_STRICTLY_INSIDE");for(auto other:holes)need(h.x1<other.x0||other.x1<h.x0||h.y1<other.y0||other.y1<h.y0,"INTERSECTING_OR_TOUCHING_HOLES");holes.push_back(h);xs.push_back(h.x0);xs.push_back(h.x1);ys.push_back(h.y0);ys.push_back(h.y1);net-=area(h);}
    need(positive(net)&&positive(parent.areaSquareMm)&&positive(units*units)&&close(net*units*units,parent.areaSquareMm),"PARENT_MATERIAL_AREA_MISMATCH");
    auto sort=[](std::vector<double>& values){std::sort(values.begin(),values.end());values.erase(std::unique(values.begin(),values.end()),values.end());};sort(xs);sort(ys);need(xs.size()-1<=limits.maxAttempts/(ys.size()-1),"GRID_ATTEMPT_BUDGET");std::vector<Rect> result;
    for(std::size_t x=1;x<xs.size();++x)for(std::size_t y=1;y<ys.size();++y){Rect cell{xs[x-1],ys[y-1],xs[x],ys[y]};bool hole=false;for(auto h:holes)if(cell.x0>=h.x0&&cell.x1<=h.x1&&cell.y0>=h.y0&&cell.y1<=h.y1){hole=true;break;}if(!hole){need(result.size()<limits.maxTiles,"TILE_BUDGET");result.push_back(cell);}}
    need(!result.empty(),"EMPTY_PARTITION");return result;
}

// Official public filing, opaque typed values, exact standard classes only.
// No TCH/private decoding. References recurse, with cycle/depth/byte budgets.
bool standardResource(AcRxClass* c){return c==AcDbLayerTableRecord::desc()||c==AcDbLinetypeTableRecord::desc()||c==AcDbMaterial::desc()||c==AcDbPlaceHolder::desc()||c==AcDbRegAppTableRecord::desc()||c==AcDbDictionary::desc()||c==AcDbXrecord::desc();}
std::string diagnosticText(const ACHAR* text){
    if(!text)return "null";const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,nullptr,0,nullptr,nullptr);if(n<=0)return "UTF8_ENCODING_FAILED";std::string out(static_cast<std::size_t>(n),'\0');if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text,-1,&out[0],n,nullptr,nullptr)!=n)return "UTF8_ENCODING_FAILED";out.pop_back();return out;
}
std::string diagnosticHandle(AcDbObjectId id){if(id.isNull())return "null";ACHAR buffer[32]{};id.handle().getIntoAsciiBuffer(buffer);return diagnosticText(buffer);}
std::string diagnosticNode(AcDbObject& object,const char* role,std::size_t depth){
    auto* cls=object.isA();auto ownerId=object.ownerId();std::ostringstream out;out<<"role="<<role<<",depth="<<depth<<",handle="<<diagnosticHandle(object.objectId())<<",class="<<diagnosticText(cls->name())<<",ownerHandle="<<diagnosticHandle(ownerId);
    const bool declaredPublic=standardResource(cls)||cls==AcDbHatch::desc()||cls==AcDbDictionary::desc()||cls==AcDbXrecord::desc()||cls==AcDbBlockTableRecord::desc()||cls==AcDbBlockTable::desc()||cls==AcDbLayerTable::desc()||cls==AcDbLinetypeTable::desc()||cls==AcDbRegAppTable::desc();
    out<<",knownPublicSDKDesc="<<(declaredPublic?"true":"not-in-diagnostic-catalog")<<",productionResourceAllowed="<<(standardResource(cls)?"true":"false");
    if(!ownerId.isNull()){Open<AcDbObject> owner(ownerId);out<<",ownerClass="<<diagnosticText(owner->isA()->name());if(owner->isA()==AcDbDictionary::desc()){AcString key;const auto status=AcDbDictionary::cast(&*owner)->nameAt(object.objectId(),key);out<<",dictionaryKeyStatus="<<status;if(status==Acad::eOk)out<<",dictionaryKey="<<diagnosticText(key.kACharPtr());}}
    if(object.isKindOf(AcDbSymbolTableRecord::desc())){AcString name;const auto status=AcDbSymbolTableRecord::cast(&object)->getName(name);out<<",symbolNameStatus="<<status;if(status==Acad::eOk)out<<",symbolName="<<diagnosticText(name.kACharPtr());}return out.str();
}
struct Graph;
class ValueFiler final:public AcDbDwgFiler {
public:
    ValueFiler(Graph& graph,AcDbObjectId self,AcDbObjectId owner):graph_(graph),self_(self),owner_(owner){out.imbue(std::locale::classic());out<<std::hexfloat;}
    Acad::ErrorStatus filerStatus()const override{return status_;}AcDb::FilerType filerType()const override{return AcDb::kBagFiler;}void setFilerStatus(Acad::ErrorStatus s)override{status_=s;}void resetFilerStatus()override{status_=Acad::eOk;}
    Acad::ErrorStatus dwgVersion(AcDb::AcDbDwgVersion& v,AcDb::MaintenanceReleaseVersion& m)const override{v=AcDb::kDHL_CURRENT;m=AcDb::kMReleaseCurrent;return Acad::eOk;}
    Acad::ErrorStatus reference(AcDbObjectId id,const char* type);
#define BCE_ID_METHOD(Name,Type) Acad::ErrorStatus read##Name(Type*)override{return Acad::eNotApplicable;} Acad::ErrorStatus write##Name(const Type& v)override{return reference(v,#Name);}
    BCE_ID_METHOD(HardOwnershipId,AcDbHardOwnershipId)
    BCE_ID_METHOD(SoftOwnershipId,AcDbSoftOwnershipId)
    BCE_ID_METHOD(HardPointerId,AcDbHardPointerId)
    BCE_ID_METHOD(SoftPointerId,AcDbSoftPointerId)
#undef BCE_ID_METHOD
#define BCE_SCALAR_METHOD(Name,Type) Acad::ErrorStatus read##Name(Type*)override{return Acad::eNotApplicable;} Acad::ErrorStatus write##Name(Type v)override{out<<#Name<<':'<<+v<<';';return Acad::eOk;}
    BCE_SCALAR_METHOD(Int8,Adesk::Int8)
    BCE_SCALAR_METHOD(Int16,Adesk::Int16)
    BCE_SCALAR_METHOD(Int32,Adesk::Int32)
    BCE_SCALAR_METHOD(Int64,Adesk::Int64)
    BCE_SCALAR_METHOD(UInt8,Adesk::UInt8)
    BCE_SCALAR_METHOD(UInt16,Adesk::UInt16)
    BCE_SCALAR_METHOD(UInt32,Adesk::UInt32)
    BCE_SCALAR_METHOD(UInt64,Adesk::UInt64)
    BCE_SCALAR_METHOD(Boolean,Adesk::Boolean)
    BCE_SCALAR_METHOD(Bool,bool)
#undef BCE_SCALAR_METHOD
    Acad::ErrorStatus readDouble(double*)override{return Acad::eNotApplicable;}Acad::ErrorStatus writeDouble(double v)override{need(std::isfinite(v),"RESOURCE_NONFINITE_VALUE");out<<"Double:"<<v<<';';return Acad::eOk;}
    Acad::ErrorStatus readString(ACHAR**)override{return Acad::eNotApplicable;}Acad::ErrorStatus readString(AcString&)override{return Acad::eNotApplicable;}Acad::ErrorStatus writeString(const AcString& s)override{return writeString(s.kACharPtr());}Acad::ErrorStatus writeString(const ACHAR* s)override{out<<"String:";if(!s){out<<"null;";return Acad::eOk;}while(*s)out<<static_cast<unsigned>(*s++)<<',';out<<';';return Acad::eOk;}
    Acad::ErrorStatus readBChunk(ads_binary*)override{return Acad::eNotApplicable;}Acad::ErrorStatus writeBChunk(const ads_binary& b)override{return writeBytes(b.buf,static_cast<Adesk::UIntPtr>(b.clen));}
    Acad::ErrorStatus readAcDbHandle(AcDbHandle*)override{return Acad::eNotApplicable;}Acad::ErrorStatus writeAcDbHandle(const AcDbHandle& h)override{need(h.isNull()||h==self_.handle()||(!owner_.isNull()&&h==owner_.handle()),"UNSUPPORTED_RESOURCE_HANDLE_REFERENCE");out<<"Handle:"<<(h.isNull()?"null":h==self_.handle()?"self":"owner")<<';';return Acad::eOk;}
#define BCE_VECTOR_METHOD(Name,Type,Fields) Acad::ErrorStatus read##Name(Type*)override{return Acad::eNotApplicable;}Acad::ErrorStatus write##Name(const Type& v)override{out<<#Name<<':';Fields return Acad::eOk;}
    BCE_VECTOR_METHOD(Point2d,AcGePoint2d,writeDouble(v.x);writeDouble(v.y);)
    BCE_VECTOR_METHOD(Point3d,AcGePoint3d,writeDouble(v.x);writeDouble(v.y);writeDouble(v.z);)
    BCE_VECTOR_METHOD(Vector2d,AcGeVector2d,writeDouble(v.x);writeDouble(v.y);)
    BCE_VECTOR_METHOD(Vector3d,AcGeVector3d,writeDouble(v.x);writeDouble(v.y);writeDouble(v.z);)
    BCE_VECTOR_METHOD(Scale3d,AcGeScale3d,writeDouble(v.sx);writeDouble(v.sy);writeDouble(v.sz);)
#undef BCE_VECTOR_METHOD
    Acad::ErrorStatus readBytes(void*,Adesk::UIntPtr)override{return Acad::eNotApplicable;}Acad::ErrorStatus writeBytes(const void* p,Adesk::UIntPtr n)override{need(n<=1048576,"RESOURCE_BYTES_BUDGET");out<<"Bytes:"<<n<<':';auto* bytes=static_cast<const unsigned char*>(p);need(p||n==0,"RESOURCE_NULL_BYTES");for(Adesk::UIntPtr i=0;i<n;++i)out<<static_cast<unsigned>(bytes[i])<<',';out<<';';return Acad::eOk;}
    Acad::ErrorStatus readAddress(void**)override{return Acad::eNotApplicable;}Acad::ErrorStatus writeAddress(const void*)override{throw std::runtime_error("UNSUPPORTED_RESOURCE_ADDRESS");}
    Acad::ErrorStatus seek(Adesk::Int64,int)override{return Acad::eNotApplicable;}Adesk::Int64 tell()const override{return static_cast<Adesk::Int64>(out.str().size());}
    std::ostringstream out;
private:Graph& graph_;AcDbObjectId self_,owner_;Acad::ErrorStatus status_{Acad::eOk};
};
struct Graph {
    std::vector<AcDbObjectId> active,resources;
    std::vector<std::string> diagnosticPath;
    std::size_t bytes{};
    std::string value(AcDbObjectId id,bool root=false,const char* role="root-or-resource-check"){
        need(!id.isNull()&&!id.isErased(),"INVALID_RESOURCE_REFERENCE");need(active.size()<32&&resources.size()<512,"RESOURCE_GRAPH_BUDGET");auto cycle=std::find(active.begin(),active.end(),id);if(cycle!=active.end())return "cycle-depth:"+std::to_string(std::distance(active.begin(),cycle));
        Open<AcDbObject> object(id);const auto node=diagnosticNode(*object,role,active.size());if(!(standardResource(object->isA())||(root&&object->isA()==AcDbHatch::desc()))){std::string trace;for(const auto& ancestor:diagnosticPath)trace+="["+ancestor+"] -> ";throw std::runtime_error("UNSUPPORTED_RESOURCE_CLASS; traversal="+trace+"["+node+"]; rejectedNodeFilerStatus=not-entered");}if(!root&&std::find(resources.begin(),resources.end(),id)==resources.end())resources.push_back(id);
        active.push_back(id);diagnosticPath.push_back(node);struct Pop{std::vector<AcDbObjectId>& stack;std::vector<std::string>& path;~Pop(){path.pop_back();stack.pop_back();}} pop{active,diagnosticPath};ValueFiler filer(*this,id,object->ownerId());filer.writeString(object->isA()->name());
        if(object->isA()==AcDbXrecord::desc()){const auto* record=AcDbXrecord::cast(&*object);filer.out<<"XrecordReferenceTranslation:"<<record->isXlateReferences()<<";XrecordMergeStyle:"<<record->mergeStyle()<<';';}
        const auto filingStatus=object->dwgOutFields(&filer);need(filingStatus==Acad::eOk&&filer.filerStatus()==Acad::eOk,"PUBLIC_RESOURCE_FILER_FAILED; "+node+"; callStatus="+std::to_string(filingStatus)+"; filerStatus="+std::to_string(filer.filerStatus()));auto result=filer.out.str();need(result.size()<=4194304&&bytes<=16777216-result.size(),"RESOURCE_GRAPH_BYTES_BUDGET");bytes+=result.size();return result;
    }
};
Acad::ErrorStatus ValueFiler::reference(AcDbObjectId id,const char* type){out<<type<<':';if(id.isNull())out<<"null;";else if(id==self_)out<<"self;";else if(id==owner_)out<<"owner;";else{const auto edge=std::string(type)+"(source="+diagnosticHandle(self_)+",filerStatus="+std::to_string(status_)+")";out<<'{'<<graph_.value(id,false,edge.c_str())<<"};";}return Acad::eOk;}
std::string expression(AcDbObjectId id){Graph graph;return graph.value(id,true);}
void equalResource(AcDbObjectId source,AcDbObjectId target,const char* reason,bool root=false){Graph a,b;const auto left=a.value(source,root),right=b.value(target,root);if(left==right)return;std::size_t offset=0;while(offset<left.size()&&offset<right.size()&&left[offset]==right[offset])++offset;const auto start=offset>64?offset-64:0;Open<AcDbObject> from(source),to(target);throw std::runtime_error(std::string(reason)+"; source=["+diagnosticNode(*from,"compare-source",0)+"]; target=["+diagnosticNode(*to,"compare-target",0)+"]; firstOpaqueDifference="+std::to_string(offset)+"; sourceLength="+std::to_string(left.size())+"; targetLength="+std::to_string(right.size())+"; sourceFragment="+left.substr(start,192)+"; targetFragment="+right.substr(start,192));}
AcDbObjectId targetResource(AcDbObjectId source,AcDbDatabase& target,unsigned depth=0){
    need(depth<32,"RESOURCE_OWNER_PATH_BUDGET");Open<AcDbObject> object(source);auto* db=object->database();AcDbObjectId targetOwner;auto owner=object->ownerId();if(owner==db->layerTableId())targetOwner=target.layerTableId();else if(owner==db->linetypeTableId())targetOwner=target.linetypeTableId();else if(owner==db->regAppTableId())targetOwner=target.regAppTableId();else if(owner==db->materialDictionaryId())targetOwner=target.materialDictionaryId();else if(owner==db->plotStyleNameDictionaryId())targetOwner=target.plotStyleNameDictionaryId();else{
        need(!owner.isNull(),"UNSUPPORTED_RESOURCE_OWNER");Open<AcDbObject> parent(owner);need(standardResource(parent->isA()),"UNSUPPORTED_RESOURCE_OWNER_CLASS");const auto mappedParent=targetResource(owner,target,depth+1);if(mappedParent.isNull())return AcDbObjectId::kNull;
        if(object->isA()==AcDbDictionary::desc()&&parent->extensionDictionary()==source){Open<AcDbObject> mapped(mappedParent);const auto dictionaryId=mapped->extensionDictionary();if(!dictionaryId.isNull()){Open<AcDbObject> dictionary(dictionaryId);need(dictionary->isA()==AcDbDictionary::desc()&&dictionary->ownerId()==mappedParent,"RESOURCE_EXTENSION_OWNER_MISMATCH");}return dictionaryId;}
        need(parent->isA()==AcDbDictionary::desc(),"UNSUPPORTED_RESOURCE_OWNER_EDGE");Open<AcDbObject> mapped(mappedParent);need(mapped->isA()==AcDbDictionary::desc(),"RESOURCE_DICTIONARY_OWNER_CLASS_MISMATCH");targetOwner=mappedParent;
    }
    AcDbObjectId id;AcString name;if(object->isKindOf(AcDbSymbolTableRecord::desc())){auto* record=AcDbSymbolTableRecord::cast(&*object);check(record->getName(name),"resource name");Open<AcDbSymbolTable> table(targetOwner);if(table->has(name.kACharPtr()))check(table->getAt(name.kACharPtr(),id),"target symbol");}else{Open<AcDbDictionary> old(owner);check(old->nameAt(source,name),"resource dictionary key");Open<AcDbDictionary> dictionary(targetOwner);if(dictionary->has(name.kACharPtr()))check(dictionary->getAt(name.kACharPtr(),id),"target resource dictionary");}return id;
}
void preflight(const AcDbObjectIdArray& roots,AcDbDatabase& target){Graph graph;for(int i=0;i<roots.length();++i)graph.value(roots[i],true);for(auto resource:graph.resources){auto destination=targetResource(resource,target);if(!destination.isNull())equalResource(resource,destination,"RESOURCE_CONFLICT");}}
class Transaction final {
public:explicit Transaction(AcDbDatabase& db):manager_(db.transactionManager()){need(manager_&&manager_->startTransaction(),"CLONE_TRANSACTION_REQUIRED");}~Transaction(){if(active_)manager_->abortTransaction();}void adopt(AcDbIdMapping& map,AcDbDatabase& db){AcDbIdMappingIter it(map);for(it.start();!it.done();it.next()){AcDbIdPair p;need(it.getMap(p),"clone map iteration");if(!p.isCloned()||p.value().isNull()||p.value().database()!=&db)continue;Open<AcDbObject> o(p.value(),AcDb::kForWrite);if(!o->isTransactionResident()){check(manager_->addNewlyCreatedDBRObject(&*o),"adopt cloned object");o.release();}}}void commit(){check(manager_->endTransaction(),"clone transaction commit");active_=false;}
private:AcDbTransactionManager* manager_;bool active_{true};
};
AcDbObjectIdArray cloneVerified(AcDbDatabase& source,const AcDbObjectIdArray& input,AcDbDatabase& target){
    preflight(input,target);Working work(target);Transaction transaction(target);AcDbIdMapping mapping;const auto status=source.wblockCloneObjects(input,acdbSymUtil()->blockModelSpaceId(&target),mapping,AcDb::kDrcIgnore);
    try{check(status,"clone verified material tiles");AcDbObjectIdArray result;
    for(int i=0;i<input.length();++i){AcDbIdPair p;p.setKey(input[i]);need(mapping.compute(p)&&p.isCloned()&&!p.value().isNull()&&p.value().database()==&target,"TILE_MAPPING_MISMATCH");equalResource(input[i],p.value(),"CLONED_NATIVE_EXPRESSION_CHANGED",true);result.append(p.value());}
    Graph graph;for(int i=0;i<input.length();++i)graph.value(input[i],true);for(auto id:graph.resources){AcDbIdPair p;p.setKey(id);need(mapping.compute(p)&&!p.value().isNull()&&p.value().database()==&target,"RESOURCE_MAPPING_MISSING");need(targetResource(id,target)==p.value(),"RESOURCE_MAPPING_NAME_OR_OWNER_CHANGED");equalResource(id,p.value(),"RESOURCE_MAPPING_CONFLICT");}transaction.adopt(mapping,target);transaction.commit();return result;
    }catch(...){transaction.adopt(mapping,target);throw;}
}
std::string snapshot(AcDbDatabase& db,const AcDbObjectIdArray& ids){auto actual=modelRoots(db);need(actual.length()==ids.length(),"TILE_ROOT_SET_CHANGED");std::string data;for(int i=0;i<ids.length();++i){need(actual.contains(ids[i]),"TILE_ROOT_CHANGED");auto text=expression(ids[i]);data+=std::to_string(text.size())+":"+text;}return data;}
std::pair<double,int> measure(AcDbDatabase& db,AcDbObjectId id,Rect r){
    Working work(db);Open<AcDbHatch> h(id);need(h->isA()==AcDbHatch::desc()&&!h->associative()&&h->numLoops()==1,"INVALID_NATIVE_TILE");double measured{};check(h->getArea(measured),"native tile area");need(close(measured,area(r)),"NATIVE_TILE_AREA_MISMATCH");
    Adesk::Int32 type{};AcGePoint2dArray p;AcGeDoubleArray bulges;check(h->getLoopAt(0,type,p,bulges),"native tile loop");need(p.length()==5&&p[0]==AcGePoint2d(r.x0,r.y0)&&p[1]==AcGePoint2d(r.x1,r.y0)&&p[2]==AcGePoint2d(r.x1,r.y1)&&p[3]==AcGePoint2d(r.x0,r.y1)&&p[4]==p[0],"NATIVE_TILE_BOUNDARY_MISMATCH");for(int i=0;i<bulges.length();++i)need(bulges[i]==0,"NATIVE_TILE_BULGE");
    AcGePoint2dArray starts,ends;check(h->getHatchLinesData(starts,ends),"native tile line data");need(!h->isSolidFill()&&h->numHatchLines()>0&&starts.length()==ends.length()&&starts.length()==h->numHatchLines(),"EMPTY_NATIVE_PATTERN_OUTPUT");const double eps=1e-9*(std::max)({1.0,std::abs(r.x0),std::abs(r.x1),std::abs(r.y0),std::abs(r.y1)});for(int i=0;i<starts.length();++i)for(auto point:{starts[i],ends[i]})need(std::isfinite(point.x)&&std::isfinite(point.y)&&point.x>=r.x0-eps&&point.x<=r.x1+eps&&point.y>=r.y0-eps&&point.y<=r.y1+eps,"NATIVE_TILE_LINES_OUTSIDE_BOUNDARY");return {measured,starts.length()};
}
std::string patternKey(AcDbHatch& h){
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::hexfloat;out<<h.patternType()<<';'<<h.patternScale()<<';'<<h.patternAngle()<<';'<<h.patternSpace()<<';'<<h.patternDouble()<<';'<<h.originPoint().x<<';'<<h.originPoint().y<<';'<<h.color().color()<<';'<<h.numPatternDefinitions()<<';';for(const ACHAR* s=h.patternName();*s;++s)out<<static_cast<unsigned>(*s)<<',';
    for(int i=0;i<h.numPatternDefinitions();++i){double angle{},x{},y{},dx{},dy{};AcGeDoubleArray d;check(h.getPatternDefinitionAt(i,angle,x,y,dx,dy,d),"native shared pattern");out<<angle<<';'<<x<<';'<<y<<';'<<dx<<';'<<dy<<';';for(int j=0;j<d.length();++j)out<<d[j]<<';';}return out.str();
}
void validateSeams(const AcDbObjectIdArray& roots,const std::vector<MaterialTileRecord>& tiles,std::size_t first,double units){
    struct Tile {Rect box;AcGePoint2dArray starts,ends;};std::vector<Tile> native;std::string physical;const double eps=1e-5/units;
    for(std::size_t i=first;i<tiles.size();++i){Open<AcDbHatch> h(roots[static_cast<int>(i)]);auto current=patternKey(*h);if(i==first)physical=current;need(physical==current,"INCONSISTENT_GLOBAL_MATERIAL_PHASE");Tile tile;tile.box=rectangle(tiles[i].geometry.outer);check(h->getHatchLinesData(tile.starts,tile.ends),"seam actual lines");need(tile.starts.length()>0&&tile.starts.length()==tile.ends.length(),"EMPTY_NATIVE_PATTERN_OUTPUT");native.push_back(std::move(tile));}
    auto contacts=[&](const Tile& tile,bool vertical,double at,double lo,double hi){std::vector<double> result;for(int i=0;i<tile.starts.length();++i){auto a=tile.starts[i],b=tile.ends[i];if(std::abs(vertical?a.x-b.x:a.y-b.y)<=eps)continue;for(auto p:{a,b})if(std::abs((vertical?p.x:p.y)-at)<=eps){const double c=vertical?p.y:p.x;if(c>lo+eps&&c<hi-eps)result.push_back(c);}}std::sort(result.begin(),result.end());return result;};
    for(std::size_t i=0;i<native.size();++i)for(std::size_t j=i+1;j<native.size();++j){auto a=native[i].box,b=native[j].box;double dx=(std::min)(a.x1,b.x1)-(std::max)(a.x0,b.x0),dy=(std::min)(a.y1,b.y1)-(std::max)(a.y0,b.y0);need(!(dx>0&&dy>0),"OVERLAPPING_NATIVE_TILES");const bool vertical=dy>eps&&(a.x1==b.x0||b.x1==a.x0),horizontal=dx>eps&&(a.y1==b.y0||b.y1==a.y0);if(!vertical&&!horizontal)continue;const double at=vertical?(a.x1==b.x0?a.x1:b.x1):(a.y1==b.y0?a.y1:b.y1);const double lo=vertical?(std::max)(a.y0,b.y0):(std::max)(a.x0,b.x0),hi=vertical?(std::min)(a.y1,b.y1):(std::min)(a.x1,b.x1);auto one=contacts(native[i],vertical,at,lo,hi),two=contacts(native[j],vertical,at,lo,hi);need(one.size()==two.size(),"NATIVE_SEAM_CONTACT_COUNT_MISMATCH");for(std::size_t k=0;k<one.size();++k)need(std::abs(one[k]-two[k])<=eps,"NATIVE_SEAM_PHASE_MISMATCH");}
}
}
struct PreparedMaterialTiles::State:AcDbDatabaseReactor {
    std::unique_ptr<AcDbDatabase> database;AcDbObjectIdArray roots;MaterialPlan parent;std::vector<MaterialTileRecord> tiles;std::vector<PreparedMaterialBatch> children;std::set<AcDbDatabase*> sources;std::string baseline;bool armed{},changed{};
    ~State(){if(armed)database->removeReactor(this);}
    void objectAppended(const AcDbDatabase*,const AcDbObject*)override{changed=true;}void objectUnAppended(const AcDbDatabase*,const AcDbObject*)override{changed=true;}void objectReAppended(const AcDbDatabase*,const AcDbObject*)override{changed=true;}void objectOpenedForModify(const AcDbDatabase*,const AcDbObject*)override{changed=true;}void objectModified(const AcDbDatabase*,const AcDbObject*)override{changed=true;}void objectErased(const AcDbDatabase*,const AcDbObject*,Adesk::Boolean)override{changed=true;}void headerSysVarWillChange(const AcDbDatabase*,const ACHAR*)override{changed=true;}
};
PreparedMaterialTiles::PreparedMaterialTiles(std::unique_ptr<State> s):state_(std::move(s)){}
PreparedMaterialTiles::~PreparedMaterialTiles()=default;
PreparedMaterialTiles::PreparedMaterialTiles(PreparedMaterialTiles&&)noexcept=default;
PreparedMaterialTiles& PreparedMaterialTiles::operator=(PreparedMaterialTiles&&)noexcept=default;
AcDbDatabase& PreparedMaterialTiles::database()const{need(state_!=nullptr,"MOVED_TILE_BATCH");return *state_->database;}
const AcDbObjectIdArray& PreparedMaterialTiles::roots()const{need(state_!=nullptr,"MOVED_TILE_BATCH");return state_->roots;}
const std::vector<MaterialTileRecord>& PreparedMaterialTiles::tiles()const{need(state_!=nullptr,"MOVED_TILE_BATCH");return state_->tiles;}
const MaterialPlan& PreparedMaterialTiles::parentPlan()const{need(state_!=nullptr,"MOVED_TILE_BATCH");return state_->parent;}
bool PreparedMaterialTiles::intact()const noexcept{try{if(!state_||state_->changed)return false;for(const auto& child:state_->children)if(!child.intact())return false;Working work(*state_->database);return snapshot(*state_->database,state_->roots)==state_->baseline;}catch(...){return false;}}
Result<std::vector<AcDbObjectId>> PreparedMaterialTiles::cloneToEmpty(AcDbDatabase& target)const{
    try{need(state_&&&target!=state_->database.get()&&state_->sources.count(&target)==0&&&target!=acdbHostApplicationServices()->workingDatabase()&&!documentDatabase(target),"INDEPENDENT_NON_DOCUMENT_TARGET_REQUIRED");need(modelRoots(target).isEmpty(),"EMPTY_TARGET_REQUIRED");need(intact(),"PREPARED_MATERIAL_TILES_CHANGED");auto ids=cloneVerified(*state_->database,state_->roots,target);need(modelRoots(target).length()==ids.length(),"EXTRA_CLONED_ROOT");std::vector<AcDbObjectId> result;for(int i=0;i<ids.length();++i)result.push_back(ids[i]);return {std::move(result),{}};}catch(const std::exception& e){return {std::nullopt,{{"MATERIAL_TILES_CLONE_REJECTED",e.what(),{},true}}};}
}
Result<PreparedMaterialTiles> prepareMaterialTiles(const MaterialPlan& plan,const std::vector<ReferenceMaterialSeed>& seeds,const MaterialPreparationOptions& options,const MaterialTileLimits& limits){
    std::string diagnosticLocation;
    try{
        need(positive(plan.mmPerInputUnit)&&!plan.regions.empty()&&!plan.unitConfirmationSource.empty(),"MATERIAL_FACTS_REQUIRED");need(limits.maxTiles>0&&limits.maxTiles<=4096&&limits.maxAttempts>0&&limits.maxAttempts<=8192&&limits.maxDepth<=32,"INVALID_TILE_LIMITS");need(options.placements.size()==plan.regions.size(),"EXPLICIT_MATERIAL_PLACEMENT_REQUIRED");auto state=std::make_unique<PreparedMaterialTiles::State>();state->database=std::make_unique<AcDbDatabase>(true,true);state->parent=plan;for(const auto& seed:seeds)if(seed.database)state->sources.insert(seed.database);std::size_t attempts{};std::set<std::pair<std::string,std::string>> identities;
        for(const auto& parent:plan.regions){need(identities.emplace(parent.viewIdentity,parent.identity).second,"DUPLICATE_MATERIAL_REGION");const MaterialPlacement* placement{};for(const auto& p:options.placements)if(p.regionIdentity==parent.identity&&p.viewIdentity==parent.viewIdentity){need(!placement,"DUPLICATE_PLACEMENT");placement=&p;}need(placement!=nullptr,"MATERIAL_PLACEMENT_REQUIRED");const auto cells=partition(parent,plan.mmPerInputUnit,limits);double total{};AcDbObjectId layer;const auto first=state->tiles.size();
            std::function<void(Rect,unsigned)> prepare=[&](Rect cell,unsigned depth){
                {std::ostringstream context;context<<std::setprecision(17)<<"parent="<<parent.viewIdentity<<"/"<<parent.identity<<"; tile=["<<cell.x0<<','<<cell.y0<<','<<cell.x1<<','<<cell.y1<<"]; splitDepth="<<depth<<"; prototype="<<parent.expression.prototypeId;for(const auto& seed:seeds)if(seed.prototypeId==parent.expression.prototypeId)context<<"; originalReferenceHandle="<<seed.handle;diagnosticLocation=context.str();}
                need(++attempts<=limits.maxAttempts,"TILE_ATTEMPT_BUDGET");need(state->tiles.size()<limits.maxTiles,"TILE_BUDGET");auto childPlan=plan;childPlan.regions={parent};childPlan.regions[0].geometry=geometry(cell);childPlan.regions[0].areaSquareMm=area(cell)*plan.mmPerInputUnit*plan.mmPerInputUnit;need(positive(childPlan.regions[0].areaSquareMm),"NONFINITE_TILE_PHYSICAL_AREA");MaterialPreparationOptions childOptions;childOptions.placements={*placement};auto child=prepareMaterials(childPlan,seeds,childOptions);
                if(!child.canCommit()){
                    const auto reason=child.issues.empty()?std::string("MATERIAL_CHILD_FAILED"):child.issues.front().message;need(reason.find("EMPTY_NATIVE_PATTERN_OUTPUT")!=std::string::npos,reason);need(depth<limits.maxDepth,"TILE_DEPTH_BUDGET");Rect left=cell,right=cell;const bool splitX=(cell.x1-cell.x0)>=(cell.y1-cell.y0);if(splitX){double middle=cell.x0+(cell.x1-cell.x0)*.5;need(middle>cell.x0&&middle<cell.x1,"TILE_FLOATING_POINT_SPLIT_EXHAUSTED");left.x1=middle;right.x0=middle;}else{double middle=cell.y0+(cell.y1-cell.y0)*.5;need(middle>cell.y0&&middle<cell.y1,"TILE_FLOATING_POINT_SPLIT_EXHAUSTED");left.y1=middle;right.y0=middle;}prepare(left,depth+1);prepare(right,depth+1);return;
                }
                need(child.value->intact()&&child.value->roots().length()==1,"INVALID_CHILD_BATCH");auto measured=measure(child.value->database(),child.value->roots()[0],cell);auto ids=cloneVerified(child.value->database(),child.value->roots(),*state->database);need(ids.length()==1,"TILE_ROOT_MAPPING_COUNT");auto copied=measure(*state->database,ids[0],cell);need(close(measured.first,copied.first)&&measured.second==copied.second,"CLONE_NATIVE_OUTPUT_CHANGED");{Open<AcDbHatch> h(ids[0]);if(layer.isNull())layer=h->layerId();need(h->layerId()==layer,"DUPLICATE_PARENT_MATERIAL_LAYER");}state->roots.append(ids[0]);state->tiles.push_back({parent.identity,parent.viewIdentity,geometry(cell),copied.first,copied.second,depth});state->children.push_back(std::move(*child.value));total+=copied.first;
            };
            for(auto cell:cells)prepare(cell,0);need(close(total*plan.mmPerInputUnit*plan.mmPerInputUnit,parent.areaSquareMm),"NATIVE_WHOLE_MATERIAL_AREA_MISMATCH");{Working work(*state->database);validateSeams(state->roots,state->tiles,first,plan.mmPerInputUnit);}
        }
        state->baseline=snapshot(*state->database,state->roots);check(state->database->addReactor(state.get()),"arm immutable tile batch");state->armed=true;return {PreparedMaterialTiles(std::move(state)),{}};
    }catch(const std::exception& e){return {std::nullopt,{{"MATERIAL_TILES_PREPARATION_REJECTED",diagnosticLocation+"; "+e.what(),{},true}}};}
}
}
