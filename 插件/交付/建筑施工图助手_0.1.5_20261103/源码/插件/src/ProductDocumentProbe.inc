// Isolated validation only. Makes real changes through public AutoCAD APIs;
// does not assign/reset DBMOD, save the input, or bypass productCheckDocument.
void productDocumentProbe(const fs::path& source,const fs::path& receipt){
 const auto behavior=productSetting(L"testBehavior");
 if(behavior!=L"document-state-20"&&behavior!=L"document-state-entity")return;
 require(productValidation()&&curDoc()&&fs::weakly_canonical(source)==fs::weakly_canonical(receipt/L"control.dwg")&&fs::weakly_canonical(curDoc()->fileName())==fs::weakly_canonical(source),"document probe requires own isolated control copy");
 auto* shortCommand=acedRegCmds->lookupGlobalCmd(L"BCP");auto* oldCommand=acedRegCmds->lookupGlobalCmd(L"BCP_BUILD");
 require(shortCommand&&oldCommand&&shortCommand->functionAddr()==oldCommand->functionAddr(),"BCP alias differs from generation command");
 resbuf before{},after{};require(acedGetVar(L"DBMOD",&before)==RTNORM&&before.restype==RTSHORT&&before.resval.rint==0,"document probe must start clean");
 const auto diskHash=sha(source);
 if(behavior==L"document-state-20"){
  resbuf precision{};require(acedGetVar(L"LUPREC",&precision)==RTNORM&&precision.restype==RTSHORT,"read fixture precision");precision.resval.rint=precision.resval.rint==3?4:3;
  require(acedSetVar(L"LUPREC",&precision)==RTNORM,"change fixture precision");
  require(acedCommandS(RTSTR,L"_.ZOOM",RTSTR,L"_S",RTSTR,L"0.99x",RTNONE)==RTNORM,"change fixture view");
 }else{
  auto line=std::make_unique<AcDbLine>(AcGePoint3d(0,0,0),AcGePoint3d(100,0,0));line->setDatabaseDefaults(curDoc()->database());AcDbObjectId id;
  {auto model=editObject<AcDbBlockTableRecord>(modelId(*curDoc()->database()));status(model->appendAcDbEntity(id,line.get()),"append unsaved fixture entity");}line.release()->close();
 }
 require(acedGetVar(L"DBMOD",&after)==RTNORM&&after.restype==RTSHORT,"read actual fixture DBMOD");
 require(behavior==L"document-state-20"?after.resval.rint==20:(after.resval.rint&1)!=0,"fixture did not produce expected DBMOD");require(sha(source)==diskHash,"fixture changed saved source");
 std::ostringstream out;out<<"{\"before\":0,\"after\":"<<after.resval.rint<<",\"aliasSameFunction\":true,\"savedFileUnchanged\":true,\"sourceSHA256\":\""<<diskHash<<"\"}";writeNew(receipt/L"document-state-probe.json",out.str());
}
