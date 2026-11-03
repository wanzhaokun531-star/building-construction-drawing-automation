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
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
struct TestHooks {
    std::vector<AcDbDatabase*> live;
    std::size_t created{},destroyed{};
    bool destructionWhileWorking{},exporting{};
    int fault{-1};
    std::string bindings;
};
thread_local TestHooks* testHooks{};
class HookScope final {
public:explicit HookScope(TestHooks& hooks){need(!testHooks,"NESTED_MATERIAL_TEST_HOOKS");testHooks=&hooks;}~HookScope(){testHooks=nullptr;}
};
void inject(material_tiles_diagnostics::OwnedFault point){if(testHooks&&testHooks->fault==static_cast<int>(point))throw std::runtime_error("INJECTED_OWNED_FAILURE:"+std::to_string(static_cast<int>(point)));}
#endif
using OwnedDbPtr=std::unique_ptr<AcDbDatabase,MaterialDatabaseDeleter>;
OwnedDbPtr newOwnedDatabase(){
    OwnedDbPtr db(new AcDbDatabase(true,true));need(db.get()!=acdbHostApplicationServices()->workingDatabase()&&!documentDatabase(*db),"FRESH_NON_DOCUMENT_DATABASE_REQUIRED");
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
    if(testHooks){testHooks->live.push_back(db.get());++testHooks->created;}
#endif
    return db;
}
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
    Acad::ErrorStatus dataReference(AcDbObjectId id,const char* type,const std::string& context="XrecordTypedPayload");
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
// Exact standard Xrecord uses its complete public DXF stream. The DWG
// implementation (even qualified AcDbObject::dwgOutFields) contains derived
// opaque reference bytes; never infer or filter that internal representation.
class XrecordDxfFiler final:public AcDbDxfFiler {
public:
    XrecordDxfFiler(const AcDbXrecord& record,ValueFiler& values):record_(record),values_(values){}
    int rewindFiler()override{throw std::runtime_error("UNSUPPORTED_XRECORD_DXF_REWIND");}
    Acad::ErrorStatus filerStatus()const override{return Acad::eOk;}void resetFilerStatus()override{}
    Acad::ErrorStatus setError(Acad::ErrorStatus es,const ACHAR*,...)override{throw std::runtime_error("XRECORD_DXF_ERROR:"+std::to_string(es));}
    Acad::ErrorStatus setError(const ACHAR*,...)override{throw std::runtime_error("XRECORD_DXF_ERROR");}
    AcDb::FilerType filerType()const override{return AcDb::kFileFiler;}
    AcDbDatabase* database()const override{return record_.database();}
    Acad::ErrorStatus dwgVersion(AcDb::AcDbDwgVersion& v,AcDb::MaintenanceReleaseVersion& m)const override{v=AcDb::kDHL_CURRENT;m=AcDb::kMReleaseCurrent;return Acad::eOk;}
    bool includesDefaultValues()const override{return true;}
    int precision()const override{return kMaxPrec;}
    void finish()const{need(sawSelf_&&sawOwner_&&payload_&&controls_.empty(),"XRECORD_DXF_BASE_INCOMPLETE; self="+std::to_string(sawSelf_)+"; owner="+std::to_string(sawOwner_)+"; subclass="+std::to_string(payload_));}
    Acad::ErrorStatus writeObjectId(AcDb::DxfCode code,const AcDbObjectId& id)override{
        if(static_cast<int>(code)==5){need(id==record_.objectId(),"XRECORD_DXF_SELF_ID_MISMATCH");return writeAcDbHandle(code,id.handle());}
        const auto type=acdbGroupCodeToType(code);const char* role=type==AcDb::kDwgHardOwnershipId?"HardOwnershipId":type==AcDb::kDwgSoftOwnershipId?"SoftOwnershipId":type==AcDb::kDwgHardPointerId?"HardPointerId":type==AcDb::kDwgSoftPointerId?"SoftPointerId":nullptr;
        need(role!=nullptr,"UNSUPPORTED_XRECORD_DXF_ID_CODE:"+std::to_string(code));need(id.isNull()||id.database()==database(),"XRECORD_DXF_REFERENCE_DATABASE_MISMATCH");tag(code);
        if(!payload_&&!xdata_&&controls_.empty()&&static_cast<int>(code)==330){need(!sawOwner_&&id==record_.ownerId(),"XRECORD_DXF_OWNER_MISMATCH");sawOwner_=true;return values_.reference(id,role);}
        std::string context="DXF group="+std::to_string(code)+";payload="+std::to_string(payload_)+";xdata="+std::to_string(xdata_)+";controls=";for(const auto& control:controls_)context+=diagnosticText(control.c_str())+"/";return values_.dataReference(id,role,context);
    }
    Acad::ErrorStatus writeAcDbHandle(AcDb::DxfCode code,const AcDbHandle& handle)override{
        if(static_cast<int>(code)==5&&!payload_&&!xdata_&&controls_.empty()){need(!sawSelf_&&handle==record_.objectId().handle(),"XRECORD_DXF_SELF_HANDLE_MISMATCH");sawSelf_=true;tag(code);return values_.writeAcDbHandle(handle);}
        const auto type=acdbGroupCodeToType(code);need(isReference(type),"UNSUPPORTED_XRECORD_DXF_HANDLE; group="+std::to_string(code));AcDbObjectId id;if(!handle.isNull())check(database()->getAcDbObjectId(id,false,handle),"resolve public DXF reference handle");return writeObjectId(code,id);
    }
    Acad::ErrorStatus writeString(AcDb::DxfCode code,const AcString& value)override{return writeString(code,value.kACharPtr());}
    Acad::ErrorStatus writeString(AcDb::DxfCode code,const ACHAR* value)override{
        const auto type=acdbGroupCodeToType(code);if(isReference(type)||type==AcDb::kDwgHandle){need(value&&*value,"INVALID_XRECORD_DXF_HANDLE_TEXT");std::size_t n=0;for(const ACHAR* p=value;*p;++p){need(++n<=16&&((*p>=L'0'&&*p<=L'9')||(*p>=L'a'&&*p<=L'f')||(*p>=L'A'&&*p<=L'F')),"INVALID_XRECORD_DXF_HANDLE_TEXT");}return writeAcDbHandle(code,AcDbHandle(value));}
        tag(code);values_.writeString(value);
        if(static_cast<int>(code)==1001){need(value&&*value,"XRECORD_DXF_XDATA_APP_NAME");Open<AcDbRegAppTable> apps(database()->regAppTableId());AcDbObjectId app;check(apps->getAt(value,app),"resolve Xrecord XData RegApp");values_.dataReference(app,"HardPointerId","DXF XDataRegApp1001");}
        if(!payload_&&!xdata_&&static_cast<int>(code)==102){need(value!=nullptr,"INVALID_XRECORD_DXF_CONTROL");if(value[0]==L'{'){need(controls_.size()<32,"XRECORD_DXF_CONTROL_BUDGET");controls_.push_back(value);}else if(wcscmp(value,L"}")==0){need(!controls_.empty(),"XRECORD_DXF_CONTROL_UNBALANCED");controls_.pop_back();}else throw std::runtime_error("UNSUPPORTED_XRECORD_DXF_CONTROL");}
        if(!payload_&&!xdata_&&static_cast<int>(code)==100&&value&&wcscmp(value,L"AcDbXrecord")==0){need(controls_.empty(),"XRECORD_DXF_SUBCLASS_IN_CONTROL");payload_=true;}
        return Acad::eOk;
    }
    Acad::ErrorStatus writeBChunk(AcDb::DxfCode code,const ads_binary& value)override{tag(code);need(value.clen>=0,"INVALID_XRECORD_DXF_BINARY_LENGTH");return values_.writeBChunk(value);}
#define BCE_DXF_SCALAR(Name,Type) Acad::ErrorStatus write##Name(AcDb::DxfCode code,Type value)override{tag(code);return values_.write##Name(value);}
    BCE_DXF_SCALAR(Int8,Adesk::Int8)
    BCE_DXF_SCALAR(Int16,Adesk::Int16)
    BCE_DXF_SCALAR(Int32,Adesk::Int32)
    BCE_DXF_SCALAR(Int64,Adesk::Int64)
    BCE_DXF_SCALAR(UInt8,Adesk::UInt8)
    BCE_DXF_SCALAR(UInt16,Adesk::UInt16)
    BCE_DXF_SCALAR(UInt32,Adesk::UInt32)
    BCE_DXF_SCALAR(UInt64,Adesk::UInt64)
    BCE_DXF_SCALAR(Boolean,Adesk::Boolean)
    BCE_DXF_SCALAR(Bool,bool)
#undef BCE_DXF_SCALAR
    Acad::ErrorStatus writeDouble(AcDb::DxfCode code,double value,int=kDfltPrec)override{tag(code);return values_.writeDouble(value);}
#define BCE_DXF_VECTOR(Name,Type) Acad::ErrorStatus write##Name(AcDb::DxfCode code,const Type& value,int=kDfltPrec)override{tag(code);return values_.write##Name(value);}
    BCE_DXF_VECTOR(Point2d,AcGePoint2d)
    BCE_DXF_VECTOR(Point3d,AcGePoint3d)
    BCE_DXF_VECTOR(Vector2d,AcGeVector2d)
    BCE_DXF_VECTOR(Vector3d,AcGeVector3d)
    BCE_DXF_VECTOR(Scale3d,AcGeScale3d)
#undef BCE_DXF_VECTOR
    Acad::ErrorStatus writeXDataStart()override{xdata_=true;values_.out<<"DXF_XDATA_START;";return Acad::eOk;}
    Acad::ErrorStatus writeEmbeddedObjectStart()override{throw std::runtime_error("UNSUPPORTED_XRECORD_DXF_EMBEDDED_OBJECT");}
    Acad::ErrorStatus writeResBuf(const resbuf& item)override{
        const auto code=static_cast<AcDb::DxfCode>(item.restype);const auto type=acdbGroupCodeToType(code);const auto& value=item.resval;
        switch(type){
        case AcDb::kDwgReal:return writeDouble(code,value.rreal);
        case AcDb::kDwgInt8:tag(code);values_.out<<"Int8Resbuf:"<<value.rint<<';';return Acad::eOk;
        case AcDb::kDwgInt16:return writeInt16(code,value.rint);
        case AcDb::kDwgInt32:return writeInt32(code,value.rlong);
        case AcDb::kDwgInt64:return writeInt64(code,value.mnInt64);
        case AcDb::kDwgText:return writeString(code,value.rstring);
        case AcDb::kDwgBChunk:return writeBChunk(code,value.rbinary);
        case AcDb::kDwg3Real:return writePoint3d(code,{value.rpoint[0],value.rpoint[1],value.rpoint[2]});
        case AcDb::kDwgHardOwnershipId:case AcDb::kDwgSoftOwnershipId:case AcDb::kDwgHardPointerId:case AcDb::kDwgSoftPointerId:{AcDbObjectId id;if(value.rlname[0]!=0||value.rlname[1]!=0)check(acdbGetObjectId(id,value.rlname),"resolve public DXF resbuf reference");return writeObjectId(code,id);}
        default:throw std::runtime_error("UNSUPPORTED_XRECORD_DXF_RESBUF; group="+std::to_string(code)+"; type="+std::to_string(type));
        }
    }
private:
    static bool isReference(AcDb::DwgDataType type){return type==AcDb::kDwgHardOwnershipId||type==AcDb::kDwgSoftOwnershipId||type==AcDb::kDwgHardPointerId||type==AcDb::kDwgSoftPointerId;}
    void tag(AcDb::DxfCode code){need(++count_<=8192&&values_.out.tellp()<=4194304,"XRECORD_DXF_BUDGET");values_.out<<"DXFGroup:"<<code<<';';}
    const AcDbXrecord& record_;ValueFiler& values_;std::vector<std::wstring> controls_;std::size_t count_{};bool sawSelf_{},sawOwner_{},payload_{},xdata_{};
};
void publicXrecordValues(const AcDbXrecord& record,ValueFiler& filer){
    AcDbXrecordIterator iterator(&record);std::size_t count=0;
    for(iterator.start();!iterator.done();){
        need(++count<=4096,"XRECORD_TYPED_ITEM_BUDGET");const int code=iterator.curRestype();const auto type=acdbGroupCodeToType(static_cast<AcDb::DxfCode>(code));
        // Handle-valued items have no verified cross-database binding contract.
        // Reject them before reading rather than guessing a resbuf union field.
        need(type!=AcDb::kDwgHandle&&type!=AcDb::kDwgNull&&type!=AcDb::kDwgNotRecognized,"UNSUPPORTED_XRECORD_TYPED_VALUE; group="+std::to_string(code)+"; type="+std::to_string(type));
        struct Item {resbuf value{};~Item(){acdbFreeResBufContents(&value);}} item;item.value.restype=static_cast<short>(code);check(iterator.getCurResbuf(item.value,record.database()),"public Xrecord typed value");need(item.value.restype==code,"XRECORD_TYPED_GROUP_CHANGED");
        filer.out<<"XrecordItem:"<<count<<",group="<<code<<",type="<<type<<';';const auto& value=item.value.resval;
        switch(type){
        case AcDb::kDwgReal:filer.writeDouble(value.rreal);break;
        case AcDb::kDwgInt8:filer.out<<"Int8Resbuf:"<<value.rint<<';';break;
        case AcDb::kDwgInt16:filer.writeInt16(value.rint);break;
        case AcDb::kDwgInt32:filer.writeInt32(value.rlong);break;
        case AcDb::kDwgInt64:filer.writeInt64(value.mnInt64);break;
        case AcDb::kDwgText:filer.writeString(value.rstring);break;
        case AcDb::kDwgBChunk:need(value.rbinary.clen>=0,"INVALID_XRECORD_BINARY_LENGTH");filer.writeBChunk(value.rbinary);break;
        case AcDb::kDwg3Real:filer.writePoint3d({value.rpoint[0],value.rpoint[1],value.rpoint[2]});break;
        case AcDb::kDwgHardOwnershipId:case AcDb::kDwgSoftOwnershipId:case AcDb::kDwgHardPointerId:case AcDb::kDwgSoftPointerId:{
            AcDbObjectId id;if(value.rlname[0]!=0||value.rlname[1]!=0)check(acdbGetObjectId(id,value.rlname),"resolve public Xrecord object reference");need(id.isNull()||id.database()==record.database(),"XRECORD_REFERENCE_DATABASE_MISMATCH");const char* role=type==AcDb::kDwgHardOwnershipId?"HardOwnershipId":type==AcDb::kDwgSoftOwnershipId?"SoftOwnershipId":type==AcDb::kDwgHardPointerId?"HardPointerId":"SoftPointerId";filer.dataReference(id,role);break;
        }
        default:throw std::runtime_error("UNSUPPORTED_XRECORD_TYPED_VALUE; group="+std::to_string(code)+"; type="+std::to_string(type));
        }
        need(filer.out.tellp()<=4194304,"XRECORD_TYPED_BYTES_BUDGET");const auto status=iterator.next();need(status==Acad::eOk||(status==Acad::eIteratorDone&&iterator.done()),"XRECORD_TYPED_ITERATOR_FAILED:"+std::to_string(status));
    }
    filer.out<<"XrecordItemCount:"<<count<<';';
}
struct Graph {
    struct Edge {AcDbObjectId from,to;std::string role,context;};
    std::vector<AcDbObjectId> active,resources;
    std::vector<Edge> edges;
    std::vector<std::string> diagnosticPath,resourcePaths;
    std::size_t bytes{};
    std::string value(AcDbObjectId id,bool root=false,const char* role="root-or-resource-check"){
        need(!id.isNull()&&!id.isErased(),"INVALID_RESOURCE_REFERENCE");need(active.size()<32&&resources.size()<512,"RESOURCE_GRAPH_BUDGET");auto cycle=std::find(active.begin(),active.end(),id);if(cycle!=active.end())return "cycle-depth:"+std::to_string(std::distance(active.begin(),cycle));
        Open<AcDbObject> object(id);const auto node=diagnosticNode(*object,role,active.size());if(!(standardResource(object->isA())||(root&&object->isA()==AcDbHatch::desc()))){std::string trace;for(const auto& ancestor:diagnosticPath)trace+="["+ancestor+"] -> ";throw std::runtime_error("UNSUPPORTED_RESOURCE_CLASS; traversal="+trace+"["+node+"]; rejectedNodeFilerStatus=not-entered");}if(!root&&std::find(resources.begin(),resources.end(),id)==resources.end()){resources.push_back(id);std::string path;for(const auto& ancestor:diagnosticPath)path+="["+ancestor+"] -> ";resourcePaths.push_back(path+"["+node+"]");}
        active.push_back(id);diagnosticPath.push_back(node);struct Pop{std::vector<AcDbObjectId>& stack;std::vector<std::string>& path;~Pop(){path.pop_back();stack.pop_back();}} pop{active,diagnosticPath};ValueFiler filer(*this,id,object->ownerId());filer.writeString(object->isA()->name());
        Acad::ErrorStatus filingStatus;
        if(object->isA()==AcDbXrecord::desc()){
            const auto* record=AcDbXrecord::cast(&*object);filer.out<<"XrecordReferenceTranslation:"<<record->isXlateReferences()<<";XrecordMergeStyle:"<<record->mergeStyle()<<';';
            XrecordDxfFiler dxf(*record,filer);filingStatus=record->dxfOut(&dxf,true,nullptr);check(filingStatus,"complete public Xrecord DXF");dxf.finish();publicXrecordValues(*record,filer);
        }else filingStatus=object->dwgOutFields(&filer);
        need(filingStatus==Acad::eOk&&filer.filerStatus()==Acad::eOk,"PUBLIC_RESOURCE_FILER_FAILED; "+node+"; callStatus="+std::to_string(filingStatus)+"; filerStatus="+std::to_string(filer.filerStatus()));auto result=filer.out.str();need(result.size()<=4194304&&bytes<=16777216-result.size(),"RESOURCE_GRAPH_BYTES_BUDGET");bytes+=result.size();return result;
    }
};
Acad::ErrorStatus ValueFiler::reference(AcDbObjectId id,const char* type){out<<type<<':';if(id.isNull())out<<"null;";else if(id==self_)out<<"self;";else if(id==owner_)out<<"owner;";else{need(graph_.edges.size()<16384,"RESOURCE_EDGE_BUDGET");graph_.edges.push_back({self_,id,type,"DWGTypedReference"});const auto edge=std::string(type)+"(source="+diagnosticHandle(self_)+",filerStatus="+std::to_string(status_)+")";out<<'{'<<graph_.value(id,false,edge.c_str())<<"};";}return Acad::eOk;}
Acad::ErrorStatus ValueFiler::dataReference(AcDbObjectId id,const char* type,const std::string& context){
    // Unlike the base object's structural self/owner fields, every non-null
    // Xrecord data reference remains an explicit edge, including self/owner.
    out<<type<<':';if(id.isNull()){out<<"null;";return Acad::eOk;}need(graph_.edges.size()<16384,"RESOURCE_EDGE_BUDGET");graph_.edges.push_back({self_,id,type,context});const auto edge=std::string(type)+"(XrecordDataSource="+diagnosticHandle(self_)+")";out<<'{'<<graph_.value(id,false,edge.c_str())<<"};";return Acad::eOk;
}
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
using ResourcePairs=std::vector<std::pair<AcDbObjectId,AcDbObjectId>>;
ResourcePairs preflight(const AcDbObjectIdArray& roots,AcDbDatabase& target){Graph graph;for(int i=0;i<roots.length();++i)graph.value(roots[i],true);ResourcePairs existing;for(auto resource:graph.resources){auto destination=targetResource(resource,target);if(!destination.isNull()){equalResource(resource,destination,"RESOURCE_CONFLICT");existing.emplace_back(resource,destination);}}return existing;}
// SDK map and verified owned-path bindings are separate evidence. Never insert
// synthetic entries into AcDbIdMapping. One source graph is checked at a time.
struct VerifiedBinding {
    AcDbObjectId source,target,reusedSource,reusedTarget;
    bool sdkPair{},cloned{};
};
class BindingVerifier final {
public:
    BindingVerifier(AcDbIdMapping& mapping,AcDbDatabase& target,const ResourcePairs& existing,const AcDbObjectIdArray& roots)
        :mapping_(mapping),target_(target),existing_(existing),roots_(roots){}
    VerifiedBinding bind(AcDbObjectId source){
        try{return bindImpl(source);}catch(const std::exception& e){throw std::runtime_error("VERIFIED_BINDING_SOURCE="+diagnosticHandle(source)+"; "+e.what());}
    }
private:
    VerifiedBinding bindImpl(AcDbObjectId source){
        for(const auto& b:bindings_)if(b.source==source)return b;
        need(bindings_.size()<4096&&active_.size()<32,"VERIFIED_BINDING_BUDGET");need(std::find(active_.begin(),active_.end(),source)==active_.end(),"RESOURCE_OWNER_CYCLE");active_.push_back(source);struct Pop{std::vector<AcDbObjectId>& v;~Pop(){v.pop_back();}} pop{active_};
        const bool root=roots_.contains(source);Open<AcDbObject> from(source);need(root?from->isA()==AcDbHatch::desc():standardResource(from->isA()),"UNSUPPORTED_BINDING_CLASS");
        AcDbIdPair pair;pair.setKey(source);const bool computed=mapping_.compute(pair);VerifiedBinding binding;binding.source=source;binding.sdkPair=computed;
        if(computed){
            need(!pair.value().isNull()&&pair.value().database()==&target_&&!pair.value().isErased(),"INVALID_SDK_RESOURCE_MAPPING");binding.target=pair.value();binding.cloned=pair.isCloned();
            if(root)need(pair.isCloned(),"ROOT_NOT_CLONED");
            if(!pair.isCloned()){
                const auto found=std::find(existing_.begin(),existing_.end(),std::make_pair(source,pair.value()));need(found!=existing_.end(),"UNPROVEN_REUSED_SDK_RESOURCE");binding.reusedSource=source;binding.reusedTarget=pair.value();
            }
        }else{
            need(!root&&!from->ownerId().isNull(),"RESOURCE_MAPPING_MISSING_WITHOUT_OWNER");const auto parent=bind(from->ownerId());need(!parent.reusedSource.isNull(),"RESOURCE_MAPPING_MISSING_UNDER_NEW_PARENT");
            binding.target=targetResource(source,target_);need(!binding.target.isNull(),"RESOURCE_OWNED_PATH_MISSING");ownedPair(source,binding.target,parent.source,parent.target);binding.reusedSource=parent.reusedSource;binding.reusedTarget=parent.reusedTarget;
        }
        Open<AcDbObject> to(binding.target);need(to->isA()==from->isA(),"RESOURCE_BINDING_CLASS_MISMATCH");
        if(root)need(to->ownerId()==acdbSymUtil()->blockModelSpaceId(&target_),"ROOT_TARGET_OWNER_MISMATCH");
        else need(targetResource(source,target_)==binding.target,"RESOURCE_BINDING_LOCATION_MISMATCH");
        // Check actual owner, not just an independent same-name lookup.
        if(!root){const auto owner=from->ownerId();auto* sourceDb=from->database();AcDbObjectId expectedOwner;
            if(owner==sourceDb->layerTableId())expectedOwner=target_.layerTableId();else if(owner==sourceDb->linetypeTableId())expectedOwner=target_.linetypeTableId();else if(owner==sourceDb->regAppTableId())expectedOwner=target_.regAppTableId();else if(owner==sourceDb->materialDictionaryId())expectedOwner=target_.materialDictionaryId();else if(owner==sourceDb->plotStyleNameDictionaryId())expectedOwner=target_.plotStyleNameDictionaryId();else expectedOwner=bind(owner).target;
            need(to->ownerId()==expectedOwner,"RESOURCE_BINDING_OWNER_MISMATCH");
        }
        equalResource(source,binding.target,"RESOURCE_BINDING_DEFINITION_CONFLICT",root);
        for(const auto& b:bindings_)need(b.target!=binding.target||b.source==source,"RESOURCE_BINDING_ALIAS_COLLISION");bindings_.push_back(binding);return binding;
    }
public:
    void verifyEdges(const Graph& from,const Graph& to){
        need(from.edges.size()==to.edges.size(),"RESOURCE_REFERENCE_EDGE_COUNT");
        for(std::size_t i=0;i<from.edges.size();++i){const auto& a=from.edges[i];const auto& b=to.edges[i];VerifiedBinding child;
            try{const auto owner=bind(a.from);child=bind(a.to);need(a.role==b.role&&a.context==b.context&&owner.target==b.from&&child.target==b.to,"RESOURCE_REFERENCE_BINDING_MISMATCH");
                if(!child.sdkPair){
                    // The incoming edge does not establish a missing SDK map.
                    // Recheck the independently established owner chain to an
                    // actual, preflight-verified reused SDK pair first.
                    recheckOwnedPath(child);
                    if(a.role=="HardOwnershipId"||a.role=="SoftOwnershipId")ownedPair(a.to,b.to,a.from,b.from);
                    else{need(a.role=="HardPointerId"||a.role=="SoftPointerId","UNSUPPORTED_NONOWNING_REFERENCE_ROLE");
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
                        if(edgeProofs_.size()<32768)edgeProofs_+="[VerifiedNonowningTarget; "+edgeDiagnostic(a,b,child)+"];";
#endif
                    }
                }
            }catch(const std::exception& error){throw std::runtime_error(std::string(error.what())+"; edgeIndex="+std::to_string(i)+"; "+edgeDiagnostic(a,b,child));}
        }
    }
    std::string summary()const{std::ostringstream out;for(const auto& b:bindings_)out<<bindingDiagnostic(b);
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
        out<<edgeProofs_;
#endif
        return out.str();}
private:
    static std::string bindingDiagnostic(const VerifiedBinding& b){std::ostringstream out;out<<"[source="<<diagnosticHandle(b.source)<<",target="<<diagnosticHandle(b.target)<<",kind="<<(b.sdkPair?"SdkPair":"VerifiedOwnedPath")<<",cloned="<<b.cloned<<",reusedSource="<<diagnosticHandle(b.reusedSource)<<",reusedTarget="<<diagnosticHandle(b.reusedTarget)<<"];";return out.str();}
    static std::string edgeDiagnostic(const Graph::Edge& a,const Graph::Edge& b,const VerifiedBinding& child){
        try{Open<AcDbObject> af(a.from),at(a.to),bf(b.from),bt(b.to);return "sourceEdge=[role="+a.role+",context="+a.context+",from={"+diagnosticNode(*af,"edge-from",0)+"},to={"+diagnosticNode(*at,"edge-to",0)+"}]; targetEdge=[role="+b.role+",context="+b.context+",from={"+diagnosticNode(*bf,"edge-from",0)+"},to={"+diagnosticNode(*bt,"edge-to",0)+"}]; child="+bindingDiagnostic(child);}
        catch(const std::exception& detail){return "sourceEdge=[role="+a.role+",context="+a.context+",from="+diagnosticHandle(a.from)+",to="+diagnosticHandle(a.to)+"]; targetEdge=[role="+b.role+",context="+b.context+",from="+diagnosticHandle(b.from)+",to="+diagnosticHandle(b.to)+"]; child="+bindingDiagnostic(child)+"; diagnosticDetailError="+detail.what();}
    }
    void recheckOwnedPath(const VerifiedBinding& child){
        need(!child.sdkPair&&!child.reusedSource.isNull()&&!child.reusedTarget.isNull(),"MISSING_SDK_PAIR_WITHOUT_VERIFIED_OWNED_PATH");auto current=child;std::size_t depth=0;
        while(!current.sdkPair){need(++depth<=32&&current.reusedSource==child.reusedSource&&current.reusedTarget==child.reusedTarget,"OWNED_PATH_ANCHOR_OR_DEPTH_MISMATCH");Open<AcDbObject> source(current.source);need(!source->ownerId().isNull(),"OWNED_PATH_OWNER_MISSING");const auto parent=bind(source->ownerId());ownedPair(current.source,current.target,parent.source,parent.target);current=parent;}
        need(!current.cloned&&current.source==child.reusedSource&&current.target==child.reusedTarget,"OWNED_PATH_REUSED_ANCHOR_MISMATCH");AcDbIdPair pair;pair.setKey(current.source);need(mapping_.compute(pair)&&!pair.isCloned()&&pair.value()==current.target,"OWNED_PATH_ANCHOR_SDK_PAIR_CHANGED");need(std::find(existing_.begin(),existing_.end(),std::make_pair(current.source,current.target))!=existing_.end(),"OWNED_PATH_ANCHOR_NOT_PREFLIGHT_VERIFIED");
        // bindImpl already checked every node's full definition, exact class,
        // unique target and location in this read-only verification scope.
    }
    static void ownedPair(AcDbObjectId source,AcDbObjectId target,AcDbObjectId sourceParent,AcDbObjectId targetParent){
        Open<AcDbObject> a(source),b(target),pa(sourceParent),pb(targetParent);need(a->ownerId()==sourceParent&&b->ownerId()==targetParent&&pa->isA()==pb->isA()&&a->isA()==b->isA(),"OWNED_BINDING_REVERSE_OWNER_OR_CLASS");
        if(pa->extensionDictionary()==source){need(a->isA()==AcDbDictionary::desc()&&pb->extensionDictionary()==target,"OWNED_EXTENSION_FORWARD_MISMATCH");return;}
        need(pa->isA()==AcDbDictionary::desc()&&pb->isA()==AcDbDictionary::desc(),"UNSUPPORTED_OWNED_BINDING_EDGE");auto* da=AcDbDictionary::cast(&*pa);auto* db=AcDbDictionary::cast(&*pb);AcString ka,kb;check(da->nameAt(source,ka),"source owned key");check(db->nameAt(target,kb),"target reverse owned key");need(ka==kb,"OWNED_DICTIONARY_KEY_MISMATCH");AcDbObjectId ia,ib;check(da->getAt(ka.kACharPtr(),ia),"source forward key");check(db->getAt(kb.kACharPtr(),ib),"target forward key");need(ia==source&&ib==target,"OWNED_DICTIONARY_FORWARD_MISMATCH");
    }
    AcDbIdMapping& mapping_;AcDbDatabase& target_;const ResourcePairs& existing_;const AcDbObjectIdArray& roots_;std::vector<VerifiedBinding> bindings_;std::vector<AcDbObjectId> active_;
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
    std::string edgeProofs_;
#endif
};
// Only private owned targets reach this core. Failure abandons the entire owner;
// this function makes no claim that wblockClone changes were rolled back.
AcDbObjectIdArray cloneVerified(AcDbDatabase& source,const AcDbObjectIdArray& input,AcDbDatabase& target){
    const auto existing=preflight(input,target);Working work(target);AcDbIdMapping mapping;const auto status=source.wblockCloneObjects(input,acdbSymUtil()->blockModelSpaceId(&target),mapping,AcDb::kDrcIgnore);check(status,"clone verified material tiles");
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
    if(testHooks&&testHooks->exporting)inject(material_tiles_diagnostics::OwnedFault::AfterExportClone);
#endif
    BindingVerifier verifier(mapping,target,existing,input);AcDbObjectIdArray result;Graph from,to;
    for(int i=0;i<input.length();++i){const auto binding=verifier.bind(input[i]);result.append(binding.target);const auto a=from.value(input[i],true),b=to.value(binding.target,true);need(a==b,"CLONED_NATIVE_EXPRESSION_CHANGED");}
    for(auto resource:from.resources)verifier.bind(resource);verifier.verifyEdges(from,to);
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
    if(testHooks&&testHooks->bindings.size()<262144)testHooks->bindings+=verifier.summary();
#endif
    return result;
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
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
namespace material_tiles_diagnostics {
// Test-only census of every live object handle in a fresh owned database. Full
// opaque payload is compared for model Hatches, layers and Xrecords; every
// dictionary is compared by its complete public key/ID/ownership contract.
std::string resourceCensus(AcDbDatabase& db){
    Working working(db);const auto seed=db.handseed();need(seed.high()==0&&seed.low()<=65536,"ABORT_PROBE_HANDLE_BUDGET");const auto roots=modelRoots(db);std::ostringstream out;out.imbue(std::locale::classic());out<<"modelRoots=";for(int i=0;i<roots.length();++i)out<<diagnosticHandle(roots[i])<<',';out<<';';
    for(Adesk::UInt64 value=1;value<seed.low();++value){AcDbObjectId id;const auto lookup=db.getAcDbObjectId(id,false,AcDbHandle(value));if(lookup==Acad::eUnknownHandle)continue;check(lookup,"abort census handle lookup");if(id.isNull()||id.isErased())continue;Open<AcDbObject> object(id);out<<"[handle="<<diagnosticHandle(id)<<",class="<<diagnosticText(object->isA()->name())<<",owner="<<diagnosticHandle(object->ownerId())<<",extension="<<diagnosticHandle(object->extensionDictionary())<<';';
        if(object->isKindOf(AcDbDictionary::desc())){auto* dictionary=AcDbDictionary::cast(&*object);out<<"dictionary-count="<<dictionary->numEntries()<<",hard="<<dictionary->isTreatElementsAsHard()<<",merge="<<dictionary->mergeStyle()<<';';std::unique_ptr<AcDbDictionaryIterator> iterator(dictionary->newIterator());need(iterator!=nullptr,"abort census dictionary iterator");std::vector<std::pair<std::wstring,AcDbObjectId>> entries;for(;!iterator->done();iterator->next()){need(entries.size()<4096,"ABORT_PROBE_DICTIONARY_BUDGET");entries.push_back({iterator->name(),iterator->objectId()});}std::sort(entries.begin(),entries.end(),[](const auto& a,const auto& b){return a.first<b.first;});for(const auto& entry:entries){Open<AcDbObject> child(entry.second);out<<"key="<<diagnosticText(entry.first.c_str())<<",id="<<diagnosticHandle(entry.second)<<",owner="<<diagnosticHandle(child->ownerId())<<",class="<<diagnosticText(child->isA()->name())<<';';}}
        if(object->isA()==AcDbLayerTableRecord::desc()||object->isA()==AcDbXrecord::desc()||roots.contains(id)){Graph graph;const auto data=graph.value(id,roots.contains(id));out<<"opaqueLength="<<data.size()<<':'<<data;}out<<"];";need(out.tellp()<=4194304,"ABORT_PROBE_CENSUS_BYTES_BUDGET");
    }return out.str();
}
}
#endif
void MaterialDatabaseDeleter::operator()(AcDbDatabase* db)const noexcept{
    if(!db)return;
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
    bool observed=false;if(testHooks){const auto it=std::find(testHooks->live.begin(),testHooks->live.end(),db);if(it!=testHooks->live.end()){observed=true;testHooks->destructionWhileWorking|=acdbHostApplicationServices()->workingDatabase()==db;testHooks->live.erase(it);}}
#endif
    delete db;
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
    if(observed)++testHooks->destroyed; // Witness after the actual SDK destructor.
#endif
}
struct PreparedMaterialTiles::State:AcDbDatabaseReactor {
    OwnedDbPtr database;AcDbObjectIdArray roots;MaterialPlan parent;std::vector<MaterialTileRecord> tiles;std::vector<PreparedMaterialBatch> children;std::set<AcDbDatabase*> sources;std::string baseline;bool armed{},changed{};
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
Result<OwnedMaterialDatabase> PreparedMaterialTiles::cloneIntoNewOwnedDb()const{
    try{need(intact(),"PREPARED_MATERIAL_TILES_CHANGED");OwnedMaterialDatabase result;result.database=newOwnedDatabase();need(result.database.get()!=state_->database.get()&&state_->sources.count(result.database.get())==0,"NEW_EXPORT_IDENTITY_REQUIRED");result.roots=cloneVerified(*state_->database,state_->roots,*result.database);need(modelRoots(*result.database).length()==result.roots.length()&&static_cast<std::size_t>(result.roots.length())==state_->tiles.size(),"EXTRA_OR_MISSING_CLONED_ROOT");
        for(int i=0;i<result.roots.length();++i){const auto cell=rectangle(state_->tiles[static_cast<std::size_t>(i)].geometry.outer);const auto measured=measure(*result.database,result.roots[i],cell);need(close(measured.first,state_->tiles[static_cast<std::size_t>(i)].nativeArea)&&measured.second==state_->tiles[static_cast<std::size_t>(i)].nativeLines,"EXPORT_NATIVE_OUTPUT_CHANGED");}
        need(intact(),"PREPARED_MATERIAL_TILES_CHANGED_DURING_EXPORT");
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
        inject(material_tiles_diagnostics::OwnedFault::BeforeExportPublish);
#endif
        return {std::move(result),{}};
    }catch(const std::exception& e){return {std::nullopt,{{"MATERIAL_TILES_OWNED_EXPORT_REJECTED",e.what(),{},true}}};}
}
Result<PreparedMaterialTiles> prepareMaterialTiles(const MaterialPlan& plan,const std::vector<ReferenceMaterialSeed>& seeds,const MaterialPreparationOptions& options,const MaterialTileLimits& limits){
    std::string diagnosticLocation;
    try{
        need(positive(plan.mmPerInputUnit)&&!plan.regions.empty()&&!plan.unitConfirmationSource.empty(),"MATERIAL_FACTS_REQUIRED");need(limits.maxTiles>0&&limits.maxTiles<=4096&&limits.maxAttempts>0&&limits.maxAttempts<=8192&&limits.maxDepth<=32,"INVALID_TILE_LIMITS");need(options.placements.size()==plan.regions.size(),"EXPLICIT_MATERIAL_PLACEMENT_REQUIRED");auto state=std::make_unique<PreparedMaterialTiles::State>();state->database=newOwnedDatabase();state->parent=plan;for(const auto& seed:seeds)if(seed.database)state->sources.insert(seed.database);std::size_t attempts{};std::set<std::pair<std::string,std::string>> identities;
        for(const auto& parent:plan.regions){need(identities.emplace(parent.viewIdentity,parent.identity).second,"DUPLICATE_MATERIAL_REGION");const MaterialPlacement* placement{};for(const auto& p:options.placements)if(p.regionIdentity==parent.identity&&p.viewIdentity==parent.viewIdentity){need(!placement,"DUPLICATE_PLACEMENT");placement=&p;}need(placement!=nullptr,"MATERIAL_PLACEMENT_REQUIRED");const auto cells=partition(parent,plan.mmPerInputUnit,limits);double total{};AcDbObjectId layer;const auto first=state->tiles.size();
            std::function<void(Rect,unsigned)> prepare=[&](Rect cell,unsigned depth){
                {std::ostringstream context;context<<std::setprecision(17)<<"parent="<<parent.viewIdentity<<"/"<<parent.identity<<"; tile=["<<cell.x0<<','<<cell.y0<<','<<cell.x1<<','<<cell.y1<<"]; splitDepth="<<depth<<"; prototype="<<parent.expression.prototypeId;for(const auto& seed:seeds)if(seed.prototypeId==parent.expression.prototypeId)context<<"; originalReferenceHandle="<<seed.handle;diagnosticLocation=context.str();}
                need(++attempts<=limits.maxAttempts,"TILE_ATTEMPT_BUDGET");need(state->tiles.size()<limits.maxTiles,"TILE_BUDGET");auto childPlan=plan;childPlan.regions={parent};childPlan.regions[0].geometry=geometry(cell);childPlan.regions[0].areaSquareMm=area(cell)*plan.mmPerInputUnit*plan.mmPerInputUnit;need(positive(childPlan.regions[0].areaSquareMm),"NONFINITE_TILE_PHYSICAL_AREA");MaterialPreparationOptions childOptions;childOptions.placements={*placement};auto child=prepareMaterials(childPlan,seeds,childOptions);
                if(!child.canCommit()){
                    const auto reason=child.issues.empty()?std::string("MATERIAL_CHILD_FAILED"):child.issues.front().message;need(reason.find("EMPTY_NATIVE_PATTERN_OUTPUT")!=std::string::npos,reason);need(depth<limits.maxDepth,"TILE_DEPTH_BUDGET");Rect left=cell,right=cell;const bool splitX=(cell.x1-cell.x0)>=(cell.y1-cell.y0);if(splitX){double middle=cell.x0+(cell.x1-cell.x0)*.5;need(middle>cell.x0&&middle<cell.x1,"TILE_FLOATING_POINT_SPLIT_EXHAUSTED");left.x1=middle;right.x0=middle;}else{double middle=cell.y0+(cell.y1-cell.y0)*.5;need(middle>cell.y0&&middle<cell.y1,"TILE_FLOATING_POINT_SPLIT_EXHAUSTED");left.y1=middle;right.y0=middle;}prepare(left,depth+1);prepare(right,depth+1);return;
                }
                need(child.value->intact()&&child.value->roots().length()==1,"INVALID_CHILD_BATCH");auto measured=measure(child.value->database(),child.value->roots()[0],cell);auto ids=cloneVerified(child.value->database(),child.value->roots(),*state->database);need(ids.length()==1,"TILE_ROOT_MAPPING_COUNT");auto copied=measure(*state->database,ids[0],cell);need(close(measured.first,copied.first)&&measured.second==copied.second,"CLONE_NATIVE_OUTPUT_CHANGED");{Open<AcDbHatch> h(ids[0]);if(layer.isNull())layer=h->layerId();need(h->layerId()==layer,"DUPLICATE_PARENT_MATERIAL_LAYER");}state->roots.append(ids[0]);state->tiles.push_back({parent.identity,parent.viewIdentity,geometry(cell),copied.first,copied.second,depth});state->children.push_back(std::move(*child.value));total+=copied.first;
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
                if(state->tiles.size()==1)inject(material_tiles_diagnostics::OwnedFault::AfterFirstTile);if(state->tiles.size()==2)inject(material_tiles_diagnostics::OwnedFault::AfterSecondTile);
#endif
            };
            for(auto cell:cells)prepare(cell,0);need(close(total*plan.mmPerInputUnit*plan.mmPerInputUnit,parent.areaSquareMm),"NATIVE_WHOLE_MATERIAL_AREA_MISMATCH");{Working work(*state->database);validateSeams(state->roots,state->tiles,first,plan.mmPerInputUnit);}
        }
        state->baseline=snapshot(*state->database,state->roots);check(state->database->addReactor(state.get()),"arm immutable tile batch");state->armed=true;return {PreparedMaterialTiles(std::move(state)),{}};
    }catch(const std::exception& e){return {std::nullopt,{{"MATERIAL_TILES_PREPARATION_REJECTED",diagnosticLocation+"; "+e.what(),{},true}}};}
}
#ifdef BCE_MATERIAL_TILE_DIAGNOSTICS
namespace material_tiles_diagnostics {
namespace {
void finishProbe(OwnedProbe& result,const TestHooks& hooks,AcDbDatabase* working){result.created=hooks.created;result.destroyed=hooks.destroyed;result.destroyedAll=hooks.created>0&&hooks.created==hooks.destroyed&&hooks.live.empty()&&!hooks.destructionWhileWorking;result.workingRestored=acdbHostApplicationServices()->workingDatabase()==working;result.bindings=hooks.bindings;}
AcDbObjectId firstXrecord(AcDbObjectId dictionaryId){Open<AcDbDictionary> dictionary(dictionaryId);std::unique_ptr<AcDbDictionaryIterator> it(dictionary->newIterator());need(it!=nullptr,"CONFLICT_FIXTURE_ITERATOR");for(;!it->done();it->next()){Open<AcDbObject> object(it->objectId());if(object->isA()==AcDbXrecord::desc())return it->objectId();}throw std::runtime_error("CONFLICT_FIXTURE_XRECORD_REQUIRED");}
void extraKey(AcDbObjectId dictionaryId){Open<AcDbDictionary> dictionary(dictionaryId,AcDb::kForWrite);auto object=std::make_unique<AcDbPlaceHolder>();AcDbObjectId id;check(dictionary->setAt(L"BCE_M2_CONFLICT_EXTRA",object.get(),id),"conflict extra dictionary key");object.release()->close();}
void setReferencePair(AcDbObjectId recordId,AcDbObjectId first,AcDbObjectId second){
    resbuf a{},b{};a.restype=340;b.restype=340;a.rbnext=&b;check(acdbGetAdsName(a.resval.rlname,first),"first public hard pointer");check(acdbGetAdsName(b.resval.rlname,second),"second public hard pointer");Open<AcDbXrecord> record(recordId,AcDb::kForWrite);record->setXlateReferences(true);check(record->setFromRbChain(a),"public hard-pointer Xrecord fixture");
}
void requireReferencePair(AcDbObjectId recordId,AcDbObjectId first,AcDbObjectId second){
    Open<AcDbXrecord> record(recordId);need(record->isA()==AcDbXrecord::desc()&&record->isXlateReferences(),"ALIAS_FIXTURE_XRECORD_CLASS_POLICY");AcDbXrecordIterator it(&*record);it.start();for(auto expected:{first,second}){need(!it.done()&&it.curRestype()==340,"ALIAS_FIXTURE_REFERENCE_TYPE_COUNT");resbuf value{};check(it.getCurResbuf(value,record->database()),"read public fixture reference");AcDbObjectId actual;check(acdbGetObjectId(actual,value.resval.rlname),"resolve public fixture reference");need(actual==expected,"ALIAS_FIXTURE_REFERENCE_READBACK");const auto status=it.next();need(status==Acad::eOk||(status==Acad::eIteratorDone&&it.done()),"ALIAS_FIXTURE_ITERATOR_END");}need(it.done(),"ALIAS_FIXTURE_EXTRA_REFERENCE");
}
OwnedProbe referenceAlias(const PreparedMaterialTiles& batch){
    OwnedProbe report;auto* working=acdbHostApplicationServices()->workingDatabase();TestHooks hooks;HookScope scope(hooks);
    try{
        // Editable owned source copy and owned target only. No plot-style
        // operation: Xrecord's documented group 340 is a public hard pointer.
        auto original=batch.cloneIntoNewOwnedDb();need(original.canCommit(),"ALIAS_FIXTURE_SOURCE_COPY_FAILED");auto source=std::move(*original.value);auto target=newOwnedDatabase();const bool sourceMode=source.database->plotStyleMode(),targetMode=target->plotStyleMode();Working sourceWork(*source.database);AcDbObjectId dictionaryId,a,b,recordId;
        {Open<AcDbHatch> root(source.roots[0]);Open<AcDbLayerTableRecord> layer(root->layerId());dictionaryId=layer->extensionDictionary();need(!dictionaryId.isNull(),"ALIAS_FIXTURE_LAYER_DICTIONARY_REQUIRED");}
        {Open<AcDbDictionary> dictionary(dictionaryId,AcDb::kForWrite);auto one=std::make_unique<AcDbPlaceHolder>();check(dictionary->setAt(L"BCE_M2_ALIAS_A",one.get(),a),"alias A dictionary fixture");one.release()->close();auto two=std::make_unique<AcDbPlaceHolder>();check(dictionary->setAt(L"BCE_M2_ALIAS_B",two.get(),b),"alias B dictionary fixture");two.release()->close();auto record=std::make_unique<AcDbXrecord>();check(dictionary->setAt(L"BCE_M2_ALIAS_REFERENCES",record.get(),recordId),"alias Xrecord dictionary fixture");record.release()->close();}
        need(a!=b,"ALIAS_FIXTURE_DISTINCT_IDS");equalResource(a,b,"ALIAS_FIXTURE_PLACEHOLDER_PAYLOAD_NOT_EQUAL");setReferencePair(recordId,a,b);requireReferencePair(recordId,a,b);
        const auto cloned=cloneVerified(*source.database,source.roots,*target);const auto mappedA=targetResource(a,*target),mappedB=targetResource(b,*target),mappedRecord=targetResource(recordId,*target);need(!mappedA.isNull()&&!mappedB.isNull()&&mappedA!=mappedB&&!mappedRecord.isNull(),"ALIAS_FIXTURE_TARGET_PATHS");requireReferencePair(mappedRecord,mappedA,mappedB);equalResource(mappedA,mappedB,"ALIAS_FIXTURE_TARGET_PAYLOAD_NOT_EQUAL");setReferencePair(mappedRecord,mappedB,mappedB);requireReferencePair(mappedRecord,mappedB,mappedB);requireReferencePair(recordId,a,b);
        hooks.bindings+="ALIAS_FIXTURE source[A="+diagnosticHandle(a)+",B="+diagnosticHandle(b)+",record="+diagnosticHandle(recordId)+"] target[A="+diagnosticHandle(mappedA)+",B="+diagnosticHandle(mappedB)+",record="+diagnosticHandle(mappedRecord)+"];sourceRefs=A,B;targetRefs=B,B;";
        for(int i=0;i<cloned.length();++i){Open<AcDbEntity> root(cloned[i],AcDb::kForWrite);check(root->erase(),"clear only alias target fixture roots");}const auto before=resourceCensus(*target);
        try{cloneVerified(*source.database,source.roots,*target);report.issue="ALIAS_NOT_REJECTED";}catch(const std::exception& e){report.noResult=true;report.issue=e.what();report.preflightUnchanged=resourceCensus(*target)==before;}
        need(source.database->plotStyleMode()==sourceMode&&target->plotStyleMode()==targetMode,"ALIAS_FIXTURE_PLOTSTYLE_MODE_CHANGED");need(batch.intact(),"ALIAS_FIXTURE_MUTATED_IMMUTABLE_BATCH");
    }catch(const std::exception& e){report.noResult=false;report.issue=std::string("CONFLICT_FIXTURE_FAILED: ")+e.what();}
    finishProbe(report,hooks,working);return report;
}
}
OwnedProbe ownershipFault(const MaterialPlan& plan,const std::vector<ReferenceMaterialSeed>& seeds,const MaterialPreparationOptions& options,OwnedFault point){
    OwnedProbe report;auto* working=acdbHostApplicationServices()->workingDatabase();const bool exportFault=point==OwnedFault::AfterExportClone||point==OwnedFault::BeforeExportPublish;
    // Export setup is outside the observed scope; only the new output is counted.
    auto prepared=exportFault?prepareMaterialTiles(plan,seeds,options):Result<PreparedMaterialTiles>{std::nullopt,{}};
    if(exportFault&&!prepared.canCommit()){report.issue="FAULT_FIXTURE_PREPARATION_FAILED: "+(prepared.issues.empty()?std::string("unknown"):prepared.issues.front().message);return report;}
    TestHooks hooks;HookScope scope(hooks);hooks.fault=static_cast<int>(point);hooks.exporting=exportFault;
    if(exportFault){auto output=prepared.value->cloneIntoNewOwnedDb();report.noResult=!output.value&&!output.canCommit();report.issue=output.issues.empty()?"FAULT_NOT_REACHED":output.issues.front().message;need(prepared.value->intact(),"FAULT_MUTATED_PREPARED_SOURCE");}
    else{auto output=prepareMaterialTiles(plan,seeds,options);report.noResult=!output.value&&!output.canCommit();report.issue=output.issues.empty()?"FAULT_NOT_REACHED":output.issues.front().message;}
    finishProbe(report,hooks,working);return report;
}
OwnedProbe typedXrecordValues(const PreparedMaterialTiles& batch){
    OwnedProbe report;auto* working=acdbHostApplicationServices()->workingDatabase();TestHooks hooks;HookScope scope(hooks);
    try{
        auto original=batch.cloneIntoNewOwnedDb();need(original.canCommit(),"TYPED_FIXTURE_SOURCE_COPY_FAILED");auto source=std::move(*original.value);auto target=newOwnedDatabase();Working sourceWork(*source.database);AcDbObjectId sourceRecord,dictionaryId;
        {Open<AcDbHatch> root(source.roots[0]);Open<AcDbLayerTableRecord> layer(root->layerId());dictionaryId=layer->extensionDictionary();}
        {Open<AcDbDictionary> dictionary(dictionaryId,AcDb::kForWrite);auto record=std::make_unique<AcDbXrecord>();check(dictionary->setAt(L"BCE_M2_TYPED_NONREFERENCE",record.get(),sourceRecord),"typed fixture record");record.release()->close();}
        {Open<AcDbRegAppTable> apps(source.database->regAppTableId(),AcDb::kForWrite);auto app=std::make_unique<AcDbRegAppTableRecord>();check(app->setName(L"BCE_M2_DXF_BASE"),"typed XData RegApp name");AcDbObjectId appId;check(apps->add(appId,app.get()),"typed XData RegApp create");app.release()->close();}
        const char* names[]={"Real","Int32","Int16","Int8","Text","Binary","Point3","Int64","BaseXData"};
        auto assign=[&](AcDbObjectId id,int mutation){
            resbuf items[8]{};for(int i=0;i<7;++i)items[i].rbnext=&items[i+1];
            items[0].restype=40;items[0].resval.rreal=1.25+(mutation==0?.5:0);
            items[1].restype=90;items[1].resval.rlong=-1234567+(mutation==1?1:0);
            items[2].restype=70;items[2].resval.rint=static_cast<short>(-123+(mutation==2?1:0));
            items[3].restype=280;items[3].resval.rint=static_cast<short>(201+(mutation==3?1:0));
            ACHAR text[]=L"M2 非引用原值",changed[]=L"M2 非引用变值";items[4].restype=1;items[4].resval.rstring=mutation==4?changed:text;
            unsigned char bytes[]={0,127,128,0,255,17};if(mutation==5)bytes[3]=5;items[5].restype=310;items[5].resval.rbinary.clen=static_cast<short>(sizeof(bytes));items[5].resval.rbinary.buf=reinterpret_cast<char*>(bytes);
            items[6].restype=10;items[6].resval.rpoint[0]=-.125;items[6].resval.rpoint[1]=2.5;items[6].resval.rpoint[2]=1000.001+(mutation==6?.25:0);
            items[7].restype=160;items[7].resval.mnInt64=-0x123456789ABLL+(mutation==7?1:0);
            const AcDb::DwgDataType types[]={AcDb::kDwgReal,AcDb::kDwgInt32,AcDb::kDwgInt16,AcDb::kDwgInt8,AcDb::kDwgText,AcDb::kDwgBChunk,AcDb::kDwg3Real,AcDb::kDwgInt64};for(int i=0;i<8;++i)need(acdbGroupCodeToType(static_cast<AcDb::DxfCode>(items[i].restype))==types[i],"TYPED_FIXTURE_OFFICIAL_CLASSIFICATION");
            Open<AcDbXrecord> record(id,AcDb::kForWrite);check(record->setFromRbChain(items[0]),"set complete typed fixture");
            resbuf xdata[3]{};xdata[0].restype=1001;ACHAR appName[]=L"BCE_M2_DXF_BASE",baseText[]=L"基础扩展数据";xdata[0].resval.rstring=appName;xdata[0].rbnext=&xdata[1];xdata[1].restype=1000;xdata[1].resval.rstring=baseText;xdata[1].rbnext=&xdata[2];xdata[2].restype=1040;xdata[2].resval.rreal=3.125+(mutation==8?.5:0);check(record->setXData(xdata),"set complete base XData fixture");
        };
        assign(sourceRecord,-1);AcDbObjectIdArray one;one.append(source.roots[0]);const auto cloned=cloneVerified(*source.database,one,*target);need(cloned.length()==1,"TYPED_FIXTURE_CLONE_ROOT");const auto targetRecord=targetResource(sourceRecord,*target);need(!targetRecord.isNull(),"TYPED_FIXTURE_RECORD_MAPPING");equalResource(sourceRecord,targetRecord,"TYPED_FIXTURE_NORMAL_COPY_CHANGED");report.typedChecks.emplace_back("NormalCrossDatabaseClone",true);bool all=true;
        for(int mutation=0;mutation<9;++mutation){assign(targetRecord,mutation);const auto before=resourceCensus(*target);bool rejected=false;std::string reason;
            try{cloneVerified(*source.database,one,*target);reason="MUTATION_NOT_REJECTED";}catch(const std::exception& e){reason=e.what();rejected=reason.find("RESOURCE_CONFLICT")!=std::string::npos&&resourceCensus(*target)==before;}
            report.typedChecks.emplace_back(names[mutation],rejected);report.issue+=std::string(names[mutation])+":"+reason+";";all=all&&rejected;if(!rejected)break;assign(targetRecord,-1);equalResource(sourceRecord,targetRecord,"TYPED_FIXTURE_RESTORE_CHANGED");
        }
        report.noResult=all&&report.typedChecks.size()==10;report.preflightUnchanged=report.noResult;need(batch.intact(),"TYPED_FIXTURE_MUTATED_IMMUTABLE_BATCH");
    }catch(const std::exception& e){report.noResult=false;report.issue=std::string("TYPED_FIXTURE_FAILED: ")+e.what();}
    finishProbe(report,hooks,working);return report;
}
OwnedProbe resourceConflict(const PreparedMaterialTiles& batch,ResourceConflict mutation){
    if(mutation==ResourceConflict::ReferenceAlias)return referenceAlias(batch);
    OwnedProbe report;auto* working=acdbHostApplicationServices()->workingDatabase();TestHooks hooks;HookScope scope(hooks);
    try{
        auto initial=batch.cloneIntoNewOwnedDb();need(initial.canCommit(),"CONFLICT_FIXTURE_INITIAL_EXPORT_FAILED");auto owned=std::move(*initial.value);auto& target=*owned.database;AcDbObjectId layerId,layerDictionary,materialDictionary;
        {Open<AcDbHatch> root(owned.roots[0]);layerId=root->layerId();Open<AcDbLayerTableRecord> layer(layerId);layerDictionary=layer->extensionDictionary();Open<AcDbMaterial> material(layer->materialId());materialDictionary=material->extensionDictionary();}
        for(int i=0;i<owned.roots.length();++i){Open<AcDbEntity> root(owned.roots[i],AcDb::kForWrite);check(root->erase(),"erase private conflict fixture roots");}
        if(mutation==ResourceConflict::LayerColor){Open<AcDbLayerTableRecord> layer(layerId,AcDb::kForWrite);AcCmColor color;color.setColorIndex(layer->color().colorIndex()==1?2:1);layer->setColor(color);}
        else if(mutation==ResourceConflict::DictionaryKey)extraKey(layerDictionary);
        else if(mutation==ResourceConflict::ReusedOwnedKey)extraKey(materialDictionary);
        else{
            const bool reused=mutation==ResourceConflict::ReusedOwnedPayload||mutation==ResourceConflict::ReusedOwnedOwner;const auto record=firstXrecord(reused?materialDictionary:layerDictionary);Open<AcDbXrecord> object(record,AcDb::kForWrite);
            if(mutation==ResourceConflict::XrecordPolicy)object->setXlateReferences(!object->isXlateReferences());
            else if(mutation==ResourceConflict::ReusedOwnedOwner)check(object->setOwnerId(layerDictionary),"private conflicting owner fixture");
            else{resbuf item{};item.restype=90;item.resval.rlong=1729;check(object->appendRbChain(item),"opaque payload conflict fixture");}
        }
        const auto before=resourceCensus(target);
        try{cloneVerified(batch.database(),batch.roots(),target);report.issue="CONFLICT_NOT_REJECTED";}
        catch(const std::exception& e){report.noResult=true;report.issue=e.what();report.preflightUnchanged=resourceCensus(target)==before;}
        need(batch.intact(),"CONFLICT_MUTATED_PREPARED_SOURCE");
    }catch(const std::exception& e){report.noResult=false;report.issue=std::string("CONFLICT_FIXTURE_FAILED: ")+e.what();}
    finishProbe(report,hooks,working);return report;
}
}
#endif
}
