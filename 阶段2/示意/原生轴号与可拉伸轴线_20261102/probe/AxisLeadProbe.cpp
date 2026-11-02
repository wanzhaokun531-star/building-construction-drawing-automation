#include <Windows.h>
#include <aced.h>
#include <acedads.h>
#include <acedCmdNF.h>
#include <adscodes.h>
#include <acutads.h>
#include <dbents.h>
#include <dbgrip.h>
#include <dbtrans.h>
#include <dbapserv.h>
#include <acdocman.h>
#include <rxregsvc.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <cmath>

namespace {
namespace fs=std::filesystem;
constexpr double eps=1e-6;
int moduleAnchor;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void check(Acad::ErrorStatus status,const char* message){require(status==Acad::eOk,message);}
std::wstring setting(const wchar_t* key){HMODULE module{};require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&moduleAnchor),&module)!=0,"own module identity");wchar_t path[32768]{},value[32768]{};require(GetModuleFileNameW(module,path,32768)!=0,"own module filename");auto ini=fs::path(path);ini.replace_extension(L".ini");require(GetPrivateProfileStringW(L"probe",key,L"",value,32768,ini.c_str())>0,"owned probe setting");return value;}
std::string quote(const std::string& s){std::string out="\"";for(char c:s){if(c=='"'||c=='\\')out+='\\';if(c=='\n'){out+="\\n";continue;}out+=c;}return out+'"';}
struct Geometry {std::vector<AcGePoint3d> grips,stretch;};
Geometry geometry(AcDbEntity& e){Geometry out;AcGePoint3dArray p;AcDbIntArray a,b;check(e.getGripPoints(p,a,b),"read native legacy grip points");for(int i=0;i<p.length();++i)out.grips.push_back(p[i]);p.setLogicalLength(0);check(e.getStretchPoints(p),"read native stretch points");for(int i=0;i<p.length();++i)out.stretch.push_back(p[i]);return out;}
bool same(const std::vector<AcGePoint3d>& a,const std::vector<AcGePoint3d>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].distanceTo(b[i])>eps)return false;return true;}
bool same(const Geometry& a,const Geometry& b){return same(a.grips,b.grips)&&same(a.stretch,b.stretch);}
Geometry readGeometry(AcDbObjectId id){AcDbEntity* e{};check(acdbOpenObject(e,id,AcDb::kForRead),"open native geometry");struct Close {AcDbEntity* e;~Close(){if(e)e->close();}} close{e};return geometry(*e);}
std::string points(const std::vector<AcGePoint3d>& p){std::ostringstream s;s<<std::setprecision(17)<<'[';for(std::size_t i=0;i<p.size();++i){if(i)s<<',';s<<'['<<p[i].x<<','<<p[i].y<<','<<p[i].z<<']';}return s.str()+']';}
std::string json(const Geometry& g){return "{\"grips\":"+points(g.grips)+",\"stretch\":"+points(g.stretch)+'}';}
struct View {double pixelsPerUnit{};int size{};AcGeVector3d direction;};
View view(){resbuf screen{},height{},size{},dir{};require(acedGetVar(L"SCREENSIZE",&screen)==RTNORM&&acedGetVar(L"VIEWSIZE",&height)==RTNORM&&acedGetVar(L"GRIPSIZE",&size)==RTNORM&&acedGetVar(L"VIEWDIR",&dir)==RTNORM,"native view parameters");require(height.restype==RTREAL&&height.resval.rreal>0&&size.restype==RTSHORT&&size.resval.rint>0,"native view types");View v;v.pixelsPerUnit=screen.resval.rpoint[1]/height.resval.rreal;v.size=size.resval.rint;v.direction={dir.resval.rpoint[0],dir.resval.rpoint[1],dir.resval.rpoint[2]};require(std::isfinite(v.pixelsPerUnit)&&v.pixelsPerUnit>0&&v.direction.z>0&&std::abs(v.direction.x)<eps&&std::abs(v.direction.y)<eps,"top view required");return v;}
struct Grips {
 AcDbEntity& entity;AcDbGripDataPtrArray data;bool acquired{},cleaned{},safe=true;
 explicit Grips(AcDbEntity& e):entity(e){}
 void cleanup() noexcept {if(!acquired||cleaned)return;cleaned=true;try{entity.gripStatus(AcDb::kGripsToBeDeleted);}catch(...){safe=false;}std::set<const void*> alt,wrappers;for(int i=0;i<data.length();++i)if(data[i]){auto* p=data[i]->alternateBasePoint();if(p&&alt.insert(p).second)try{delete p;}catch(...){safe=false;}}for(int i=0;i<data.length();++i)if(data[i]&&wrappers.insert(data[i]).second)try{delete data[i];}catch(...){safe=false;}data.setLogicalLength(0);try{entity.gripStatus(AcDb::kGripsDone);}catch(...){safe=false;}}
 ~Grips(){cleanup();}
};
struct Transaction {
 AcDbTransactionManager* manager;AcTransaction* transaction;bool done{};
 explicit Transaction(AcDbDatabase& db):manager(db.transactionManager()),transaction(nullptr){require(manager&&manager->numActiveTransactions()==0,"idle transaction manager required");transaction=manager->startTransaction();require(transaction!=nullptr,"start native grip transaction");}
 void finish(bool commit){require(!done,"transaction already finished");const auto status=commit?manager->endTransaction():manager->abortTransaction();done=true;check(status,"finish native grip transaction");}
 ~Transaction(){if(!done)manager->abortTransaction();}
 AcDbEntity& entity(AcDbObjectId id){AcDbObject* object{};check(transaction->getObject(object,id,AcDb::kForWrite),"open transaction native axis");auto* e=AcDbEntity::cast(object);require(e&&!e->isAProxy()&&std::wstring(e->isA()->name())==L"TDbAxisLabelSet","real native axis required");return *e;}
};
struct Attempt {int index{},second=-1,count{},status=-1;bool matched{},committed{},hasAppData{},innerMatched{};Geometry after,firstAfter;};
int move(AcDbEntity& e,int index,const AcGeVector3d& offset,const AcGePoint3d* target,const View& v,int& count,bool& hasData){Grips owned(e);owned.acquired=true;check(e.getGripPoints(owned.data,v.pixelsPerUnit,v.size,v.direction,0),"modern native getGripPoints");count=owned.data.length();require(count>0&&count<=64,"modern grip count");int status=-1;if(index<count&&owned.data[index]&&owned.data[index]->appData()){hasData=true;const auto delta=target?*target-owned.data[index]->gripPoint():offset;void* const identity=owned.data[index]->appData();AcDbVoidPtrArray selected;selected.append(identity);status=static_cast<int>(e.moveGripPointsAt(selected,delta,0));}owned.cleanup();require(owned.safe,"modern grip lifecycle cleanup");return status;}
Attempt apply(AcDbDatabase& db,AcDbObjectId id,int index,int second,std::size_t anchor,const Geometry& before,const Geometry& expected,const View& v,bool commit,const AcGeVector3d& calibration=AcGeVector3d(0,0,0)){Attempt out;out.index=index;out.second=second;Transaction tx(db);auto& e=tx.entity(id);const auto target=expected.grips[anchor]+calibration;out.status=move(e,index,AcGeVector3d::kIdentity,&target,v,out.count,out.hasAppData);out.firstAfter=geometry(e);out.after=out.firstAfter;out.innerMatched=out.status==0&&out.after.grips.size()==before.grips.size()&&out.after.grips[anchor].distanceTo(expected.grips[anchor])<eps;
 if(out.innerMatched&&!same(out.after,expected)&&second>=0){const auto restore=before.grips[anchor+1]-out.after.grips[anchor+1];out.status=move(e,second,restore,nullptr,v,out.count,out.hasAppData);out.after=geometry(e);}out.matched=out.status==0&&same(out.after,expected);out.committed=commit&&out.matched;tx.finish(out.committed);return out;}
double number(const resbuf* r){require(r&&r->restype==RTREAL&&std::isfinite(r->resval.rreal),"finite real argument required");return r->resval.rreal;}
int fixPoint(){std::unique_ptr<resbuf,decltype(&acutRelRb)> args(acedGetArgs(),acutRelRb);std::string error;fs::path output;std::vector<Attempt> attempts;Geometry before,expected,after;bool completed=false;int chosen=-1;
 try{auto* a=args.get();require(a&&a->restype==RTENAME,"native entity argument");AcDbObjectId id;check(acdbGetObjectId(id,a->resval.rlname),"native argument entity id");a=a->rbnext;double n[6]{};for(auto& value:n){value=number(a);a=a->rbnext;}require(a&&a->restype==RTSTR&&a->resval.rstring&&!a->rbnext,"case identifier argument");std::wstring name=a->resval.rstring;require(!name.empty()&&name.find_first_not_of(L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::wstring::npos,"safe case identifier");output=fs::path(setting(L"output"))/(L"native-lead-"+name+L".json");require(!fs::exists(output),"probe report must be new");auto* doc=acDocManager?acDocManager->curDocument():nullptr;require(doc&&acDocManager->mdiActiveDocument()==doc&&!acDocManager->isApplicationContext(),"owned current document context");require(!_wcsicmp(fs::weakly_canonical(doc->fileName()).c_str(),fs::weakly_canonical(setting(L"control")).c_str()),"owned document filename");auto& db=*doc->database();require(id.database()==&db&&acdbHostApplicationServices()->workingDatabase()==&db,"native axis database");before=readGeometry(id);expected=before;AcGePoint3d old(n[0],n[1],n[2]),target(n[3],n[4],n[5]);int g=0,s=0;std::size_t anchor=0;for(std::size_t i=0;i<expected.grips.size();++i)if(expected.grips[i].distanceTo(old)<eps){expected.grips[i]=target;anchor=i;++g;}for(auto& p:expected.stretch)if(p.distanceTo(old)<eps){p=target;++s;}require(g==1&&s==1&&anchor%4==0&&anchor+1<before.grips.size(),"unique public inner endpoint required");const auto v=view();int count=1,secondChosen=-1;AcGeVector3d chosenCalibration(0,0,0);for(int i=0;i<count&&chosen<0;++i){auto first=apply(db,id,i,-1,anchor,before,expected,v,false);count=first.count;attempts.push_back(first);require(same(readGeometry(id),before),"native grip trial rollback geometry differs");AcGeVector3d calibration(0,0,0);if(!first.innerMatched&&first.status==0&&first.after.grips.size()==before.grips.size()&&first.after.grips[anchor].distanceTo(before.grips[anchor])>eps){calibration=target-first.after.grips[anchor];first=apply(db,id,i,-1,anchor,before,expected,v,false,calibration);attempts.push_back(first);require(same(readGeometry(id),before),"calibrated trial rollback differs");}if(first.matched){chosen=i;chosenCalibration=calibration;break;}if(!first.innerMatched)continue;for(int j=0;j<count;++j){auto trial=apply(db,id,i,j,anchor,before,expected,v,false,calibration);attempts.push_back(trial);require(same(readGeometry(id),before),"native grip paired trial rollback differs");if(trial.matched){chosen=i;secondChosen=j;chosenCalibration=calibration;break;}}}
 require(chosen>=0,"no modern grip pair preserves the circle at the requested inner endpoint");auto done=apply(db,id,chosen,secondChosen,anchor,before,expected,v,true,chosenCalibration);attempts.push_back(done);require(done.committed,"modern grip final semantic match");require(acedCommandS(RTSTR,L"_.REGEN",RTNONE)==RTNORM,"regenerate native axis");after=readGeometry(id);require(same(after,expected),"modern grip persisted geometry differs");completed=true;
 }catch(const std::exception& e){error=e.what();}catch(...){error="unknown native grip exception";}
 if(!output.empty()){std::ostringstream out;out<<"{\"contract\":\"bcp.oneoff-native-axis-lead.v1\",\"completed\":"<<(completed?"true":"false")<<",\"chosenModernGrip\":"<<chosen<<",\"opaqueAppDataDecoded\":false,\"before\":"<<json(before)<<",\"expected\":"<<json(expected)<<",\"after\":"<<json(after)<<",\"attempts\":[";for(std::size_t i=0;i<attempts.size();++i){if(i)out<<',';const auto& a=attempts[i];out<<"{\"index\":"<<a.index<<",\"secondIndex\":"<<a.second<<",\"gripCount\":"<<a.count<<",\"status\":"<<a.status<<",\"hasAppData\":"<<(a.hasAppData?"true":"false")<<",\"innerMatched\":"<<(a.innerMatched?"true":"false")<<",\"matched\":"<<(a.matched?"true":"false")<<",\"committed\":"<<(a.committed?"true":"false")<<",\"firstAfter\":"<<json(a.firstAfter)<<",\"after\":"<<json(a.after)<<'}';}out<<"],\"error\":"<<(error.empty()?"null":quote(error))<<'}';std::ofstream f(output,std::ios::binary);f<<out.str();if(!f)completed=false;}
 if(!completed)acutPrintf(L"\nAXLEAD_FIX failed; see owned native report.\n");acedRetInt(completed?1:0);return RTNORM;
}
}
extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message,void* packet){try{if(message==AcRx::kInitAppMsg){acrxUnlockApplication(packet);acrxRegisterAppMDIAware(packet);if(acedDefun(L"AXLEAD_FIX",0)!=RTNORM)return AcRx::kRetError;}else if(message==AcRx::kInvkSubrMsg){fixPoint();}else if(message==AcRx::kUnloadAppMsg){acedUndef(L"AXLEAD_FIX",0);}return AcRx::kRetOK;}catch(...){return AcRx::kRetError;}}
