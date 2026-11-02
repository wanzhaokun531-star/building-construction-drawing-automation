#include <Windows.h>
#include <Wincrypt.h>
#include <rxregsvc.h>
#include <rxdlinkr.h>
#include <rxdict.h>
#include <rxditer.h>
#include <aced.h>
#include <acedads.h>
#include <acdocman.h>
#include <dbapserv.h>
#include <dbsymtb.h>
#include <dbents.h>
#include <dbcurve.h>
#include <dbdim.h>
#include <dbidmap.h>
#include <dbdict.h>
#include <dbspfilt.h>
#include <dbgrip.h>
#include <dbpl.h>
#include <dbmtext.h>
#include "ApronGeometry.h"
#include <dbgroup.h>
#include <dbtable.h>
#include <dbdynblk.h>
#include <sorttab.h>
#include <dbwipe.h>
#include <imgvars.h>
#include <set>
#include <algorithm>
#include <adslib.h>
#include <acutads.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <map>
#include "PlanRules.h"
#include "UniqueRawParts.h"
extern "C" IMAGE_DOS_HEADER __ImageBase;
namespace {
namespace fs=std::filesystem;
void require(bool ok,const char* m){if(!ok)throw std::runtime_error(m);}
void status(Acad::ErrorStatus s,const char* m){require(s==Acad::eOk,m);}
struct File {HANDLE value=INVALID_HANDLE_VALUE;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct Crypto {HCRYPTPROV provider{};HCRYPTHASH hash{};~Crypto(){if(hash)CryptDestroyHash(hash);if(provider)CryptReleaseContext(provider,0);}};
std::string sha(const fs::path& path){Crypto c;require(CryptAcquireContextW(&c.provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)!=0,"SHA provider");require(CryptCreateHash(c.provider,CALG_SHA_256,0,0,&c.hash)!=0,"SHA handle");File f;f.value=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);require(f.value!=INVALID_HANDLE_VALUE,"SHA file read");BYTE buffer[65536];DWORD bytes{};for(;;){require(ReadFile(f.value,buffer,sizeof(buffer),&bytes,nullptr)!=0,"SHA read");if(!bytes)break;require(CryptHashData(c.hash,buffer,bytes,0)!=0,"SHA bytes");}BYTE digest[32];bytes=sizeof(digest);require(CryptGetHashParam(c.hash,HP_HASHVAL,digest,&bytes,0)!=0&&bytes==32,"SHA result");std::ostringstream out;for(BYTE b:digest)out<<std::uppercase<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(b);return out.str();}
std::wstring iniPath(){wchar_t b[32768]{};require(GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase),b,32768)>0,"module path");fs::path p(b);p.replace_extension(L".ini");return p.wstring();}
std::wstring ini(const std::wstring& file,const wchar_t* key){wchar_t b[32768]{};const auto n=GetPrivateProfileStringW(L"stage2",key,L"",b,32768,file.c_str());require(n>0&&n<32767,"missing/truncated stage2 setting");return {b,n};}
std::string ascii(const std::wstring& s){require(s.find_first_not_of(L"0123456789ABCDEF") == std::wstring::npos,"nonhex identity");return {s.begin(),s.end()};}
std::wstring desktop(HANDLE h){wchar_t b[512]{};DWORD n{};require(h&&GetUserObjectInformationW(h,UOI_NAME,b,sizeof(b),&n),"desktop read");return b;}
void isolated(const std::wstring& nonce){const auto current=desktop(GetThreadDesktop(GetCurrentThreadId()));HDESK input=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);require(input!=nullptr,"input desktop read");std::wstring active;try{active=desktop(input);}catch(...){CloseDesktop(input);throw;}CloseDesktop(input);require(current==L"CodexS1_"+nonce&&current!=active,"stage2 test command requires attested private desktop");resbuf value{};require(acedGetVar(L"SECURELOAD",&value)==RTNORM&&value.restype==RTSHORT&&value.resval.rint==1,"SECURELOAD must remain1");}
template<class T>struct Close {void operator()(T* p)const noexcept{if(p)p->close();}};
template<class T>std::unique_ptr<T,Close<T>> open(AcDbObjectId id){T* p{};status(acdbOpenObject(p,id,AcDb::kForRead),"read native object");return std::unique_ptr<T,Close<T>>(p);}
AcDbObjectId modelId(AcDbDatabase& db){AcDbBlockTable* raw{};status(db.getBlockTable(raw,AcDb::kForRead),"read block table");std::unique_ptr<AcDbBlockTable,Close<AcDbBlockTable>> table(raw);AcDbObjectId id;status(table->getAt(ACDB_MODEL_SPACE,id),"model id");return id;}
AcDbObjectId find(AcDbDatabase& db,const std::string& handle){const std::wstring h(handle.begin(),handle.end());AcDbObjectId id;status(db.getAcDbObjectId(id,false,AcDbHandle(h.c_str())),"cache object id");return id;}
struct Working {AcDbDatabase* previous;explicit Working(AcDbDatabase* db):previous(acdbHostApplicationServices()->workingDatabase()){acdbHostApplicationServices()->setWorkingDatabase(db);}~Working(){acdbHostApplicationServices()->setWorkingDatabase(previous);}};
bool same(bcp::Point a,const AcGePoint3d& b){return std::abs(a.x-b.x)<=bcp::numericalEpsilonMm&&std::abs(a.y-b.y)<=bcp::numericalEpsilonMm;}
int verify(AcDbDatabase& db,const bcp::Plan& plan,int added=0){Working work(&db);int count=0,walls=0,openings=0;{auto model=open<AcDbBlockTableRecord>(modelId(db));AcDbBlockTableRecordIterator* raw{};status(model->newIterator(raw),"iterate model");std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw);for(;!iterator->done();iterator->step()){AcDbObjectId id;status(iterator->getEntityId(id),"model entity id");auto root=open<AcDbEntity>(id);require(!root->isAProxy(),"proxy model object unsupported");const std::wstring type=root->isA()->name();++count;if(type==L"TDbWall")++walls;if(type==L"TDbOpening"||type==L"TDbGroupOpening")++openings;}}
 require(count==plan.sourceObjectCount+added&&walls==static_cast<int>(plan.walls.size())&&openings==static_cast<int>(plan.openings.size()),"cache omitted/added source objects");
 for(const auto& wall:plan.walls){auto root=open<AcDbEntity>(find(db,wall.handle));require(std::wstring(root->isA()->name())==L"TDbWall","wall type changed");const auto* curve=AcDbCurve::cast(root.get());require(curve!=nullptr,"wall has no public curve");AcGePoint3d a,b;status(curve->getStartPoint(a),"wall native start");status(curve->getEndPoint(b),"wall native end");require(same(wall.start,a)&&same(wall.end,b),"wall cached geometry changed");}
 for(const auto& opening:plan.openings){auto root=open<AcDbEntity>(find(db,opening.handle));const std::wstring type=root->isA()->name();require(type==L"TDbOpening"||type==L"TDbGroupOpening","opening type changed");AcGePoint3dArray points;AcDbIntArray modes,ids;status(root->getGripPoints(points,modes,ids),"opening native grips");require(points.length()>=3&&same(opening.a,points[1])&&same(opening.b,points[2]),"opening cached geometry changed");}
 for(const auto& room:plan.rooms){auto root=open<AcDbEntity>(find(db,room.handle));require(std::wstring(root->isA()->name())==L"TDbText","cached text type changed");AcDbExtents e;status(root->getGeomExtents(e),"native room-label extents");if(!same(room.min,e.minPoint())||!same(room.max,e.maxPoint())){std::ostringstream detail;detail<<std::setprecision(17)<<"cached text extent "<<room.handle<<" expected "<<room.min.x<<","<<room.min.y<<":"<<room.max.x<<","<<room.max.y<<" actual "<<e.minPoint().x<<","<<e.minPoint().y<<":"<<e.maxPoint().x<<","<<e.maxPoint().y;throw std::runtime_error(detail.str());}}
 return count;
}
std::map<std::wstring,std::wstring> modelTypes(AcDbDatabase& db){std::map<std::wstring,std::wstring> result;auto model=open<AcDbBlockTableRecord>(modelId(db));AcDbBlockTableRecordIterator* raw{};status(model->newIterator(raw),"type snapshot iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);for(;!it->done();it->step()){AcDbObjectId id;status(it->getEntityId(id),"type snapshot id");auto root=open<AcDbEntity>(id);AcDbHandle handle;root->getAcDbHandle(handle);wchar_t name[32]{};handle.getIntoAsciiBuffer(name);require(result.emplace(name,root->isA()->name()).second,"duplicate source handle");}return result;}
void writeNew(const fs::path& p,const std::string& data){File f;f.value=CreateFileW(p.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);require(f.value!=INVALID_HANDLE_VALUE,"output exists or cannot create");require(data.size()<=MAXDWORD,"report too large");DWORD wrote{};require(WriteFile(f.value,data.data(),static_cast<DWORD>(data.size()),&wrote,nullptr)!=0&&wrote==data.size(),"report write");}
#include "NativeGroupEndpoints.inc"
#include "NativeAnnotationFactory.inc"
void maskAuditCommand()noexcept{fs::path receipt;try{const auto config=iniPath();isolated(ini(config,L"nonce"));receipt=ini(config,L"receiptDirectory");require(curDoc()!=nullptr,"no active mask document");const fs::path control=curDoc()->fileName();require(control.filename()==L"control.dwg"&&control.parent_path().filename().wstring().rfind(L"stage2-preview-",0)==0,"mask audit requires isolated preview control");receipt=control.parent_path();auto* db=curDoc()->database();require(db!=nullptr,"no active mask database");Working current(db);require(witnessFrameSetting(*db)==0,"active document frame setting differs");resbuf frame{};require(acedGetVar(L"WIPEOUTFRAME",&frame)==RTNORM&&frame.restype==RTSHORT&&frame.resval.rint==0,"active WIPEOUTFRAME is not zero");int count=0;auto model=open<AcDbBlockTableRecord>(modelId(*db));AcDbBlockTableRecordIterator* raw{};status(model->newIterator(raw),"active mask iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);for(;!it->done();it->step()){AcDbObjectId id;status(it->getEntityId(id),"active mask id");auto e=open<AcDbEntity>(id);auto* w=AcDbWipeout::cast(e.get());if(w){require(w->ownerId()==modelId(*db)&&!w->frame(),"active mask border visible");++count;}}require(count==4,"active mask count");writeNew(receipt/L"native-mask-document-audit.json","{\"contract\":\"bcp.native-mask-document-audit.v1\",\"masks\":4,\"frame70\":0,\"WIPEOUTFRAME\":0,\"publicVisibleFrames\":0,\"readOnly\":true,\"error\":null}");}catch(const std::exception& e){try{writeNew(receipt/L"native-mask-document-audit.error",e.what());}catch(...){}}}
void analyseCommand()noexcept{
 std::wstring config;fs::path receipt;try{config=iniPath();isolated(ini(config,L"nonce"));const fs::path source=ini(config,L"source"),cache=ini(config,L"cache"),outputRoot=ini(config,L"outputRoot");receipt=ini(config,L"receiptDirectory");if(GetPrivateProfileIntW(L"stage2",L"standardStudyOnly",0,config.c_str())==1){layerStandardStudy(config);return;}
  require(fs::weakly_canonical(outputRoot)==fs::weakly_canonical(cache.parent_path().parent_path()/L"runs"),"output must stay under stage2 runs");require(fs::weakly_canonical(receipt.parent_path().parent_path())==fs::weakly_canonical(cache.parent_path().parent_path().parent_path()/L"阶段1"),"receipt must stay under stage1/runs");
  const auto sourceHash=sha(source),cacheHash=sha(cache);require(sourceHash==ascii(ini(config,L"sourceHash"))&&cacheHash==ascii(ini(config,L"cacheHash")),"frozen input/cache hash mismatch");std::ifstream input(cache);require(static_cast<bool>(input),"cache read");auto plan=bcp::readCache(input);require(plan.sourceSha==sourceHash,"cached source identity mismatch");
  fs::path decisionsPath;std::string decisionsHash,decisionsText;wchar_t decisionSetting[32768]{};const auto decisionLength=GetPrivateProfileStringW(L"stage2",L"axisDecisions",L"",decisionSetting,32768,config.c_str());require(decisionLength<32767,"axis decision path truncated");if(decisionLength){decisionsPath=decisionSetting;require(fs::weakly_canonical(decisionsPath.parent_path())==fs::weakly_canonical(cache.parent_path()),"axis decisions must stay in input directory");decisionsHash=sha(decisionsPath);require(decisionsHash==ascii(ini(config,L"axisDecisionsHash")),"axis decision hash mismatch");std::ifstream d(decisionsPath);require(static_cast<bool>(d),"axis decisions read");std::ostringstream text;text<<d.rdbuf();require(!d.bad(),"axis decisions read failed");decisionsText=text.str();std::istringstream values(decisionsText);bcp::readAxisExclusions(values,plan);}
  fs::path prefixesPath;std::string prefixesHash,prefixesText;if(GetPrivateProfileIntW(L"stage2",L"simplifyOpeningAnnotations",0,config.c_str())==1){prefixesPath=ini(config,L"openingPrefixes");require(fs::weakly_canonical(prefixesPath.parent_path())==fs::weakly_canonical(cache.parent_path()),"opening prefixes outside input directory");prefixesHash=sha(prefixesPath);require(prefixesHash==ascii(ini(config,L"openingPrefixesHash")),"opening prefix hash mismatch");std::ifstream values(prefixesPath);std::ostringstream text;text<<values.rdbuf();require(values&&!values.bad(),"opening prefix input read");prefixesText=text.str();std::istringstream parser(prefixesText);bcp::readOpeningPrefixes(parser,plan);}
  ScopedNativeMaskFrame maskEnvironment(GetPrivateProfileIntW(L"stage2",L"anchorCaptionsAndDirectThird",0,config.c_str())==1);AcDbDatabase db(false,true);status(db.readDwgFile(source.c_str(),AcDbDatabase::kForReadAndAllShare,false),"read frozen scheme");status(db.closeInput(true),"finish frozen read");const int checked=verify(db,plan);const auto sourceTypes=modelTypes(db);std::string endpointSupplement;const auto endpointCapture=captureGroupEndpoints(db,plan,endpointSupplement);const auto result=bcp::analyse(plan);const auto drawn=plan.rooms.empty()?DrawnAnnotations{}:drawAnnotations(db,plan,result,config);const auto added=static_cast<int>(drawn.axes.size()+drawn.dimensions.size()+drawn.guides.size()+drawn.captions.size()+drawn.witnessMasks.size()+drawn.apronStrokes.size()+drawn.apronTexts.size()+fullAxisRootCount(drawn));
  SYSTEMTIME now{};GetLocalTime(&now);std::wostringstream name;name<<L"native-"<<now.wYear<<L'-'<<now.wMonth<<L'-'<<now.wDay<<L'-'<<now.wHour<<L'-'<<now.wMinute<<L'-'<<now.wSecond<<L'-'<<GetCurrentProcessId()<<L'-'<<GetTickCount64();const auto output=outputRoot/name.str();require(fs::create_directory(output),"unique stage2 output directory");
  const auto work=output/(added?L"方案施工图_阶段2中间图.dwg":L"方案工作副本_未生成标注.dwg");status(db.saveAs(work.c_str(),false,AcDb::kDHL_CURRENT),"save stage2 foundation DWG");AcDbDatabase reopened(false,true);status(reopened.readDwgFile(work.c_str(),AcDbDatabase::kForReadAndAllShare,false),"reopen foundation DWG");status(reopened.closeInput(true),"finish output read");require(verify(reopened,plan,added)==checked+added,"output native geometry differs");const auto savedTypes=modelTypes(reopened);for(const auto& item:sourceTypes)require(savedTypes.count(item.first)&&savedTypes.at(item.first)==item.second,"original handle/type changed");verifyDrawnAnnotations(reopened,drawn);require(sha(source)==sourceHash&&sha(cache)==cacheHash,"input changed during analysis");
  if(!decisionsPath.empty()){require(sha(decisionsPath)==decisionsHash,"axis decisions changed during generation");writeNew(output/L"axis-exclusions.tsv",decisionsText);}
  if(!prefixesPath.empty()){require(sha(prefixesPath)==prefixesHash,"opening prefix input changed");writeNew(output/L"opening-prefixes.tsv",prefixesText);}
  writeNew(output/L"analysis.json",bcp::toJson(plan,result));writeNew(output/L"endpoint-capture.json",endpointCapture);writeNew(output/L"endpoint-supplement.tsv",endpointSupplement);if(added)writeNew(output/L"native-annotations.json",drawn.report);std::ostringstream evidence;evidence<<"{\"contract\":\"bcp.stage2.native-foundation.v1\",\"sourceSHA256\":\""<<sourceHash<<"\",\"cacheSHA256\":\""<<cacheHash<<"\",\"sourceObjectsVerified\":"<<checked<<",\"outputObjectsVerified\":"<<checked<<",\"outputDWGSHA256\":\""<<sha(work)<<"\",\"allModelHandleTypesRetained\":true,\"semanticGeometryObjectsVerified\":"<<(plan.walls.size()+plan.openings.size()+plan.rooms.size())<<",\"outputModelCount\":"<<(checked+added)<<",\"annotationsAdded\":"<<added<<",\"sourceDimensionsHidden\":"<<drawn.suppressed.size()<<",\"confirmedAxisExclusions\":"<<plan.axisExclusions.size()<<",\"axisDecisionsSHA256\":\""<<decisionsHash<<"\",\"savedReopened\":true,\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"constructionDrawingComplete\":false,\"error\":null}";writeNew(output/L"native-foundation.json",evidence.str());maskEnvironment.finish();writeNew(output/L"native-mask-frame-environment.json",std::string("{\"enabled\":")+(maskEnvironment.enabled?"true":"false")+",\"before\":"+std::to_string(maskEnvironment.before)+",\"frameBefore\":"+std::to_string(maskEnvironment.frameBefore)+",\"applied\":0,\"restored\":"+(maskEnvironment.restored?"true":"false")+"}");writeNew(receipt/L"stage2-analysis.done",evidence.str());acutPrintf(L"\nBCP_STAGE2_ANALYZE: %d objects verified; output %ls\n",checked,output.c_str());
 }catch(const std::exception& e){acutPrintf(L"\nBCP_STAGE2_ANALYZE failed: %hs\n",e.what());try{if(!receipt.empty())writeNew(receipt/L"stage2-analysis.error",std::string("failure: ")+e.what());}catch(...){}}catch(...){acutPrintf(L"\nBCP_STAGE2_ANALYZE unknown failure\n");}
}
}
extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode msg,void* pkt){try{if(msg==AcRx::kInitAppMsg){acrxUnlockApplication(pkt);acrxRegisterAppMDIAware(pkt);if(acedRegCmds->addCommand(L"BCP_STAGE2",L"BCP_STAGE2_MASKAUDIT",L"BCP_STAGE2_MASKAUDIT",ACRX_CMD_MODAL,maskAuditCommand)!=Acad::eOk)return AcRx::kRetError;if(acedRegCmds->addCommand(L"BCP_STAGE2",L"BCP_STAGE2_ANALYZE",L"BCP_STAGE2_ANALYZE",ACRX_CMD_MODAL,analyseCommand)!=Acad::eOk)return AcRx::kRetError;}else if(msg==AcRx::kUnloadAppMsg)acedRegCmds->removeGroup(L"BCP_STAGE2");return AcRx::kRetOK;}catch(...){return AcRx::kRetError;}}
