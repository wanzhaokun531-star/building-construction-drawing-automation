struct ProductInputInventory {
 bcpproduct::ModelCounts counts;int proxies{},generatedCarriers{},dimensions{};std::map<std::wstring,int> types;std::string geometry;
};
struct ProductDiagnosticContext {fs::path source,output,log;std::wstring stage=L"入口检查";std::string input="null";};
ProductDiagnosticContext productDiagnostic;
ProductInputInventory productInventory(AcDbDatabase& db){
 Working scope(&db);ProductInputInventory result;std::ostringstream geometry;geometry<<std::setprecision(17)<<'[';bool first=true;
 auto model=open<AcDbBlockTableRecord>(modelId(db));AcDbBlockTableRecordIterator* raw{};status(model->newIterator(raw),"input inventory iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);
 for(;!it->done();it->step()){
  AcDbObjectId id;status(it->getEntityId(id),"input inventory entity");auto e=open<AcDbEntity>(id);const std::wstring type=e->isA()->name();++result.counts.objects;++result.types[type];if(e->isAProxy())++result.proxies;
  if(type==L"TDbWall")++result.counts.walls;if(type==L"TDbOpening"||type==L"TDbGroupOpening")++result.counts.openings;if(type==L"TDbDimension2")++result.dimensions;
  if(auto* b=AcDbBlockReference::cast(e.get()))if(symbolName(b->blockTableRecord()).rfind(L"BCP_AXIS_",0)==0)++result.generatedCarriers;
  if(type!=L"TDbWall"&&type!=L"TDbOpening"&&type!=L"TDbGroupOpening")continue;
  if(!first)geometry<<',';first=false;geometry<<"{\"handle\":\""<<axisHandle(id)<<"\",\"type\":"<<jsonUtf8(type);
  if(auto* c=AcDbCurve::cast(e.get())){AcGePoint3d a,b;const auto sa=c->getStartPoint(a),sb=c->getEndPoint(b);geometry<<",\"curveStatus\":["<<sa<<','<<sb<<']';if(sa==Acad::eOk&&sb==Acad::eOk)geometry<<",\"ends\":"<<wholePoints({a,b});}
  AcGePoint3dArray gp;AcDbIntArray modes,ids;const auto gs=e->getGripPoints(gp,modes,ids);geometry<<",\"gripStatus\":"<<gs<<",\"grips\":[";if(gs==Acad::eOk)for(int i=0;i<gp.length();++i){if(i)geometry<<',';geometry<<'['<<gp[i].x<<','<<gp[i].y<<','<<gp[i].z<<']';}geometry<<"]}";
 }
 geometry<<']';result.geometry=geometry.str();return result;
}
std::string productInventoryJson(const ProductInputInventory& input){std::ostringstream out;out<<"{\"modelObjects\":"<<input.counts.objects<<",\"nativeWalls\":"<<input.counts.walls<<",\"nativeOpenings\":"<<input.counts.openings<<",\"proxies\":"<<input.proxies<<",\"existingNativeDimensions\":"<<input.dimensions<<",\"generatedAxisCarriers\":"<<input.generatedCarriers<<",\"types\":{";bool first=true;for(const auto& t:input.types){if(!first)out<<',';first=false;out<<jsonUtf8(t.first)<<':'<<t.second;}out<<"},\"nativeGeometry\":"<<input.geometry<<'}';return out.str();}
fs::path productDiagnosticDirectory(){
 if(productValidation())return fs::path(ini(iniPath(),L"receiptDirectory"))/L"diagnostics";
 wchar_t path[32768]{};const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);require(n>0&&n<32768,"diagnostic directory unavailable");return fs::path(path)/L"BuildingPlan2020/Logs";
}
fs::path productWriteDiagnostic(const std::string& message,bool success=false){
 const auto directory=productDiagnosticDirectory();fs::create_directories(directory);static unsigned serial{};const auto file=directory/(L"BCP_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetTickCount64())+L"_"+std::to_wstring(++serial)+L".json");
 std::ostringstream out;out<<"{\"contract\":\"bcp.input-diagnostic.v1\",\"version\":\"0.1.4\",\"stage\":"<<jsonUtf8(productDiagnostic.stage)<<",\"source\":"<<jsonUtf8(productDiagnostic.source.wstring())<<",\"output\":"<<jsonUtf8(productDiagnostic.output.wstring())<<",\"message\":"<<jsonUtf8(bcpproduct::decodeMessage(message))<<",\"input\":"<<productDiagnostic.input<<",\"success\":"<<(success?"true":"false")<<",\"drawingWritten\":false}";writeNew(file,out.str());productDiagnostic.log=file;return file;
}
fs::path productCheckDocument(){
 auto* doc=curDoc();require(doc!=nullptr,u8"E_NO_DOCUMENT：请先打开定稿方案图。");
 if(doc->fileName())productDiagnostic.source=doc->fileName();
 require(doc->isNamedDrawing(),u8"E_UNSAVED_DOCUMENT：当前图纸尚未命名保存，请先保存为DWG。");
 require(doc->fileName()&&*doc->fileName(),u8"E_INPUT_PATH：CAD未提供当前图纸的文件路径。");
 require(fs::is_regular_file(productDiagnostic.source),u8"E_INPUT_PATH：未找到当前图纸的磁盘文件，请先将DWG解压/保存到本地文件夹。");
 resbuf modified{};require(acedGetVar(L"DBMOD",&modified)==RTNORM&&modified.restype==RTSHORT,u8"E_DOCUMENT_STATE：无法读取图纸修改状态DBMOD。");
 if(modified.resval.rint!=0)throw std::runtime_error(u8"E_DOCUMENT_MODIFIED：当前图纸DBMOD="+std::to_string(modified.resval.rint)+u8"。请先保存并确认定稿后再生成。");return productDiagnostic.source;
}
#include "NativeInputProperties.inc"
std::string approvedInteractionProof(AcDbDatabase&);
void productInspectFile(const fs::path& source,const fs::path& receipt={}){
 productDiagnostic.source=source;productDiagnostic.stage=L"输入结构读取";productDependencies();AcDbDatabase db(false,true);status(db.readDwgFile(source.c_str(),AcDbDatabase::kForReadAndAllShare,false),"read inspection source");status(db.closeInput(true),"close inspection source");const auto inventory=productInventory(db);productDiagnostic.input=productInventoryJson(inventory);if((productSetting(L"testBehavior")==L"inspect-list"||productSetting(L"testBehavior")==L"inspect-edit")){const auto folder=receipt/L"public-properties";const auto fields=productReadProperties(db,folder);const auto plan=productCurrentPlan(db,source,fields);const auto cache=productPlanCache(plan);std::istringstream check(cache);const auto parsed=bcp::readCache(check);require(parsed.walls.size()==plan.walls.size()&&parsed.openings.size()==plan.openings.size(),"current input cache roundtrip differs");writeNew(receipt/L"current-input.tsv",cache);}
 if(productValidation()&&productSetting(L"testBehavior")==L"inspect-edit"){require(curDoc()&&fs::weakly_canonical(curDoc()->fileName())==fs::weakly_canonical(source),"native interaction probe requires owned active source copy");writeNew(receipt/L"approved-edit-proof.json",approvedInteractionProof(*curDoc()->database()));}
 const auto log=productWriteDiagnostic(u8"输入结构已读取；尚未进行施工图生成。",true);
 acutPrintf(L"\n输入读取完成：模型对象 %d，天正墙体 %d，天正门窗 %d，代理 %d。\n报告：%ls\n",inventory.counts.objects,inventory.counts.walls,inventory.counts.openings,inventory.proxies,log.c_str());
 if(!receipt.empty())writeNew(receipt/L"product-result.json",std::string("{\"contract\":\"bcp.input-inspection.v1\",\"completed\":true,\"drawingWritten\":false,\"sourceSHA256\":\"")+sha(source)+"\",\"input\":"+productDiagnostic.input+",\"report\":"+jsonUtf8(log.wstring())+",\"error\":null}");
}

void productExtraFixture(const fs::path& source,const fs::path& receipt){require(productValidation()&&sha(source)=="5BC4122C45AE0B6E49551C9AF0DED6F67FBC4339E96CCA7509C529DD3EE1D35E","extra-root fixture requires frozen original source");productDependencies();AcDbDatabase db(false,true);status(db.readDwgFile(source.c_str(),AcDbDatabase::kForReadAndAllShare,false),"read extra-root fixture");status(db.closeInput(true),"close extra-root fixture");Working work(&db);const auto before=wholeProtected(db,{});auto line=std::make_unique<AcDbLine>(AcGePoint3d(1000,1000,0),AcGePoint3d(2000,1000,0));line->setDatabaseDefaults(&db);AcDbObjectId id;{auto model=editObject<AcDbBlockTableRecord>(modelId(db));status(model->appendAcDbEntity(id,line.get()),"append owned regression line");}line.release()->close();require(wholeProtected(db,{axisHandle(id)})==before,"fixture altered original objects");const auto output=receipt/L"extra-input.dwg";require(!fs::exists(output),"extra input already exists");status(db.saveAs(output.c_str(),false,AcDb::kDHL_CURRENT),"save extra-root fixture");writeNew(receipt/L"product-result.json",std::string("{\"completed\":true,\"drawingWritten\":true,\"sourceSHA256\":\"")+sha(source)+"\",\"fixture\":"+jsonUtf8(output.wstring())+",\"fixtureSHA256\":\""+sha(output)+"\",\"addedLine\":\""+axisHandle(id)+"\",\"error\":null}");}
