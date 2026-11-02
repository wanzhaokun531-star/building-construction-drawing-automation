#pragma once
#include "NativeMaterials.h"
#include "NativeEvidence.h"
#include <acdocman.h>
#include <aced.h>
#include <acutmem.h>
#include <adscodes.h>
#include <dbHatch.h>
#include <dbapserv.h>
#include <dbents.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <cmath>
#include <functional>
#include <iomanip>
#include <sstream>
#include <limits>
namespace bce::material_test {
inline void ok(Acad::ErrorStatus s,const char* m){native::require(s==Acad::eOk,std::string(m)+":"+std::to_string(s));}
template<class T> class Open final {public:explicit Open(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead){ok(acdbOpenObject(p,id,mode),"test open");}~Open(){if(p)p->close();}Open(const Open&)=delete;Open& operator=(const Open&)=delete;T* operator->()const{return p;}T* p{};};
class Working final {public:explicit Working(AcDbDatabase& db):old(acdbHostApplicationServices()->workingDatabase()){acdbHostApplicationServices()->setWorkingDatabase(&db);}~Working(){acdbHostApplicationServices()->setWorkingDatabase(old);}Working(const Working&)=delete;Working& operator=(const Working&)=delete;AcDbDatabase* old;};
struct Monitor final:AcDbDatabaseReactor {explicit Monitor(AcDbDatabase& d):db(d){ok(db.addReactor(this),"monitor add");}~Monitor(){db.removeReactor(this);}void objectOpenedForModify(const AcDbDatabase*,const AcDbObject*)override{++changes;}void objectModified(const AcDbDatabase*,const AcDbObject*)override{++changes;}void objectAppended(const AcDbDatabase*,const AcDbObject*)override{++changes;}void objectErased(const AcDbDatabase*,const AcDbObject*,Adesk::Boolean)override{++changes;}void headerSysVarWillChange(const AcDbDatabase*,const ACHAR*)override{++changes;}AcDbDatabase& db;unsigned changes{};};
inline bool nearValue(double a,double b){return std::abs(a-b)<=1e-8*(std::max)(1.0,std::abs(b));}
inline std::string number(double v){std::ostringstream out;out<<std::setprecision(17)<<v;return out.str();}
inline std::string globals(){std::string out;for(const auto name:{L"INSUNITS",L"LTSCALE",L"CELTSCALE",L"DIMSTYLE",L"TEXTSTYLE",L"FRAME",L"WIPEOUTFRAME",L"IMAGEFRAME",L"XCLIPFRAME",L"FONTALT",L"MIRRTEXT"}){resbuf v{};native::require(acedGetVar(name,&v)==RTNORM,"read globals");if(v.restype==RTSHORT)out+=std::to_string(v.resval.rint);else if(v.restype==RTLONG)out+=std::to_string(v.resval.rlong);else if(v.restype==RTREAL)out+=number(v.resval.rreal);else if(v.restype==RTSTR){out+=native::utf8(v.resval.rstring);acutDelString(v.resval.rstring);}else throw std::runtime_error("unsupported global");out+=';';}return out;}
inline AcDbObjectIdArray modelRoots(AcDbDatabase& db){Open<AcDbBlockTableRecord> b(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};ok(b->newIterator(raw),"iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);AcDbObjectIdArray ids;for(;!it->done();it->step()){AcDbObjectId id;ok(it->getEntityId(id),"entity id");ids.append(id);}return ids;}
inline AcDbObjectId appendText(AcDbDatabase& db){Working work(db);Open<AcDbBlockTableRecord> b(acdbSymUtil()->blockModelSpaceId(&db),AcDb::kForWrite);auto text=std::make_unique<AcDbText>();text->setDatabaseDefaults(&db);ok(text->setTextString(L"M1 未保存中文材料源保持"),"current Chinese fixture");ok(text->setPosition({12.25,77.125,0}),"text position");ok(text->setHeight(250),"text height");AcDbObjectId id;ok(b->appendAcDbEntity(id,text.get()),"current append fixture");text.release()->close();return id;}
inline bool loops(AcDbHatch& h,double ox=0,double oy=0){if(h.isA()!=AcDbHatch::desc()||h.numLoops()!=2||h.associative())return false;AcDbObjectIdArray assoc;ok(h.getAssocObjIds(assoc),"assoc");if(!assoc.isEmpty())return false;
    const double coordinates[2][4][2]={{{0,0},{10000,0},{10000,6000},{0,6000}},{{1200,900},{2400,900},{2400,2400},{1200,2400}}};
    for(int i=0;i<2;++i){Adesk::Int32 type{};AcGePoint2dArray p;AcGeDoubleArray bulges;ok(h.getLoopAt(i,type,p,bulges),"loop read");if(!(type&AcDbHatch::kPolyline)||(i==0&&!(type&AcDbHatch::kExternal))||p.length()<4||p.length()>5)return false;for(int j=0;j<4;++j)if(!nearValue(p[j].x,coordinates[i][j][0]+ox)||!nearValue(p[j].y,coordinates[i][j][1]+oy))return false;if(p.length()==5&&p.first()!=p.last())return false;for(int j=0;j<bulges.length();++j)if(bulges[j]!=0)return false;}return true;}
// Independent line/rectangle clip: an evaluated pattern segment must not enter the open window.
inline bool crossesWindow(AcGePoint2d a,AcGePoint2d b){double lo=0,hi=1;const double av[2]={a.x,a.y},dv[2]={b.x-a.x,b.y-a.y},mn[2]={1200.00001,900.00001},mx[2]={2399.99999,2399.99999};for(int k=0;k<2;++k){if(std::abs(dv[k])<1e-14){if(av[k]<=mn[k]||av[k]>=mx[k])return false;}else{auto t1=(mn[k]-av[k])/dv[k],t2=(mx[k]-av[k])/dv[k];if(t1>t2)std::swap(t1,t2);lo=(std::max)(lo,t1);hi=(std::min)(hi,t2);if(lo>=hi)return false;}}return hi>lo;}
inline bool voidLines(AcDbHatch& h,int& count){count=h.numHatchLines();if(count<=0)return false;for(int i=0;i<count;++i){AcGePoint2d a,b;ok(h.getHatchLineDataAt(i,a,b),"evaluated pattern segment");if(crossesWindow(a,b))return false;}return true;}
inline std::wstring layer(AcDbHatch& h){Open<AcDbLayerTableRecord> l(h.layerId());AcString name;ok(l->getName(name),"layer name");return name.kACharPtr();}
inline std::wstring linetype(AcDbHatch& h){Open<AcDbLinetypeTableRecord> l(h.linetypeId());AcString name;ok(l->getName(name),"linetype name");return name.kACharPtr();}
inline double spacing(AcDbHatch& h){double a{},x{},y{},dx{},dy{};AcGeDoubleArray dash;ok(h.getPatternDefinitionAt(0,a,x,y,dx,dy,dash),"pattern definition");return std::hypot(dx,dy);}
inline std::string hatchState(AcDbHatch& h,const std::string& stage) {
    native::Fields fields={{"stage",native::quote(stage)},{"originX",number(h.originPoint().x)},{"originY",number(h.originPoint().y)},{"scale",number(h.patternScale())},{"angle",number(h.patternAngle())},{"pattern",native::quote(std::wstring(h.patternName()))},{"patternType",std::to_string(h.patternType())},{"solid",h.isSolidFill()?"true":"false"},{"lineGeneration",h.lineGenerationEnabled()?"true":"false"},{"numHatchLines",std::to_string(h.numHatchLines())},{"patternDefinitions",std::to_string(h.numPatternDefinitions())}};
    AcGePoint2dArray starts,ends;const auto lineStatus=h.getHatchLinesData(starts,ends);fields.push_back({"getLinesStatus",std::to_string(lineStatus)});fields.push_back({"returnedSegments",std::to_string(starts.length())});
    if(h.numPatternDefinitions()>0){double a{},x{},y{},dx{},dy{};AcGeDoubleArray d;ok(h.getPatternDefinitionAt(0,a,x,y,dx,dy,d),"diagnostic first pattern");fields.push_back({"definitionAngle",number(a)});fields.push_back({"baseX",number(x)});fields.push_back({"baseY",number(y)});fields.push_back({"offsetX",number(dx)});fields.push_back({"offsetY",number(dy)});}
    return native::object(fields);
}
inline void diagnosePattern(const PreparedMaterialBatch& batch,const native::Config& config,native::Evidence& evidence) {
    std::vector<std::string> states;
    for(int experiment=0;experiment<3;++experiment) {
        AcDbDatabase target(true,true);auto copied=batch.cloneToEmpty(target);native::require(copied.canCommit(),"diagnostic independent clone");Working work(target);const auto id=copied.value->front();
        {Open<AcDbHatch> h(id,AcDb::kForWrite);const auto prefix=std::string("experiment-")+std::to_string(experiment);states.push_back(hatchState(*h.p,prefix+"-initial"));
            if(experiment==1){const auto status=h->setOriginPoint({12.25,37.5});states.push_back(native::object({{"operation",native::quote(std::string("setOriginPoint"))},{"status",std::to_string(status)}}));states.push_back(hatchState(*h.p,prefix+"-after-origin"));}
            if(experiment==2){const auto type=h->patternType();const std::wstring name=h->patternName();const auto status=h->setPattern(type,name.c_str());states.push_back(native::object({{"operation",native::quote(std::string("setPattern-same-name-independent-probe-only"))},{"status",std::to_string(status)}}));states.push_back(hatchState(*h.p,prefix+"-after-setPattern"));}
            h->setLineGenerationEnabled(true);const auto evaluated=h->evaluateHatch();states.push_back(native::object({{"operation",native::quote(prefix+"-evaluateHatch")},{"status",std::to_string(evaluated)}}));states.push_back(hatchState(*h.p,prefix+"-immediately-evaluated"));
            int count{};const bool voidOk=voidLines(*h.p,count);states.push_back(native::object({{"operation",native::quote(prefix+"-direct-native-void-check")},{"segments",std::to_string(count)},{"voidOk",voidOk?"true":"false"}}));
            double area{};const auto status=h->getArea(area);states.push_back(native::object({{"operation",native::quote(prefix+"-getArea")},{"status",std::to_string(status)},{"area",number(area)}}));states.push_back(hatchState(*h.p,prefix+"-after-area"));
        }
        {Open<AcDbHatch> h(id);states.push_back(hatchState(*h.p,std::string("experiment-")+std::to_string(experiment)+"-closed-reopened"));}
    }
    auto report=native::create_new(config.output+L"/material-diagnostic.json");native::write_flushed(report.get(),native::object({{"experiments",native::array(states)}}));
    evidence.capability(native::object({{"diagnostic",native::quote(std::string("M1 pattern cache/origin independent experiments"))},{"states",native::array(states)}}));
}inline std::string displayProperties(AcDbHatch& h,AcDbDatabase& db,const std::string& stage) {
    Open<AcDbLayerTableRecord> l(h.layerId());AcString name,plot;ok(l->getName(name),"diagnostic layer name");ok(l->plotStyleName(plot),"diagnostic layer plotstyle");
    native::Fields values={{"stage",native::quote(stage)},{"hatch",hatchState(h,stage)},{"entityVisibility",std::to_string(h.visibility())},{"entityColorIndex",std::to_string(h.colorIndex())},{"entityColorValue",std::to_string(h.color().color())},{"entityColorMethod",std::to_string(h.color().colorMethod())},{"entityTransparency",std::to_string(h.transparency().serializeOut())},{"entityAlpha",std::to_string(h.transparency().alpha())},{"entityTransparencyClear",h.transparency().isClear()?"true":"false"},{"entityLineWeight",std::to_string(h.lineWeight())},{"entityLineTypeScale",number(h.linetypeScale())},{"layerName",native::quote(std::wstring(name.kACharPtr()))},{"layerOff",l->isOff()?"true":"false"},{"layerFrozen",l->isFrozen()?"true":"false"},{"layerLocked",l->isLocked()?"true":"false"},{"layerPlottable",l->isPlottable()?"true":"false"},{"layerViewportDefaultFrozen",l->VPDFLT()?"true":"false"},{"layerColorIndex",std::to_string(l->color().colorIndex())},{"layerColorValue",std::to_string(l->color().color())},{"layerColorMethod",std::to_string(l->color().colorMethod())},{"layerTransparency",std::to_string(l->transparency().serializeOut())},{"layerAlpha",std::to_string(l->transparency().alpha())},{"layerLineWeight",std::to_string(l->lineWeight())},{"layerPlotStyle",native::quote(std::wstring(plot.kACharPtr()))},{"databaseFillMode",db.fillmode()?"true":"false"},{"databaseInsunits",std::to_string(db.insunits())},{"databaseLtscale",number(db.ltscale())},{"hatchStyle",std::to_string(h.hatchStyle())},{"hatchAssociative",h.associative()?"true":"false"},{"hatchElevation",number(h.elevation())},{"hatchNormal",native::array({number(h.normal().x),number(h.normal().y),number(h.normal().z)})},{"backgroundColorValue",std::to_string(h.backgroundColor().color())},{"backgroundColorMethod",std::to_string(h.backgroundColor().colorMethod())}};
    std::vector<std::string> loops;for(int i=0;i<h.numLoops();++i){Adesk::Int32 type=h.loopTypeAt(i);native::Fields f={{"type",std::to_string(type)}};if(type&AcDbHatch::kPolyline){AcGePoint2dArray p;AcGeDoubleArray b;ok(h.getLoopAt(i,type,p,b),"diagnostic loop");std::vector<std::string> coords,bulges;for(int j=0;j<p.length();++j)coords.push_back(native::array({number(p[j].x),number(p[j].y)}));for(int j=0;j<b.length();++j)bulges.push_back(number(b[j]));f.push_back({"points",native::array(coords)});f.push_back({"bulges",native::array(bulges)});}loops.push_back(native::object(f));}values.push_back({"loops",native::array(loops)});
    return native::object(values);
}
inline std::string displayGlobals() {
    native::Fields fields;for(const auto name:{L"FILLMODE",L"HPMAXLINES",L"HPQUICKPREVIEW",L"HPLINETYPE",L"HPASSOC"}){resbuf value{};const auto status=acedGetVar(name,&value);native::Fields item={{"getStatus",std::to_string(status)},{"restype",std::to_string(value.restype)}};if(status==RTNORM){if(value.restype==RTSHORT)item.push_back({"value",std::to_string(value.resval.rint)});else if(value.restype==RTLONG)item.push_back({"value",std::to_string(value.resval.rlong)});else if(value.restype==RTREAL)item.push_back({"value",number(value.resval.rreal)});else if(value.restype==RTSTR){item.push_back({"value",native::quote(std::wstring(value.resval.rstring))});acutDelString(value.resval.rstring);}}fields.push_back({native::utf8(name),native::object(item)});}return native::object(fields);
}inline void diagnoseLoopRebuild(AcDbDatabase& source,AcDbObjectId sourceId,const native::Config& config,native::Evidence& evidence) {
    struct Ring {Adesk::Int32 type;AcGePoint2dArray points;AcGeDoubleArray bulges;};
    std::vector<Ring> original;
    {Open<AcDbHatch> h(sourceId);for(int i=0;i<h->numLoops();++i){Ring r;r.type=h->loopTypeAt(i);native::require((r.type&AcDbHatch::kPolyline)!=0,"known source loops must be public polylines");ok(h->getLoopAt(i,r.type,r.points,r.bulges),"original loop values");original.push_back(r);}}
    std::vector<std::string> steps;
    auto rectangle=[](bool hole,bool reverse,bool zeros){Ring r;r.type=hole?AcDbHatch::kDefault:AcDbHatch::kExternal;const std::vector<AcGePoint2d> p=hole?std::vector<AcGePoint2d>{{1200,900},{2400,900},{2400,2400},{1200,2400},{1200,900}}:std::vector<AcGePoint2d>{{0,0},{10000,0},{10000,6000},{0,6000},{0,0}};for(std::size_t i=0;i<p.size();++i){r.points.append(p[reverse?p.size()-1-i:i]);if(zeros)r.bulges.append(0);}return r;};
    for(int track=0;track<2;++track){
        AcDbDatabase target(true,true);Working work(target);AcDbObjectIdArray input;input.append(sourceId);AcDbIdMapping map;ok(source.wblockCloneObjects(input,acdbSymUtil()->blockModelSpaceId(&target),map,AcDb::kDrcMangleName),"diagnostic source clone");AcDbIdPair pair;pair.setKey(sourceId);native::require(map.compute(pair)&&pair.isCloned(),"source clone mapping");
        Open<AcDbHatch> h(pair.value(),AcDb::kForWrite);const auto prefix=std::string("track-")+std::to_string(track)+"/";
        auto capture=[&](const std::string& stage){const auto status=h->evaluateHatch();steps.push_back(native::object({{"stage",native::quote(prefix+stage)},{"evaluateStatus",std::to_string(status)},{"display",displayProperties(*h.p,target,stage)}}));};
        steps.push_back(native::object({{"stage",native::quote(prefix+"clone-before-evaluate")},{"display",displayProperties(*h.p,target,"clone-before-evaluate")}}));capture("unchanged-evaluate");
        ok(h->setAssociative(false),"diagnostic detach");ok(h->removeAssocObjIds(),"diagnostic detach ids");capture("detached-original-loops");
        if(track==0){
            ok(h->transformBy(AcGeMatrix3d::translation({12.25,37.5,0})),"diagnostic translation");capture("translation-original-loops");
            // Restore the known original geometry after translation, then replace its loops by exactly read values.
            ok(h->transformBy(AcGeMatrix3d::translation({-12.25,-37.5,0})),"diagnostic reverse translation");capture("restored-original-loops");
            while(h->numLoops())ok(h->removeLoopAt(h->numLoops()-1),"remove original loops");
            for(const auto& r:original)ok(h->appendLoop(r.type,r.points,r.bulges),"reappend identical source loop values");capture("recreated-identical-eight-loops");
            ok(h->setHatchStyle(AcDbHatch::kNormal),"diagnostic normal style");capture("identical-eight-loops-normal-style");
        }else{
            while(h->numLoops())ok(h->removeLoopAt(h->numLoops()-1),"remove for new rectangle");const auto outer=rectangle(false,false,false);ok(h->appendLoop(outer.type,outer.points,outer.bulges),"new outer documented kExternal empty bulges");capture("new-outer-only-empty-bulges-original-style");
            ok(h->setHatchStyle(AcDbHatch::kNormal),"new normal style");capture("new-outer-only-normal-style");
            auto hole=rectangle(true,false,false);ok(h->appendLoop(hole.type,hole.points,hole.bulges),"add hole empty bulges");capture("new-outer-and-ccw-hole-empty-bulges");
            ok(h->removeLoopAt(1),"remove ccw hole");hole=rectangle(true,true,false);ok(h->appendLoop(hole.type,hole.points,hole.bulges),"add reversed hole");capture("new-outer-and-cw-hole-empty-bulges");
            h->recordGraphicsModified(true);capture("public-recordGraphicsModified-true");
        }
    }
    auto file=native::create_new(config.output+L"/material-loop-diagnostic.json");const auto json=native::object({{"steps",native::array(steps)},{"scope",native::quote(std::string("two isolated source clones; stepwise public API only; no PAT/global changes or fabricated lines"))}});native::write_flushed(file.get(),json);evidence.capability(json);
}inline void diagnoseSizePosition(AcDbDatabase& source,AcDbObjectId sourceId,const native::Config& config,native::Evidence& evidence) {
    std::vector<std::string> cases;
    for(int position=0;position<2;++position)for(int size=0;size<2;++size){
        // Fixed coordinates are bound test facts from the actual reference 114CF, never production defaults.
        const double ox=position==0?0.0:70833.754221590847;
        const double oy=position==0?0.0:135497.16500728054;
        const double width=size==0?2000.0:10000.0,height=size==0?1000.0:6000.0;
        AcDbDatabase target(true,true);Working work(target);AcDbObjectIdArray ids;ids.append(sourceId);AcDbIdMapping map;
        ok(source.wblockCloneObjects(ids,acdbSymUtil()->blockModelSpaceId(&target),map,AcDb::kDrcMangleName),"2x2 source clone");AcDbIdPair pair;pair.setKey(sourceId);native::require(map.compute(pair)&&pair.isCloned(),"2x2 clone mapping");
        Open<AcDbHatch> h(pair.value(),AcDb::kForWrite);const auto before=hatchState(*h.p,"fresh-clone");double sourceArea{};const auto sourceAreaStatus=h->getArea(sourceArea);
        ok(h->setAssociative(false),"2x2 detach");ok(h->removeAssocObjIds(),"2x2 remove assoc");while(h->numLoops())ok(h->removeLoopAt(h->numLoops()-1),"2x2 remove old loops");
        AcGePoint2dArray points;AcGeDoubleArray bulges;for(const auto point:{AcGePoint2d(ox,oy),AcGePoint2d(ox+width,oy),AcGePoint2d(ox+width,oy+height),AcGePoint2d(ox,oy+height),AcGePoint2d(ox,oy)})points.append(point);
        ok(h->appendLoop(AcDbHatch::kExternal,points,bulges),"2x2 append rectangle");const auto status=h->evaluateHatch(false);const auto evaluated=hatchState(*h.p,"immediate-evaluate-false");double area{};const auto areaStatus=h->getArea(area);
        cases.push_back(native::object({{"positionCase",position==0?"\"origin\"":"\"reference-near\""},{"sizeCase",size==0?"\"2000x1000\"":"\"10000x6000\""},{"x",number(ox)},{"y",number(oy)},{"width",number(width)},{"height",number(height)},{"sourceAreaStatus",std::to_string(sourceAreaStatus)},{"sourceNativeArea",number(sourceArea)},{"before",before},{"evaluateFalseStatus",std::to_string(status)},{"immediate",evaluated},{"areaStatus",std::to_string(areaStatus)},{"nativeArea",number(area)},{"afterArea",displayProperties(*h.p,target,"after-area")}}));
    }
    auto file=native::create_new(config.output+L"/material-size-position-diagnostic.json");const auto json=native::object({{"cases",native::array(cases)},{"globalsReadOnly",displayGlobals()},{"scope",native::quote(std::string("four independent native clones; only rectangle dimensions and position differ; no origin/scale/style/global/PAT changes; not the whole-wall acceptance test"))}});native::write_flushed(file.get(),json);evidence.capability(json);
}inline bool panelLoops(AcDbHatch& h,double ox=0,double oy=0,bool withHole=false){
    if(h.isA()!=AcDbHatch::desc()||h.associative()||h.numLoops()!=(withHole?2:1))return false;AcDbObjectIdArray assoc;ok(h.getAssocObjIds(assoc),"small-panel associations");if(!assoc.isEmpty())return false;
    const double coords[2][4][2]={{{0,0},{2000,0},{2000,1000},{0,1000}},{{200,200},{600,200},{600,600},{200,600}}};
    for(int i=0;i<h.numLoops();++i){Adesk::Int32 type{};AcGePoint2dArray p;AcGeDoubleArray b;ok(h.getLoopAt(i,type,p,b),"small-panel loops");if(!(type&AcDbHatch::kPolyline)||p.length()!=5)return false;for(int j=0;j<4;++j)if(!nearValue(p[j].x,coords[i][j][0]+ox)||!nearValue(p[j].y,coords[i][j][1]+oy))return false;if(p.first()!=p.last())return false;for(int j=0;j<b.length();++j)if(b[j]!=0)return false;}return true;
}
inline bool smallWindowVoid(AcDbHatch& h,int& count){
    count=h.numHatchLines();if(count<=0)return false;
    for(int i=0;i<count;++i){AcGePoint2d a,b;ok(h.getHatchLineDataAt(i,a,b),"small window native line");double lo=0,hi=1;bool intersects=true;const double av[2]={a.x,a.y},dv[2]={b.x-a.x,b.y-a.y};
        for(int k=0;k<2;++k){if(std::abs(dv[k])<1e-14){if(av[k]<=200.00001||av[k]>=599.99999){intersects=false;break;}}else{auto t1=(200.00001-av[k])/dv[k],t2=(599.99999-av[k])/dv[k];if(t1>t2)std::swap(t1,t2);lo=(std::max)(lo,t1);hi=(std::min)(hi,t2);if(lo>=hi){intersects=false;break;}}}if(intersects&&hi>lo)return false;
    }return true;
}class DocumentLock final {
public:
    explicit DocumentLock(AcApDocument* document):document_(document){ok(acDocManager->lockDocument(document_,AcAp::kWrite,nullptr,nullptr,false),"lock owned test document");}
    ~DocumentLock(){if(document_)acDocManager->unlockDocument(document_);}
    DocumentLock(const DocumentLock&)=delete;DocumentLock& operator=(const DocumentLock&)=delete;
private:AcApDocument* document_;
};
inline void rejectDocumentTargets(const PreparedMaterialBatch& batch,const std::function<void(const std::string&,bool,const std::string&)>& expect) {
    native::require(acDocManager->isApplicationContext(),"document-target test requires explicit application context");
    auto* original=acDocManager->curDocument();
    struct OwnedDocuments {AcApDocument* original;std::vector<AcApDocument*> documents;~OwnedDocuments(){acDocManager->setCurDocument(original,AcAp::kNone,true);for(auto* d:documents)acDocManager->appContextCloseDocument(d);acDocManager->setCurDocument(original,AcAp::kNone,true);}} owned{original,{}};
    for(int i=0;i<2;++i){ok(acDocManager->appContextNewDocument(L"C:/Users/PC/AppData/Local/Autodesk/AutoCAD 2020/R23.1/chs/Template/acadiso.dwt"),"new owned target document");auto* doc=acDocManager->curDocument();native::require(doc&&doc!=original&&std::find(owned.documents.begin(),owned.documents.end(),doc)==owned.documents.end(),"fresh owned document identity");owned.documents.push_back(doc);native::require(modelRoots(*doc->database()).isEmpty(),"empty target fixture");}
    auto* current=owned.documents[0];auto* other=owned.documents[1];
    {DocumentLock currentLock(current),otherLock(other);ok(acDocManager->setCurDocument(current,AcAp::kNone,true),"select owned current document");AcDbDatabase side(true,true);Monitor currentMonitor(*current->database()),otherMonitor(*other->database());
        {Working work(side);native::require(acDocManager->curDocument()==current&&acdbHostApplicationServices()->workingDatabase()==&side,"current/working DB intentionally differ");
            auto result=batch.cloneToEmpty(*current->database());expect("CurrentDocumentRejectedWhenWorkingDbIsSideDb",!result.canCommit()&&!result.value&&modelRoots(*current->database()).isEmpty()&&currentMonitor.changes==0,result.issues.empty()?"REGRESSION: current document accepted as independent target":result.issues.front().message);
            auto second=batch.cloneToEmpty(*other->database());expect("OtherDocumentRejectedWhenWorkingDbIsSideDb",!second.canCommit()&&!second.value&&modelRoots(*other->database()).isEmpty()&&otherMonitor.changes==0,second.issues.empty()?"REGRESSION: other document accepted as independent target":second.issues.front().message);
        }
    }
    ok(acDocManager->setCurDocument(original,AcAp::kNone,true),"restore original current document");
}}







