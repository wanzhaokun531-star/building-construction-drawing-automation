#pragma once
#include "NativeMaterialTiles.h"
#include "NativeEvidence.h"
#include <acdocman.h>
#include <aced.h>
#include <acutmem.h>
#include <adscodes.h>
#include <dbHatch.h>
#include <dbapserv.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <dbdict.h>
#include <dbplhldr.h>
#include <dbxrecrd.h>
#include <dbfiler.h>
#include <dbmaterial.h>
#include <map>
#include <functional>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>
namespace bce::tile_test {
inline void ok(Acad::ErrorStatus s,const char* m){native::require(s==Acad::eOk,std::string(m)+":"+std::to_string(s));}
inline void advanceXrecord(AcDbXrecordIterator& iterator,const char* context){const auto status=iterator.next();native::require(status==Acad::eOk||(status==Acad::eIteratorDone&&iterator.done()),std::string(context)+":"+std::to_string(status));}
inline std::size_t countXrecord(AcDbXrecord& record){std::size_t count=0;AcDbXrecordIterator iterator(&record);for(iterator.start();!iterator.done();advanceXrecord(iterator,"Xrecord count")){native::require(count<4096,"Xrecord count budget");++count;}return count;}
inline std::size_t independentXrecordCount(std::size_t elements){AcDbXrecord record;for(std::size_t i=0;i<elements;++i){resbuf item{};item.restype=90;item.resval.rlong=static_cast<Adesk::Int32>(i+1);ok(record.appendRbChain(item),"independent Xrecord element");}return countXrecord(record);}
template<class T> class Open final {public:explicit Open(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead){ok(acdbOpenObject(p,id,mode),"test open");}~Open(){if(p)p->close();}Open(const Open&)=delete;Open& operator=(const Open&)=delete;T* operator->()const{return p;}T* p{};};
class Working final {public:explicit Working(AcDbDatabase& db):old(acdbHostApplicationServices()->workingDatabase()){acdbHostApplicationServices()->setWorkingDatabase(&db);}~Working(){acdbHostApplicationServices()->setWorkingDatabase(old);}Working(const Working&)=delete;Working& operator=(const Working&)=delete;AcDbDatabase* old;};
struct Monitor final:AcDbDatabaseReactor {explicit Monitor(AcDbDatabase& d):db(d){ok(db.addReactor(this),"monitor add");}~Monitor(){db.removeReactor(this);}void objectOpenedForModify(const AcDbDatabase*,const AcDbObject*)override{++changes;}void objectModified(const AcDbDatabase*,const AcDbObject*)override{++changes;}void objectAppended(const AcDbDatabase*,const AcDbObject*)override{++changes;}void objectErased(const AcDbDatabase*,const AcDbObject*,Adesk::Boolean)override{++changes;}void headerSysVarWillChange(const AcDbDatabase*,const ACHAR*)override{++changes;}AcDbDatabase& db;unsigned changes{};};
inline bool nearValue(double a,double b){return std::isfinite(a)&&std::isfinite(b)&&std::abs(a-b)<=1e-8*(std::max)(1.0,std::abs(b));}
inline std::string number(double v){std::ostringstream s;s<<std::setprecision(17)<<v;return s.str();}
inline AcDbObjectIdArray roots(AcDbDatabase& db){Open<AcDbBlockTableRecord> b(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};ok(b->newIterator(raw),"iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);AcDbObjectIdArray a;for(;!it->done();it->step()){AcDbObjectId id;ok(it->getEntityId(id),"root");a.append(id);}return a;}
inline std::string globals(){std::string out;for(auto name:{L"INSUNITS",L"LTSCALE",L"CELTSCALE",L"DIMSTYLE",L"TEXTSTYLE",L"FRAME",L"HPMAXLINES",L"FILLMODE"}){resbuf r{};native::require(acedGetVar(name,&r)==RTNORM,"globals");if(r.restype==RTSHORT)out+=std::to_string(r.resval.rint);else if(r.restype==RTLONG)out+=std::to_string(r.resval.rlong);else if(r.restype==RTREAL)out+=number(r.resval.rreal);else if(r.restype==RTSTR){out+=native::utf8(r.resval.rstring);acutDelString(r.resval.rstring);}out+=';';}return out;}
inline AcDbObjectId appendText(AcDbDatabase& db){Working work(db);Open<AcDbBlockTableRecord> b(acdbSymUtil()->blockModelSpaceId(&db),AcDb::kForWrite);auto t=std::make_unique<AcDbText>();t->setDatabaseDefaults(&db);ok(t->setTextString(L"M2 未保存中文保持"),"text");ok(t->setPosition({12.25,77.125,0}),"position");ok(t->setHeight(250),"height");AcDbObjectId id;ok(b->appendAcDbEntity(id,t.get()),"append");t.release()->close();return id;}
inline std::string handle(AcDbObjectId id){if(id.isNull())return "null";ACHAR text[32]{};id.handle().getIntoAsciiBuffer(text);return native::utf8(text);}
// Test-only metadata inventory. Unknown leaf classes are named, never serialized,
// decoded, or authorized. Only exact standard dictionaries are traversed.
inline std::string dictionaryInventory(AcDbObjectId layerId,const std::string& label){
    std::vector<std::string> nodes;std::vector<AcDbObjectId> active;std::size_t count{};
    std::function<void(AcDbObjectId,const std::string&,const std::string&,unsigned)> visit;
    visit=[&](AcDbObjectId id,const std::string& path,const std::string& role,unsigned depth){
        native::require(depth<=32&&++count<=512,"diagnostic dictionary budget");if(id.isNull())return;Open<AcDbObject> object(id);const bool cycle=std::find(active.begin(),active.end(),id)!=active.end();native::Fields row={{"path",native::quote(path)},{"edgeRole",native::quote(role)},{"depth",std::to_string(depth)},{"handle",native::quote(handle(id))},{"class",native::quote(std::wstring(object->isA()->name()))},{"ownerHandle",native::quote(handle(object->ownerId()))},{"extensionDictionaryHandle",native::quote(handle(object->extensionDictionary()))},{"cycle",cycle?"true":"false"},{"exactAcDbDictionary",object->isA()==AcDbDictionary::desc()?"true":"false"},{"exactAcDbXrecord",object->isA()==AcDbXrecord::desc()?"true":"false"},{"leafPayloadRead","false"}};
        if(!object->ownerId().isNull()){Open<AcDbObject> owner(object->ownerId());row.push_back({"ownerClass",native::quote(std::wstring(owner->isA()->name()))});}
        if(object->isA()==AcDbDictionary::desc()){auto* dictionary=AcDbDictionary::cast(object.p);row.push_back({"numEntries",std::to_string(dictionary->numEntries())});row.push_back({"treatElementsAsHard",dictionary->isTreatElementsAsHard()?"true":"false"});row.push_back({"mergeStyle",std::to_string(dictionary->mergeStyle())});}
        if(object->isA()==AcDbXrecord::desc()){auto* record=AcDbXrecord::cast(object.p);row.push_back({"xlateReferences",record->isXlateReferences()?"true":"false"});row.push_back({"mergeStyle",std::to_string(record->mergeStyle())});AcDbXrecordIterator iterator(record);std::vector<std::string> codes;for(iterator.start();!iterator.done();advanceXrecord(iterator,"public Xrecord iterator")){native::require(codes.size()<4096,"diagnostic Xrecord type budget");codes.push_back(std::to_string(iterator.curRestype()));}row.push_back({"publicRestypeCodes",native::array(codes)});row.push_back({"businessPayloadInterpreted","false"});}
        nodes.push_back(native::object(row));if(cycle)return;active.push_back(id);
        if(object->isA()==AcDbDictionary::desc()){auto* dictionary=AcDbDictionary::cast(object.p);std::unique_ptr<AcDbDictionaryIterator> it(dictionary->newIterator());native::require(it!=nullptr,"dictionary diagnostic iterator");for(;!it->done();it->next()){const auto key=native::utf8(it->name());visit(it->objectId(),path+"/key="+key,dictionary->isTreatElementsAsHard()?"dictionaryEntry/hardElements":"dictionaryEntry/softElements",depth+1);}}
        const auto extension=object->extensionDictionary();if(!extension.isNull())visit(extension,path+"/extensionDictionary","extensionDictionary/HardOwnership",depth+1);active.pop_back();
    };
    visit(layerId,label+"/layer","Hatch.layerId/HardPointer",0);return native::object({{"label",native::quote(label)},{"diagnosticOnly","true"},{"nodes",native::array(nodes)}});
}
struct Rect {double x0,y0,x1,y1;};
inline Rect bounds(const ClosedRing& r){Rect b{r.vertices[0].x,r.vertices[0].y,r.vertices[0].x,r.vertices[0].y};for(auto p:r.vertices){b.x0=(std::min)(b.x0,p.x);b.x1=(std::max)(b.x1,p.x);b.y0=(std::min)(b.y0,p.y);b.y1=(std::max)(b.y1,p.y);}return b;}
inline bool intersects(AcGePoint2d a,AcGePoint2d b,Rect r,double e){double lo=0,hi=1;double v[2]={a.x,a.y},d[2]={b.x-a.x,b.y-a.y},mn[2]={r.x0+e,r.y0+e},mx[2]={r.x1-e,r.y1-e};for(int k=0;k<2;++k){if(std::abs(d[k])<1e-15){if(v[k]<=mn[k]||v[k]>=mx[k])return false;}else{double t1=(mn[k]-v[k])/d[k],t2=(mx[k]-v[k])/d[k];if(t1>t2)std::swap(t1,t2);lo=(std::max)(lo,t1);hi=(std::min)(hi,t2);if(lo>=hi)return false;}}return hi>lo;}
inline std::string pattern(AcDbHatch& h){std::ostringstream s;s<<std::hexfloat<<native::utf8(h.patternName())<<';'<<h.patternType()<<';'<<h.patternScale()<<';'<<h.patternAngle()<<';'<<h.patternSpace()<<';'<<h.patternDouble()<<';'<<h.originPoint().x<<';'<<h.originPoint().y<<';'<<h.color().color()<<';'<<h.numPatternDefinitions()<<';';for(int i=0;i<h.numPatternDefinitions();++i){double a,x,y,dx,dy;AcGeDoubleArray d;ok(h.getPatternDefinitionAt(i,a,x,y,dx,dy,d),"67 definitions");s<<a<<';'<<x<<';'<<y<<';'<<dx<<';'<<dy<<';';for(int j=0;j<d.length();++j)s<<d[j]<<';';}return s.str();}
// Save/reopen compares persistent identities in the same DWG, not independent
// clone databases. Keep actual handles and all public DXF values; no ID or
// binary normalization is appropriate at this boundary.
using SavedFields=std::map<std::string,std::string>;
struct SavedMaterialFields {std::vector<std::string> rootHandles;SavedFields values;double area{};std::size_t lines{};};
inline bool savedResourceClass(AcRxClass* c){return c==AcDbHatch::desc()||c==AcDbLayerTableRecord::desc()||c==AcDbLinetypeTableRecord::desc()||c==AcDbMaterial::desc()||c==AcDbPlaceHolder::desc()||c==AcDbRegAppTableRecord::desc()||c==AcDbDictionary::desc()||c==AcDbXrecord::desc();}
class SavedDxfFiler final:public AcDbDxfFiler {
public:
    SavedDxfFiler(AcDbObject& object,SavedFields& fields,std::vector<AcDbObjectId>& queue):object_(object),fields_(fields),queue_(queue),prefix_("object["+handle(object.objectId())+"].dxf."){}
    int rewindFiler()override{throw std::runtime_error("roundtrip DXF rewind unsupported");}
    Acad::ErrorStatus filerStatus()const override{return Acad::eOk;}void resetFilerStatus()override{}
    Acad::ErrorStatus setError(Acad::ErrorStatus status,const ACHAR*,...)override{throw std::runtime_error("roundtrip DXF error:"+std::to_string(status));}
    Acad::ErrorStatus setError(const ACHAR*,...)override{throw std::runtime_error("roundtrip DXF error");}
    AcDb::FilerType filerType()const override{return AcDb::kFileFiler;}
    AcDbDatabase* database()const override{return object_.database();}
    Acad::ErrorStatus dwgVersion(AcDb::AcDbDwgVersion& v,AcDb::MaintenanceReleaseVersion& m)const override{v=AcDb::kDHL_CURRENT;m=AcDb::kMReleaseCurrent;return Acad::eOk;}
    bool includesDefaultValues()const override{return true;}int precision()const override{return kMaxPrec;}
    Acad::ErrorStatus writeObjectId(AcDb::DxfCode code,const AcDbObjectId& id)override{reference(id);return field(code,"ObjectId",handle(id));}
    Acad::ErrorStatus writeAcDbHandle(AcDb::DxfCode code,const AcDbHandle& value)override{
        ACHAR text[32]{};value.getIntoAsciiBuffer(text);const auto type=acdbGroupCodeToType(code);if(isReference(type)&&!value.isNull()){AcDbObjectId id;ok(database()->getAcDbObjectId(id,false,value),"roundtrip DXF handle reference");reference(id);}return field(code,"Handle",native::utf8(text));
    }
    Acad::ErrorStatus writeString(AcDb::DxfCode code,const AcString& value)override{return writeString(code,value.kACharPtr());}
    Acad::ErrorStatus writeString(AcDb::DxfCode code,const ACHAR* value)override{
        const auto type=acdbGroupCodeToType(code);if(isReference(type)){native::require(value&&*value,"roundtrip DXF reference text");const AcDbHandle h(value);AcDbObjectId id;if(!h.isNull())ok(database()->getAcDbObjectId(id,false,h),"roundtrip DXF text reference");reference(id);}
        if(static_cast<int>(code)==1001){native::require(value&&*value,"roundtrip XData app");Open<AcDbRegAppTable> apps(database()->regAppTableId());AcDbObjectId id;ok(apps->getAt(value,id),"roundtrip XData RegApp");reference(id);}
        return field(code,"String",value?"text:"+native::utf8(value):"null");
    }
    Acad::ErrorStatus writeBChunk(AcDb::DxfCode code,const ads_binary& value)override{native::require(value.clen>=0&&value.clen<=32767&&(value.buf||value.clen==0),"roundtrip binary bounds");std::ostringstream bytes;bytes<<value.clen<<':';for(int i=0;i<value.clen;++i)bytes<<static_cast<unsigned>(static_cast<unsigned char>(value.buf[i]))<<',';return field(code,"Binary",bytes.str());}
#define BCE_SAVED_SCALAR(Name,Type) Acad::ErrorStatus write##Name(AcDb::DxfCode code,Type value)override{std::ostringstream out;out<<+value;return field(code,#Name,out.str());}
    BCE_SAVED_SCALAR(Int8,Adesk::Int8)
    BCE_SAVED_SCALAR(Int16,Adesk::Int16)
    BCE_SAVED_SCALAR(Int32,Adesk::Int32)
    BCE_SAVED_SCALAR(Int64,Adesk::Int64)
    BCE_SAVED_SCALAR(UInt8,Adesk::UInt8)
    BCE_SAVED_SCALAR(UInt16,Adesk::UInt16)
    BCE_SAVED_SCALAR(UInt32,Adesk::UInt32)
    BCE_SAVED_SCALAR(UInt64,Adesk::UInt64)
    BCE_SAVED_SCALAR(Boolean,Adesk::Boolean)
    BCE_SAVED_SCALAR(Bool,bool)
#undef BCE_SAVED_SCALAR
    Acad::ErrorStatus writeDouble(AcDb::DxfCode code,double value,int=kDfltPrec)override{native::require(std::isfinite(value),"roundtrip finite DXF real");return field(code,"Double",number(value));}
#define BCE_SAVED_VECTOR(Name,Type,Values) Acad::ErrorStatus write##Name(AcDb::DxfCode code,const Type& value,int=kDfltPrec)override{return field(code,#Name,Values);}
    BCE_SAVED_VECTOR(Point2d,AcGePoint2d,number(value.x)+","+number(value.y))
    BCE_SAVED_VECTOR(Point3d,AcGePoint3d,number(value.x)+","+number(value.y)+","+number(value.z))
    BCE_SAVED_VECTOR(Vector2d,AcGeVector2d,number(value.x)+","+number(value.y))
    BCE_SAVED_VECTOR(Vector3d,AcGeVector3d,number(value.x)+","+number(value.y)+","+number(value.z))
    BCE_SAVED_VECTOR(Scale3d,AcGeScale3d,number(value.sx)+","+number(value.sy)+","+number(value.sz))
#undef BCE_SAVED_VECTOR
    Acad::ErrorStatus writeXDataStart()override{return field(static_cast<AcDb::DxfCode>(-999),"XDataStart","");}
    Acad::ErrorStatus writeEmbeddedObjectStart()override{return field(static_cast<AcDb::DxfCode>(-998),"EmbeddedObjectStart","");}
    Acad::ErrorStatus writeResBuf(const resbuf& item)override{const auto before=count_;const auto status=AcDbDxfFiler::writeResBuf(item);ok(status,"public roundtrip resbuf dispatch");native::require(count_>before,"public roundtrip resbuf not recorded");return status;}
private:
    static bool isReference(AcDb::DwgDataType t){return t==AcDb::kDwgHardOwnershipId||t==AcDb::kDwgSoftOwnershipId||t==AcDb::kDwgHardPointerId||t==AcDb::kDwgSoftPointerId;}
    void reference(AcDbObjectId id){if(id.isNull())return;native::require(id.database()==database(),"roundtrip reference database");if(id!=object_.objectId()&&id!=object_.ownerId()&&std::find(queue_.begin(),queue_.end(),id)==queue_.end()){native::require(queue_.size()<1024,"roundtrip dependency budget");queue_.push_back(id);}}
    Acad::ErrorStatus field(AcDb::DxfCode code,const char* type,const std::string& value){native::require(++count_<=16384&&value.size()<=1048576,"roundtrip field budget");fields_.emplace(prefix_+std::to_string(count_)+".group["+std::to_string(code)+"]."+type,value);return Acad::eOk;}
    AcDbObject& object_;SavedFields& fields_;std::vector<AcDbObjectId>& queue_;std::string prefix_;std::size_t count_{};
};
inline SavedMaterialFields savedMaterialFields(AcDbDatabase& db,const AcDbObjectIdArray& ids){
    SavedMaterialFields result;Working work(db);auto actual=roots(db);native::require(actual.length()==ids.length(),"roundtrip exact root set count");std::vector<AcDbObjectId> queue;
    for(int i=0;i<ids.length();++i){native::require(actual.contains(ids[i])&&std::find(queue.begin(),queue.end(),ids[i])==queue.end(),"roundtrip root membership and uniqueness");result.rootHandles.push_back(handle(ids[i]));queue.push_back(ids[i]);Open<AcDbHatch> h(ids[i]);const auto key="root["+handle(ids[i])+"]";auto& f=result.values;
        f[key+".color"]=std::to_string(h->color().color());f[key+".layerHandle"]=handle(h->layerId());f[key+".patternName"]=native::utf8(h->patternName());f[key+".patternType"]=std::to_string(h->patternType());f[key+".scale"]=number(h->patternScale());f[key+".angle"]=number(h->patternAngle());f[key+".space"]=number(h->patternSpace());f[key+".double"]=std::to_string(h->patternDouble());f[key+".originX"]=number(h->originPoint().x);f[key+".originY"]=number(h->originPoint().y);f[key+".definitionCount"]=std::to_string(h->numPatternDefinitions());
        for(int d=0;d<h->numPatternDefinitions();++d){double a,x,y,dx,dy;AcGeDoubleArray dash;ok(h->getPatternDefinitionAt(d,a,x,y,dx,dy,dash),"roundtrip complete definitions");const auto p=key+".definition["+std::to_string(d)+"]";f[p+".angle"]=number(a);f[p+".baseX"]=number(x);f[p+".baseY"]=number(y);f[p+".offsetX"]=number(dx);f[p+".offsetY"]=number(dy);f[p+".dashCount"]=std::to_string(dash.length());for(int k=0;k<dash.length();++k)f[p+".dash["+std::to_string(k)+"]"]=number(dash[k]);}
    }
    result.values["database.insunits"]=std::to_string(db.insunits());result.values["database.ltscale"]=number(db.ltscale());
    for(std::size_t i=0;i<queue.size();++i){Open<AcDbObject> object(queue[i]);native::require(savedResourceClass(object->isA()),"roundtrip unsupported public resource:"+native::utf8(object->isA()->name()));const auto key="object["+handle(queue[i])+"]";result.values[key+".class"]=native::utf8(object->isA()->name());result.values[key+".owner"]=handle(object->ownerId());result.values[key+".extensionDictionary"]=handle(object->extensionDictionary());if(!object->extensionDictionary().isNull()&&std::find(queue.begin(),queue.end(),object->extensionDictionary())==queue.end())queue.push_back(object->extensionDictionary());
        if(object->isA()==AcDbXrecord::desc()){auto* record=AcDbXrecord::cast(object.p);result.values[key+".xlateReferences"]=std::to_string(record->isXlateReferences());result.values[key+".mergeStyle"]=std::to_string(record->mergeStyle());}
        SavedDxfFiler filer(*object.p,result.values,queue);ok(object->dxfOut(&filer,true,nullptr),"roundtrip complete public DXF");native::require(result.values.size()<262144&&queue.size()<=1024,"roundtrip complete snapshot budget");
    }
    return result;
}
inline std::string savedFieldsJson(const SavedMaterialFields& state){std::vector<std::string> fields,ids;for(const auto& id:state.rootHandles)ids.push_back(native::quote(id));for(const auto& field:state.values)fields.push_back(native::object({{"field",native::quote(field.first)},{"value",native::quote(field.second)}}));return native::object({{"roots",native::array(ids)},{"area",number(state.area)},{"lines",std::to_string(state.lines)},{"fields",native::array(fields)}});}
inline std::vector<std::string> savedFieldDifferences(const SavedMaterialFields& before,const SavedMaterialFields& after){std::vector<std::string> differences;if(before.rootHandles!=after.rootHandles)differences.push_back(native::object({{"field",native::quote(std::string("rootHandles"))},{"reason",native::quote(std::string("identity/order mismatch"))}}));auto a=before.values.begin(),b=after.values.begin();while(a!=before.values.end()||b!=after.values.end()){if(b==after.values.end()||(a!=before.values.end()&&a->first<b->first)){differences.push_back(native::object({{"field",native::quote(a->first)},{"before",native::quote(a->second)},{"afterMissing","true"}}));++a;}else if(a==before.values.end()||b->first<a->first){differences.push_back(native::object({{"field",native::quote(b->first)},{"beforeMissing","true"},{"after",native::quote(b->second)}}));++b;}else{if(a->second!=b->second)differences.push_back(native::object({{"field",native::quote(a->first)},{"before",native::quote(a->second)},{"after",native::quote(b->second)}}));++a;++b;}}return differences;}
inline bool acceptSavedMaterialFields(const SavedMaterialFields& before,const SavedMaterialFields& after){
    // The saved baseline, including every persistent root and referenced public
    // resource field, is the authority across the save/reopen boundary.
    return before.rootHandles==after.rootHandles&&before.values==after.values&&nearValue(before.area,after.area)&&before.lines==after.lines;
}
inline bool recolorAllSavedRoots(AcDbDatabase& db,const AcDbObjectIdArray& ids){Working work(db);native::require(ids.length()>0,"negative control roots");Adesk::UInt16 color;{Open<AcDbHatch> first(ids[0]);color=first->colorIndex()==1?2:1;}bool changed=true;for(int i=0;i<ids.length();++i){Open<AcDbHatch> h(ids[i],AcDb::kForWrite);const auto before=h->color().color();ok(h->setColorIndex(color),"uniform color negative control");changed=changed&&h->color().color()!=before&&h->colorIndex()==color;}return changed;}
struct DefinitionComparison {bool passed{};std::string firstFailure,json;};
inline DefinitionComparison mappedDefinitions(AcDbHatch& source,AcDbHatch& target,double factor,double angle,Point2 anchor,Point2 origin){
    using namespace native;DefinitionComparison result;result.passed=source.numPatternDefinitions()==target.numPatternDefinitions();if(!result.passed)result.firstFailure="definitionCount";const double c=std::cos(angle),s=std::sin(angle);std::vector<std::string> rows;
    auto values=[](double a,double x,double y,double dx,double dy,const AcGeDoubleArray& d){std::vector<std::string> dash;for(int j=0;j<d.length();++j)dash.push_back(number(d[j]));return object({{"angle",number(a)},{"baseX",number(x)},{"baseY",number(y)},{"offsetX",number(dx)},{"offsetY",number(dy)},{"dashes",array(dash)}});};
    const int count=(std::min)(source.numPatternDefinitions(),target.numPatternDefinitions());
    for(int i=0;i<count;++i){double a,x,y,dx,dy,ta,tx,ty,tdx,tdy;AcGeDoubleArray d,td,expectedDashes;ok(source.getPatternDefinitionAt(i,a,x,y,dx,dy,d),"source pattern definition");ok(target.getPatternDefinitionAt(i,ta,tx,ty,tdx,tdy,td),"target pattern definition");
        const double ea=a+angle,ex=origin.x+factor*(c*(x-anchor.x)-s*(y-anchor.y)),ey=origin.y+factor*(s*(x-anchor.x)+c*(y-anchor.y)),edx=factor*(c*dx-s*dy),edy=factor*(s*dx+c*dy);for(int j=0;j<d.length();++j)expectedDashes.append(d[j]*factor);std::vector<std::string> checks;bool rowPassed=true;
        auto field=[&](const std::string& name,double actual,double expected){const bool pass=nearValue(actual,expected);checks.push_back(object({{"field",quote(name)},{"actual",number(actual)},{"expected",number(expected)},{"difference",number(actual-expected)},{"allowedAbsoluteDifference",number(1e-8*(std::max)(1.0,std::abs(expected)))},{"passed",pass?"true":"false"}}));if(!pass){rowPassed=false;if(result.firstFailure.empty())result.firstFailure="definition["+std::to_string(i)+"]."+name;}};
        // Offset is a coordinate vector: apply scale and rotation, no translation. Raw angle and every
        // definition value are recorded separately, without normalization.
        field("sinAngle",std::sin(ta),std::sin(ea));field("cosAngle",std::cos(ta),std::cos(ea));field("baseX",tx,ex);field("baseY",ty,ey);field("offsetX",tdx,edx);field("offsetY",tdy,edy);
        const bool sameDashes=d.length()==td.length();checks.push_back(object({{"field",quote(std::string("dashCount"))},{"actual",std::to_string(td.length())},{"expected",std::to_string(d.length())},{"passed",sameDashes?"true":"false"}}));if(!sameDashes){rowPassed=false;if(result.firstFailure.empty())result.firstFailure="definition["+std::to_string(i)+"].dashCount";}for(int j=0;j<(std::min)(d.length(),td.length());++j)field("dash["+std::to_string(j)+"]",td[j],expectedDashes[j]);
        rows.push_back(object({{"index",std::to_string(i)},{"source",values(a,x,y,dx,dy,d)},{"target",values(ta,tx,ty,tdx,tdy,td)},{"expectedUnderVectorTransform",values(ea,ex,ey,edx,edy,expectedDashes)},{"checks",array(checks)},{"passed",rowPassed?"true":"false"}}));result.passed=result.passed&&rowPassed;
    }
    result.json=object({{"sourceCount",std::to_string(source.numPatternDefinitions())},{"targetCount",std::to_string(target.numPatternDefinitions())},{"factor",number(factor)},{"rotationRadians",number(angle)},{"sourceAnchor",array({number(anchor.x),number(anchor.y)})},{"targetAnchor",array({number(origin.x),number(origin.y)})},{"offsetVectorTransform","true"},{"scalarTolerancesChanged","false"},{"passed",result.passed?"true":"false"},{"firstFailure",quote(result.firstFailure)},{"definitions",array(rows)}});return result;
}
struct Lines {Rect box;std::vector<std::pair<AcGePoint2d,AcGePoint2d>> lines;};
inline std::vector<double> contacts(const Lines& l,bool vertical,double at,double lo,double hi,double eps){std::vector<double> result;for(auto segment:l.lines){auto a=segment.first,b=segment.second;double aa=vertical?a.x:a.y,bb=vertical?b.x:b.y;if(std::abs(aa-bb)<=eps)continue;for(auto p:{a,b})if(std::abs((vertical?p.x:p.y)-at)<=eps){double c=vertical?p.y:p.x;if(c>lo+eps&&c<hi-eps)result.push_back(c);}}std::sort(result.begin(),result.end());return result;}
struct Checked {double area{};std::size_t lines{},seams{},contacts{};std::vector<std::string> rows;};
inline Checked verify(AcDbDatabase& db,const AcDbObjectIdArray& ids,const std::vector<MaterialTileRecord>& tiles,const MaterialPlan& plan,const MaterialPreparationOptions& options,bool evaluate){
    native::require(plan.regions.size()==1&&ids.length()==static_cast<int>(tiles.size())&&roots(db).length()==ids.length(),"whole exact root list");Working work(db);Checked checked;std::vector<Lines> all;std::string physical;AcDbObjectId commonLayer;const auto outer=bounds(plan.regions[0].geometry.outer);const double eps=1e-5/plan.mmPerInputUnit;
    for(int i=0;i<ids.length();++i){if(evaluate){Open<AcDbHatch> h(ids[i],AcDb::kForWrite);h->setLineGenerationEnabled(true);ok(h->evaluateHatch(),"reopen native evaluate");}
        Open<AcDbHatch> h(ids[i]);native::require(h->isA()==AcDbHatch::desc()&&h->numLoops()==1&&!h->associative(),"actual tile Hatch with one detached loop");double area{};ok(h->getArea(area),"native tile area");const auto r=bounds(tiles[i].geometry.outer);native::require(nearValue(area,(r.x1-r.x0)*(r.y1-r.y0))&&nearValue(area,tiles[i].nativeArea),"actual tile area");checked.area+=area;
        native::require(tiles[i].parentIdentity==plan.regions[0].identity&&tiles[i].viewIdentity==plan.regions[0].viewIdentity,"parent identity");native::require(r.x0>=outer.x0&&r.x1<=outer.x1&&r.y0>=outer.y0&&r.y1<=outer.y1,"no external tile");
        Adesk::Int32 type{};AcGePoint2dArray p;AcGeDoubleArray b;ok(h->getLoopAt(0,type,p,b),"actual tile loop");native::require((p.length()==4||p.length()==5)&&p[0]==AcGePoint2d(r.x0,r.y0)&&p[1]==AcGePoint2d(r.x1,r.y0)&&p[2]==AcGePoint2d(r.x1,r.y1)&&p[3]==AcGePoint2d(r.x0,r.y1),"exact unrounded loop");for(int k=0;k<b.length();++k)native::require(b[k]==0,"straight tile edge");
        const auto current=pattern(*h.p);if(i==0){physical=current;commonLayer=h->layerId();}native::require(current==physical&&h->layerId()==commonLayer,"all definitions phase color and one common layer");native::require(h->numPatternDefinitions()==67&&nearValue(h->originPoint().x,options.placements[0].targetAnchor.x)&&nearValue(h->originPoint().y,options.placements[0].targetAnchor.y),"expected global phase/67 definitions");
        AcGePoint2dArray starts,ends;ok(h->getHatchLinesData(starts,ends),"actual native line output");native::require(starts.length()>0&&starts.length()==ends.length()&&starts.length()==h->numHatchLines(),"no empty tile");Lines actual{r,{}};for(int k=0;k<starts.length();++k){for(auto point:{starts[k],ends[k]})native::require(point.x>=r.x0-eps&&point.x<=r.x1+eps&&point.y>=r.y0-eps&&point.y<=r.y1+eps,"actual line contained in tile");for(const auto& hole:plan.regions[0].geometry.holes)native::require(!intersects(starts[k],ends[k],bounds(hole),eps),"actual pattern does not enter opening");actual.lines.push_back({starts[k],ends[k]});}checked.lines+=actual.lines.size();all.push_back(std::move(actual));
        checked.rows.push_back(native::object({{"tile",std::to_string(i)},{"area",number(area)},{"lines",std::to_string(starts.length())},{"depth",std::to_string(tiles[i].depth)},{"box",native::array({number(r.x0),number(r.y0),number(r.x1),number(r.y1)})}}));
    }
    native::require(nearValue(checked.area*plan.mmPerInputUnit*plan.mmPerInputUnit,plan.regions[0].areaSquareMm),"whole native sum = parent area");
    for(std::size_t i=0;i<all.size();++i)for(std::size_t j=i+1;j<all.size();++j){auto a=all[i].box,b=all[j].box;double dx=(std::min)(a.x1,b.x1)-(std::max)(a.x0,b.x0),dy=(std::min)(a.y1,b.y1)-(std::max)(a.y0,b.y0);native::require(!(dx>0&&dy>0),"zero overlapping area");bool vertical=dy>eps&&(a.x1==b.x0||b.x1==a.x0),horizontal=dx>eps&&(a.y1==b.y0||b.y1==a.y0);if(!vertical&&!horizontal)continue;double at=vertical?(a.x1==b.x0?a.x1:b.x1):(a.y1==b.y0?a.y1:b.y1);double lo=vertical?(std::max)(a.y0,b.y0):(std::max)(a.x0,b.x0),hi=vertical?(std::min)(a.y1,b.y1):(std::min)(a.x1,b.x1);auto left=contacts(all[i],vertical,at,lo,hi,eps),right=contacts(all[j],vertical,at,lo,hi,eps);native::require(left.size()==right.size(),"actual seam contact count");for(std::size_t k=0;k<left.size();++k)native::require(std::abs(left[k]-right[k])<=eps,"actual seam phase continuity");++checked.seams;checked.contacts+=left.size();}
    native::require(checked.seams>0&&checked.contacts>0,"actual crossed seams observed");return checked;
}
}
