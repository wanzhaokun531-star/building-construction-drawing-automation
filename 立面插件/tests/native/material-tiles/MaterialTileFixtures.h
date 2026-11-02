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
#include <functional>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <algorithm>
namespace bce::tile_test {
inline void ok(Acad::ErrorStatus s,const char* m){native::require(s==Acad::eOk,std::string(m)+":"+std::to_string(s));}
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
        if(object->isA()==AcDbXrecord::desc()){auto* record=AcDbXrecord::cast(object.p);row.push_back({"xlateReferences",record->isXlateReferences()?"true":"false"});row.push_back({"mergeStyle",std::to_string(record->mergeStyle())});AcDbXrecordIterator iterator(record);std::vector<std::string> codes;for(iterator.start();!iterator.done();ok(iterator.next(),"public Xrecord iterator")){native::require(codes.size()<4096,"diagnostic Xrecord type budget");codes.push_back(std::to_string(iterator.curRestype()));}row.push_back({"publicRestypeCodes",native::array(codes)});row.push_back({"businessPayloadInterpreted","false"});}
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
inline bool mappedDefinitions(AcDbHatch& source,AcDbHatch& target,double factor,double angle,Point2 anchor,Point2 origin){
    if(source.numPatternDefinitions()!=target.numPatternDefinitions())return false;const double c=std::cos(angle),s=std::sin(angle);
    for(int i=0;i<source.numPatternDefinitions();++i){double a,x,y,dx,dy,ta,tx,ty,tdx,tdy;AcGeDoubleArray d,td;ok(source.getPatternDefinitionAt(i,a,x,y,dx,dy,d),"source pattern definition");ok(target.getPatternDefinitionAt(i,ta,tx,ty,tdx,tdy,td),"target pattern definition");
        if(!nearValue(std::sin(ta),std::sin(a+angle))||!nearValue(std::cos(ta),std::cos(a+angle))||!nearValue(tx,origin.x+factor*(c*(x-anchor.x)-s*(y-anchor.y)))||!nearValue(ty,origin.y+factor*(s*(x-anchor.x)+c*(y-anchor.y)))||!nearValue(tdx,dx*factor)||!nearValue(tdy,dy*factor)||d.length()!=td.length())return false;
        for(int j=0;j<d.length();++j)if(!nearValue(td[j],d[j]*factor))return false;
    }return true;
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
