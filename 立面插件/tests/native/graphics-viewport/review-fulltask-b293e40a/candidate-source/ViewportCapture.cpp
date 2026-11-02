#include "ViewportProbe.h"
#include "ViewportTrace.h"
#include "OwnedControlFile.h"
#include <aced.h>
#include <adscodes.h>
#include <acedCmdNF.h>
#include <acdocman.h>
#include <accmd.h>
// core_rxmfcapi.h also declares unused CPoint coordinate overloads.
class CPoint;
#include <core_rxmfcapi.h>
#include <acutads.h>
#include <dbents.h>
#include <dbmtext.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <dbidmap.h>
#include <dbapserv.h>
#include <dbproxy.h>
#include <acgi.h>
#include <drawable.h>
#include <functional>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <algorithm>
namespace bce::viewport {
using namespace bce::native;
using namespace bce::graphics;
namespace {
std::string number(double x){require(std::isfinite(x),"nonfinite host value");std::ostringstream s;s.imbue(std::locale::classic());s<<std::setprecision(17)<<x;return s.str();}
std::string handle(AcDbObjectId id){if(id.isNull())return {};ACHAR value[32]{};id.handle().getIntoAsciiBuffer(value);return utf8(value);}
std::string pt(const AcGePoint3d& p){return array({number(p.x),number(p.y),number(p.z)});}
std::string vec(const AcGeVector3d& p){return array({number(p.x),number(p.y),number(p.z)});}
Triple triple(const AcGePoint3d& p){return {p.x,p.y,p.z};}
Triple triple(const AcGeVector3d& p){return {p.x,p.y,p.z};}
std::string mat(const AcGeMatrix3d& m){std::vector<std::string> a;for(int r=0;r<4;++r)for(int c=0;c<4;++c)a.push_back(number(m(r,c)));return array(a);}
std::string q(const char* p){return quote(std::string(p));}
std::wstring ws(const ACHAR* p){return p?p:L"";}
#include "ViewportFixtures.h"
void verifyRoots(AcDbDatabase& db,const std::vector<AcDbObjectId>& expected){
    Open<AcDbBlockTableRecord> ms(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};ok(ms->newIterator(raw),"target root iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);std::size_t count{};
    for(;!it->done();it->step()){AcDbObjectId id;ok(it->getEntityId(id),"target root id");require(std::find(expected.begin(),expected.end(),id)!=expected.end(),"unexpected target model root");++count;}
    require(count==12&&count==expected.size(),"actual target root count mismatch");
}
struct Session;
void captureOnIdle();
struct QueueReactor final:AcApDocManagerReactor,AcEdCommandStackReactor {
    explicit QueueReactor(Session& owner):s(owner){}
    void documentBecameCurrent(AcApDocument* doc) override;
    void documentActivated(AcApDocument* doc) override;
    void commandWillStart(AcEdCommand* command,AcEdCommandEventArgs*) override;
    void commandEnded(AcEdCommand* command,AcEdCommandEventArgs* args) override;
    Session& s;
};
struct Session {
    Config config;Evidence& evidence;File log;
    AcDbDatabase* ambient{};std::unique_ptr<Monitor> ambientMonitor;
    std::unique_ptr<AcDbDatabase> source;std::unique_ptr<Monitor> sourceMonitor;
    std::unique_ptr<Monitor> targetMonitor;AcDbDatabase* target{};
    std::string sourceFingerprint,targetSHA;std::uint64_t preparedTargetBytes{};std::vector<std::wstring> rootHandles;
    std::vector<AcDbObjectId> roots;std::vector<std::string> classes;
    std::unique_ptr<QueueReactor> queue;bool queueAttached{},commandQueueAttached{},idleAttached{},targetBecameCurrent{},openCommandStarted{},openCommandEnded{},queued{},capturing{};
    std::string callbackError;Result result;
    std::array<std::array<std::vector<std::string>,12>,2> streams;
    std::array<std::array<bool,12>,2> seenWorld{},seenViewport{};
    int round{-1},index{-1};bool recording{};std::vector<std::string> path;
    std::vector<const AcGiDrawable*> authorizedTransient;
    std::size_t logBytes{},eventCount{};
    explicit Session(const Config& c,Evidence& e):config(c),evidence(e),log(create_new(c.output+L"\\viewport-callbacks.jsonl")){}
    void write(const std::string& line){require(++eventCount<=250000&&logBytes+line.size()<128*1024*1024,"N7 callback budget");logBytes+=line.size();write_flushed(log.get(),line+"\n");}
    void emit(AcGiGeometry& g,const char* kind,Fields fields={}){
        if(index<0||round<0||!recording)return;
        AcGeMatrix3d matrix;g.getModelToWorldTransform(matrix);
        fields.push_back({"kind",q(kind)});fields.push_back({"path",array(path)});fields.push_back({"modelToWorld",mat(matrix)});
        auto line=object(fields);streams[static_cast<std::size_t>(round)][static_cast<std::size_t>(index)].push_back(line);
        write(object({{"sample",std::to_string(index)},{"round",std::to_string(round)},{"request",line}}));
        ++result.samples[static_cast<std::size_t>(index)].observation.coverage.callbacks;
    }
    ControlObservation* observation(){return index>=0&&round==0&&recording?&result.samples[static_cast<std::size_t>(index)].observation:nullptr;}
    void points(AcGiGeometry& g,const char* kind,Adesk::UInt32 count,const AcGePoint3d* points){
        require(count<=100000&&(points||count==0),"invalid geometry points");std::vector<std::string> a;AcGeMatrix3d m;g.getModelToWorldTransform(m);
        PrimitiveObservation p;p.kind=kind;for(Adesk::UInt32 i=0;i<count;++i){a.push_back(pt(points[i]));p.points.push_back(triple(m*points[i]));}
        if(auto* o=observation())o->primitives.push_back(std::move(p));emit(g,kind,{{"points",array(a)}});
    }
    void text(AcGiGeometry& g,const AcGePoint3d& p,const AcGeVector3d& n,const AcGeVector3d& d,const ACHAR* msg,int len,bool raw,const AcGiTextStyle* style,double h,double width,double oblique){
        require(msg&&len>=-1&&len<=65536,"text pointer/length invalid");const auto count=len<0?wcsnlen_s(msg,65537):static_cast<std::size_t>(len);require(count<=65536,"text over budget");
        std::wstring textValue(msg,count);std::vector<std::string> units;for(auto v:textValue)units.push_back(std::to_string(static_cast<unsigned short>(v)));
        AcGeMatrix3d m;g.getModelToWorldTransform(m);
        Fields f={{"text",quote(textValue)},{"utf16",array(units)},{"rawLength",std::to_string(len)},{"copiedCodeUnits",std::to_string(count)},{"raw",raw?"true":"false"},{"position",pt(p)},{"worldPosition",pt(m*p)},{"normal",vec(n)},{"direction",vec(d)},{"height",number(h)},{"width",number(width)},{"oblique",number(oblique)},{"styleKnown",style?"true":"false"}};
        TextObservation t;t.text=textValue;t.position=triple(m*p);t.height=h;t.width=width;t.oblique=oblique;t.styleComplete=style!=nullptr;
        if(style){t.font=ws(style->fileName());t.bigfont=ws(style->bigFontFileName());t.tracking=style->trackingPercent();
            f.push_back({"font",quote(t.font)});f.push_back({"bigFont",quote(t.bigfont)});f.push_back({"styleName",quote(ws(style->styleName()))});f.push_back({"tracking",number(t.tracking)});
            f.push_back({"backward",style->isBackward()?"true":"false"});f.push_back({"upsideDown",style->isUpsideDown()?"true":"false"});
            f.push_back({"vertical",style->isVertical()?"true":"false"});f.push_back({"underline",style->isUnderlined()?"true":"false"});f.push_back({"overline",style->isOverlined()?"true":"false"});f.push_back({"strike",style->isStrikethrough()?"true":"false"});
            if(n.length()>1e-12&&d.length()>1e-12){t.xAxis=triple(m*(d.normal()*h*width*(style->isBackward()?-1:1)));t.yAxis=triple(m*(n.normal().crossProduct(d.normal())*h*(style->isUpsideDown()?-1:1)));t.basisValid=true;}}
        if(auto* o=observation())o->texts.push_back(t);
        if(index>=0&&round==0&&recording)++result.samples[static_cast<std::size_t>(index)].textCalls;
        emit(g,"text",f);
    }
    void guarded(){require(callbackError.empty(),callbackError);ambientMonitor->check();sourceMonitor->check();if(targetMonitor)targetMonitor->check();}
    ~Session(){trace("session.destroy",{{"queued",queued?"true":"false"},{"capturing",capturing?"true":"false"}});if(commandQueueAttached)acedRegCmds->removeReactor(queue.get());if(queueAttached)acDocManager->removeReactor(queue.get());}
};
std::unique_ptr<Session> session;
void QueueReactor::documentBecameCurrent(AcApDocument* doc){
    trace("document.becameCurrent",{{"eventDocPath",quote(std::wstring(doc&&doc->fileName()?doc->fileName():L""))},{"queued",s.queued?"true":"false"},{"capturing",s.capturing?"true":"false"}});
    if(s.queued||s.capturing||!doc||!doc->database())return;
    try {const ACHAR* name{};if(doc->database()->getFilename(name)!=Acad::eOk||!name)return;
        const auto expected=s.config.session+L"\\viewport-native-job\\control.dwg";
        if(_wcsicmp(name,expected.c_str())!=0){trace("document.pathRejected",{{"path",quote(std::wstring(name))},{"expected",quote(expected)}});return;}
        s.targetBecameCurrent=true;trace("document.captureDeferredUntilOpenEndedAndIdle");
    }catch(const std::exception& e){s.callbackError=e.what();}catch(...){s.callbackError="document reactor failure";}
}
void QueueReactor::documentActivated(AcApDocument* doc){
    try {trace("document.activated",{{"eventDocPath",quote(std::wstring(doc&&doc->fileName()?doc->fileName():L""))}});}catch(...){s.callbackError="document activation trace failure";}
}
void QueueReactor::commandWillStart(AcEdCommand* command,AcEdCommandEventArgs*){
    if(command&&command->globalName()&&_wcsicmp(command->globalName(),L"S1OPENPROBECONTROL")==0){s.openCommandStarted=true;trace("captureGate.openCommandStarted");}
}
void QueueReactor::commandEnded(AcEdCommand* command,AcEdCommandEventArgs* args){
    if(command&&command->globalName()&&_wcsicmp(command->globalName(),L"S1OPENPROBECONTROL")==0){
        s.openCommandEnded=s.openCommandStarted&&args&&args->status()==kCmdStatusNone;
        trace("captureGate.openCommandEnded",{{"successful",s.openCommandEnded?"true":"false"}});
    }
}
void captureOnIdle(){
    if(!session)return;auto& s=*session;
    if(s.queued||s.capturing||!s.targetBecameCurrent||!s.openCommandEnded)return;
    try {
        auto* doc=curDoc();if(!doc||doc!=acDocManager->mdiActiveDocument()||!doc->isQuiescent()||acDocManager->inputPending(doc)!=0)return;
        auto* db=doc->database();if(!db||db!=acdbHostApplicationServices()->workingDatabase()||db==s.ambient||db==s.source.get())return;
        const ACHAR* name{};if(db->getFilename(name)!=Acad::eOk||!name)return;
        const auto expected=s.config.session+L"\\viewport-native-job\\control.dwg";if(_wcsicmp(name,expected.c_str())!=0)return;
        s.queued=true;trace("idle.captureQueue.before",{{"command",quote(std::wstring(L"BCE_VIEWPORT_CAPTURE\n"))},{"openCommandEnded","true"},{"documentQuiescent","true"},{"inputPending","0"}});
        const auto status=acDocManager->sendStringToExecute(doc,L"BCE_VIEWPORT_CAPTURE\n",false,false,false);
        trace("idle.captureQueue.after",{{"status",std::to_string(status)}});
        if(status!=Acad::eOk)s.callbackError="idle sendStringToExecute failed status="+std::to_string(status);
    }catch(const std::exception& e){s.callbackError=e.what();}catch(...){s.callbackError="capture idle gate failure";}
}
#include "ViewportForward.h"
class Overrule final:public AcGiDrawableOverrule {
public:
    explicit Overrule(Session& state):s(state){}
    bool isApplicable(const AcRxObject* value) const override {
        if(!s.capturing)return false;const auto* entity=AcDbEntity::cast(value);
        if(entity&&entity->database())return entity->database()==s.target;
        const auto* drawable=AcGiDrawable::cast(value);
        return drawable&&std::find(s.authorizedTransient.begin(),s.authorizedTransient.end(),drawable)!=s.authorizedTransient.end();
    }
    struct Scope {
        Session& s;int index;bool recording;std::size_t depth;
        Scope(Session& state,AcGiDrawable* d,bool viewport):s(state),index(s.index),recording(s.recording),depth(s.path.size()){
            const auto* e=AcDbEntity::cast(d);const bool resident=e&&e->database();
            require(resident?e->database()==s.target:std::find(s.authorizedTransient.begin(),s.authorizedTransient.end(),d)!=s.authorizedTransient.end(),"non-target overrule");
            const auto found=resident?std::find(s.roots.begin(),s.roots.end(),e->objectId()):s.roots.end();
            if(found!=s.roots.end()) {s.index=static_cast<int>(found-s.roots.begin());auto& seen=viewport?s.seenViewport:s.seenWorld;
                s.recording=s.round>=0&&!seen[static_cast<std::size_t>(s.round)][static_cast<std::size_t>(s.index)];
                if(s.round>=0)seen[static_cast<std::size_t>(s.round)][static_cast<std::size_t>(s.index)]=true;}
            s.path.push_back(resident?quote(handle(e->objectId())):quote(std::string("transient:")+utf8(d->isA()->name())));
            if(auto* o=s.observation()){++o->coverage.drawRequests;++o->coverage.drawsEntered;}
        }
        ~Scope(){s.index=index;s.recording=recording;s.path.resize(depth);}
    };
    void context(AcGiCommonDraw& d,bool viewport,AcGiViewportDraw* vd=nullptr){
        if(s.index<0||s.round<0)return;auto& sample=s.result.samples[static_cast<std::size_t>(s.index)];
        if(viewport)++sample.viewportCalls;else ++sample.worldCalls;
        auto* c=d.context();const bool valid=c&&c->database()==s.target;
        sample.hostContext=sample.contextCalls?sample.hostContext&&valid:valid;++sample.contextCalls;
        Fields fields={{"kind",q("hostDrawContext")},{"sample",std::to_string(s.index)},{"round",std::to_string(s.round)},{"viewportDraw",viewport?"true":"false"},{"contextAddress",std::to_string(reinterpret_cast<std::uintptr_t>(c))},{"contextDatabaseMatchesTarget",valid?"true":"false"},{"regenType",std::to_string(d.regenType())},{"dragging",d.isDragging()?"true":"false"},{"secondaryCall",d.secondaryCall()?"true":"false"},{"annotationScale","null"}};
        if(vd){auto& v=vd->viewport();AcGePoint3d camera,target;AcGeVector3d up;AcGeMatrix3d eye;AcGePoint2d low,high,pixel;
            v.getCameraLocation(camera);v.getCameraTarget(target);v.getCameraUpVector(up);v.getWorldToEyeTransform(eye);v.getViewportDcCorners(low,high);v.getNumPixelsInUnitSquare(target,pixel);
            fields.push_back({"camera",pt(camera)});fields.push_back({"target",pt(target)});fields.push_back({"up",vec(up)});fields.push_back({"worldToEye",mat(eye)});fields.push_back({"dcCorners",array({number(low.x),number(low.y),number(high.x),number(high.y)})});fields.push_back({"pixelsPerUnit",array({number(pixel.x),number(pixel.y)})});
            fields.push_back({"viewportId",std::to_string(v.viewportId())});fields.push_back({"viewportObject",quote(handle(vd->viewportObjectId()))});fields.push_back({"sequence",std::to_string(vd->sequenceNumber())});fields.push_back({"perspective",v.isPerspective()?"true":"false"});fields.push_back({"linetypeScaleMultiplier",number(v.linetypeScaleMultiplier())});sample.hostViewport=true;sample.finiteViewport=true;
        }s.write(object(fields));
    }
    Adesk::Boolean worldDraw(AcGiDrawable* d,AcGiWorldDraw* wd) override {
        try {require(wd,"null host WorldDraw");Scope scope(s,d,false);context(*wd,false);WorldDraw wrapper(*wd,s);const auto result=AcGiDrawableOverrule::worldDraw(d,&wrapper);if(s.index>=0&&s.round>=0)++s.result.samples[static_cast<std::size_t>(s.index)].forwarded;return result;}
        catch(const std::exception& e){s.callbackError=e.what();return Adesk::kTrue;}catch(...){s.callbackError="worldDraw exception";return Adesk::kTrue;}
    }
    void viewportDraw(AcGiDrawable* d,AcGiViewportDraw* vd) override {
        try {require(vd,"null host ViewportDraw");Scope scope(s,d,true);context(*vd,true,vd);ViewportDraw wrapper(*vd,s);AcGiDrawableOverrule::viewportDraw(d,&wrapper);if(s.index>=0&&s.round>=0)++s.result.samples[static_cast<std::size_t>(s.index)].forwarded;}
        catch(const std::exception& e){s.callbackError=e.what();}catch(...){s.callbackError="viewportDraw exception";}
    }
private:Session& s;
};
std::unique_ptr<Overrule> overrule;bool overruleAdded{},oldOverruling{};
bool removeOverrule() noexcept {
    if(overruleAdded){if(AcRxOverrule::removeOverrule(AcGiDrawable::desc(),overrule.get())!=Acad::eOk)return false;overruleAdded=false;AcRxOverrule::setIsOverruling(oldOverruling);}
    overrule.reset();return true;
}
}
void prepare(const Config& config,Evidence& evidence){
    trace("prepare.implementation.enter",{{"existingSession",session?"true":"false"},{"existingOverrule",overrule?"true":"false"}});
    require(!session&&!overrule,"N7 already prepared");session=std::make_unique<Session>(config,evidence);auto& s=*session;
    s.ambient=acdbHostApplicationServices()->workingDatabase();require(s.ambient,"ambient missing");
    const auto sink=[&s](const std::string& line){s.write(line);};s.ambientMonitor=std::make_unique<Monitor>(*s.ambient,"ambient",sink);
    s.ambientMonitor->begin("reference-read");s.source=std::make_unique<AcDbDatabase>(false,true);
    {WorkingDatabase scope(*s.source);ok(s.source->readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false,L""),"reference read");ok(s.source->closeInput(true),"reference closeInput");}
    s.sourceMonitor=std::make_unique<Monitor>(*s.source,"reference",sink);s.sourceFingerprint=fingerprint(*s.source);s.guarded();
    evidence.checkpoint("prepare-owned-target");
    {AcDbDatabase target(true,true);WorkingDatabase scope(target);Monitor targetEvents(target,"target-setup",sink);targetEvents.protect=false;
        auto roots=controls(target);const auto owner=acdbSymUtil()->blockModelSpaceId(&target);
        for(const wchar_t* sourceHandle:{L"ACB0",L"10837",L"AC83",L"CAF3",L"11B93"}){
            AcDbObjectId sourceId;ok(s.source->getAcDbObjectId(sourceId,false,AcDbHandle(sourceHandle)),"source handle");AcDbObjectIdArray ids;ids.append(sourceId);AcDbIdMapping map;
            ok(s.source->wblockCloneObjects(ids,owner,map,AcDb::kDrcMangleName),"clone own target");AcDbIdPair pair;pair.setKey(sourceId);require(map.compute(pair)&&pair.isCloned(),"clone root mapping");roots.push_back(pair.value());s.guarded();
        }
        verifyRoots(target,roots);
        for(const auto id:roots){Open<AcDbEntity> e(id);require(e->database()==&target&&e->ownerId()==owner&&!AcDbProxyEntity::cast(&*e),"target root owner/type");const auto h=handle(id);s.rootHandles.emplace_back(h.begin(),h.end());s.classes.push_back(utf8(e->isA()->name()));}
        const auto filename=config.output+L"\\viewport-target.dwg";require(GetFileAttributesW(filename.c_str())==INVALID_FILE_ATTRIBUTES,"target file exists");ok(target.saveAs(filename.c_str()),"save own target");s.targetSHA=sha256(filename);
        WIN32_FILE_ATTRIBUTE_DATA info{};if(!GetFileAttributesExW(filename.c_str(),GetFileExInfoStandard,&info)){const auto error=GetLastError();require(false,"prepared target metadata failed error="+std::to_string(error));}
        s.preparedTargetBytes=(static_cast<std::uint64_t>(info.nFileSizeHigh)<<32)|info.nFileSizeLow;targetEvents.check();
    }
    require(acdbHostApplicationServices()->workingDatabase()==s.ambient,"prepare working database not restored");s.guarded();require(fingerprint(*s.source)==s.sourceFingerprint,"reference fingerprint changed");
    s.queue=std::make_unique<QueueReactor>(s);acDocManager->addReactor(s.queue.get());s.queueAttached=true;
    acedRegCmds->addReactor(s.queue.get());s.commandQueueAttached=true;
    require(acedRegisterOnIdleWinMsg(captureOnIdle),"N7 idle hook registration failed");s.idleAttached=true;
    std::vector<std::string> handles;for(const auto& h:s.rootHandles)handles.push_back(quote(h));
    const auto ready=object({{"contract",q("bce.graphics-viewport-prepare.v1")},{"nonce",quote(config.nonce)},{"pid",std::to_string(GetCurrentProcessId())},{"targetPath",quote(config.output+L"\\viewport-target.dwg")},{"targetSHA256",quote(s.targetSHA)},{"modelRootCount",std::to_string(s.rootHandles.size())},{"rootHandles",array(handles)},{"sourceSHA",object({{"scheme",quote(config.schemeSHA256)},{"reference",quote(config.referenceSHA256)}})}});
    {auto f=create_new(config.output+L"\\prepare-ready.tmp");write_flushed(f.get(),ready);}
    require(MoveFileExW((config.output+L"\\prepare-ready.tmp").c_str(),(config.output+L"\\prepare-ready.json").c_str(),MOVEFILE_WRITE_THROUGH)!=FALSE,"atomic prepare ready publish failed");
    trace("prepare.readyPublished");
}
Result capture(const Config& config,Evidence& evidence){
    trace("capture.implementation.enter",{{"existingSession",session?"true":"false"}});
    require(session&&session->queued&&session->openCommandEnded&&!session->capturing,"N7 not prepared/queued/open-complete or capture repeated");auto& s=*session;s.capturing=true;s.guarded();
    const auto control=config.session+L"\\viewport-native-job\\control.dwg";s.target=acdbHostApplicationServices()->workingDatabase();
    require(s.target&&s.target!=s.ambient&&s.target!=s.source.get()&&curDoc()&&curDoc()->database()==s.target,"capture target/context invalid");
    const ACHAR* filename{};ok(s.target->getFilename(filename),"target filename");require(filename&&_wcsicmp(filename,control.c_str())==0,"capture target path mismatch");
    const auto fileObserver=[](const std::string& event,const Fields& fields){trace(event.c_str(),fields);};
    const auto beforeFile=readOwnedControl(config.session,s.targetSHA,s.preparedTargetBytes,fileObserver);
    for(std::size_t i=0;i<s.rootHandles.size();++i){AcDbObjectId id;ok(s.target->getAcDbObjectId(id,false,AcDbHandle(s.rootHandles[i].c_str())),"reopened root");Open<AcDbEntity> e(id);require(utf8(e->isA()->name())==s.classes[i]&&e->ownerId()==acdbSymUtil()->blockModelSpaceId(s.target),"reopened root identity");s.roots.push_back(id);}
    verifyRoots(*s.target,s.roots);
    s.targetMonitor=std::make_unique<Monitor>(*s.target,"target-capture",[&s](const std::string& l){s.write(l);});s.targetMonitor->protect=false;
    s.result.prepared=true;
    // Exercise actual registration failure cleanup before requesting any drawing.
    oldOverruling=AcRxOverrule::isOverruling();
    {
        overrule=std::make_unique<Overrule>(s);
        ok(AcRxOverrule::addOverrule(AcGiDrawable::desc(),overrule.get()),"failure-fixture add overrule");overruleAdded=true;AcRxOverrule::setIsOverruling(true);
        struct FailureScope {~FailureScope(){removeOverrule();}};
        try {FailureScope guard;throw std::runtime_error("N7 intentional scope exit");}catch(const std::runtime_error&){}
        s.result.failureCleanup=!overruleAdded&&!overrule&&AcRxOverrule::isOverruling()==oldOverruling;
        require(s.result.failureCleanup,"injected registration scope cleanup failed");
    }
    oldOverruling=AcRxOverrule::isOverruling();overrule=std::make_unique<Overrule>(s);ok(AcRxOverrule::addOverrule(AcGiDrawable::desc(),overrule.get()),"add N7 overrule");overruleAdded=true;AcRxOverrule::setIsOverruling(true);
    struct Remove {~Remove(){removeOverrule();}} remove;
    {AcDbObjectId sourceId;ok(s.source->getAcDbObjectId(sourceId,false,AcDbHandle(L"ACB0")),"source negative-filter fixture");Open<AcDbEntity> e(sourceId);Open<AcDbEntity> own(s.roots.front());s.result.targetOnly=!overrule->isApplicable(&*e)&&overrule->isApplicable(&*own);}
    evidence.checkpoint("natural-host-regeneration");
    for(int round=0;round<2;++round){s.round=round;require(acedCommandS(RTSTR,L"_.REGEN",RTNONE)==RTNORM,"native REGEN failed");s.round=-1;s.guarded();}
    s.result.naturalRegen=true;require(removeOverrule(),"remove N7 overrule failed");s.result.registrationRemoved=!overruleAdded;s.result.globalOverruleRestored=AcRxOverrule::isOverruling()==oldOverruling;
    s.result.workingDbRestored=acdbHostApplicationServices()->workingDatabase()==s.target;
    require(fingerprint(*s.source)==s.sourceFingerprint,"reference public fingerprint changed");s.guarded();s.result.sourceProtected=true;s.result.ambientProtected=true;
    for(std::size_t i=0;i<12;++i){auto& v=s.result.samples[i];v.stable=!s.streams[0][i].empty()&&s.streams[0][i]==s.streams[1][i];
        s.result.records.push_back(object({{"sample",std::to_string(i)},{"handle",quote(s.rootHandles[i])},{"class",quote(s.classes[i])},{"worldCalls",std::to_string(v.worldCalls)},{"viewportCalls",std::to_string(v.viewportCalls)},{"forwarded",std::to_string(v.forwarded)},{"textCalls",std::to_string(v.textCalls)},{"stable",v.stable?"true":"false"},{"round0RequestCount",std::to_string(s.streams[0][i].size())},{"round1RequestCount",std::to_string(s.streams[1][i].size())},{"glyphComplete","false"}}));}
    const auto afterFile=readOwnedControl(config.session,s.targetSHA,s.preparedTargetBytes,fileObserver);
    require(beforeFile.identity.sameStable(afterFile.identity),"owned control identity/size/timestamps changed across capture");
    s.result.records.push_back(object({{"targetEvents",array(s.targetMonitor->events)},{"sourceEvents",array(s.sourceMonitor->events)},{"ambientEvents",array(s.ambientMonitor->events)},{"referenceFingerprintUnchanged","true"},{"targetDiskSHA",quote(afterFile.sha)},{"savedTargetSHA",quote(s.targetSHA)},{"ownedFileBefore",beforeFile.evidence},{"ownedFileAfter",afterFile.evidence}}));
    return s.result;
}
bool cleanup() noexcept {if(!removeOverrule())return false;if(session&&session->idleAttached){if(!acedRemoveOnIdleWinMsg(captureOnIdle))return false;session->idleAttached=false;}session.reset();return true;}
}
