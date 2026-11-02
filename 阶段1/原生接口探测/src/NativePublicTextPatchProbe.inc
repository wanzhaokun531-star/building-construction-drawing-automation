// A public entity-data interface trial, confined to an owned clone. No private filer.
std::string nativePublicTextSet(AcDbObjectId id,const std::wstring& expected,const std::wstring& replacement,const std::wstring& attemptPath,bool tch){
 {auto root=open<AcDbEntity>(id);const std::wstring name=root->isA()->name();if(name!=(tch?L"TDbText":L"AcDbText")||root->isAProxy())throw std::runtime_error("public patch actual native root differs");}
 ads_name name{};check(acdbGetAdsName(name,id),"public patch native entity name");
 std::unique_ptr<resbuf,ReleaseAxisDxfBuffer> bag(acdbEntGet(name));if(!bag)throw std::runtime_error("public patch missing public bag");
 std::vector<stage1::PublicTextToken> tokens;std::vector<resbuf*> nodes;std::size_t ordinarySlot=std::numeric_limits<std::size_t>::max();int ones=0;
 for(auto* item=bag.get();item;item=item->rbnext){
  if(nodes.size()>=1000000)throw std::runtime_error("public patch bag too large");nodes.push_back(item);
  const bool textCode=item->restype==0||item->restype==100||item->restype==1;
  tokens.push_back({item->restype,textCode&&item->resval.rstring?item->resval.rstring:L"",textCode&&item->resval.rstring!=nullptr});
  if(item->restype==1){++ones;if(item->resval.rstring&&expected==item->resval.rstring)ordinarySlot=nodes.size()-1;}
 }
 const auto slot=tch?stage1::publicTextSlot(tokens,expected):(ones==1?ordinarySlot:std::numeric_limits<std::size_t>::max());
 if(slot>=nodes.size())throw std::runtime_error("public text field is absent, ambiguous, mismatched or outside TDbText");
 const auto before=publicAxisDxf(id);check(acutUpdString(replacement.c_str(),nodes[slot]->resval.rstring),"allocate SDK-owned replacement string");
 writeNewReport(attemptPath,"{\"target\":"+quote(handleOf(id))+",\"beforePublicBag\":"+before+",\"verifiedSlot\":"+std::to_string(slot)+",\"old\":"+quote(expected)+",\"new\":"+quote(replacement)+",\"onlyWrittenField\":\"verified public text group 1\",\"apiAboutToRun\":\"acdbEntMod\"}");
 const int status=acdbEntMod(bag.get());const int update=status==RTNORM?acdbEntUpd(name):-9999;
 return "{\"entModStatus\":"+std::to_string(status)+",\"entUpdStatus\":"+std::to_string(update)+",\"beforePublicBag\":"+before+",\"afterPublicBag\":"+publicAxisDxf(id)+"}";
}
void nativePublicTextPatchImpl(){
 const auto p=paths();quietRequireIsolatedWorker();PureCommandTrace trace;
 std::string error="null",originalBefore="[]",originalAfter="[]",control="null",sourceTextJson="null";std::vector<std::string> states;std::wstring targetHandle;bool passed=false;
 try{
  auto* doc=acDocManager?acDocManager->curDocument():nullptr;auto* db=doc?doc->database():nullptr;fourAxisGuard(p,doc,db);
  const auto originals=modelIds(*db);if(originals.length()!=213)throw std::runtime_error("public text patch requires frozen scheme213");const auto originalHandles=nsrHandles(originals);originalBefore=businessGeometryJson(originals);
  const auto source=nsrFind(*db,L"CDC0");const auto sourceText=symbolPublicReadback(source);sourceTextJson=sourceText.json;
  if(!sourceText.completed||!sourceText.ownershipSafe||sourceText.texts.size()!=1||sourceText.texts[0].empty())throw std::runtime_error("public patch needs exactly one independent visible source string");
  AcDbObjectIdArray ids;ids.append(source);AcDbIdMapping map;check(db->deepCloneObjects(ids,modelId(*db),map),"public patch clone source text");AcDbIdPair pair(source,AcDbObjectId::kNull,false);if(!map.compute(pair)||!pair.isCloned()||pair.value().isNull()||pair.value()==source)throw std::runtime_error("public patch clone mapping invalid");const auto target=pair.value();targetHandle=handleOf(target);
  {auto root=open<AcDbEntity>(target,AcDb::kForWrite);check(root->transformBy(AcGeMatrix3d::translation(AcGeVector3d(40000,0,0))),"move owned diagnostic clone away from original room name");}
  AcDbObjectId standard;
  {auto model=open<AcDbBlockTableRecord>(modelId(*db),AcDb::kForWrite);auto text=std::make_unique<AcDbText>();text->setDatabaseDefaults(db);check(text->setTextString(L"S1_PUBLIC_PATCH_CONTROL"),"public patch positive control value");check(model->appendAcDbEntity(standard,text.get()),"public patch positive control append");auto* resident=text.release();check(resident->close(),"public patch positive control close");}
  control=nativePublicTextSet(standard,L"S1_PUBLIC_PATCH_CONTROL",L"S1_PUBLIC_PATCH_OK",p.output+L"\\public-text-control-attempt.json",false);
  {auto text=open<AcDbText>(standard,AcDb::kForWrite);AcString value;check(text->textString(value),"public patch positive control read");if(std::wstring(value.kACharPtr())!=L"S1_PUBLIC_PATCH_OK")throw std::runtime_error("public EntMod standard positive control did not change");check(text->erase(),"erase public patch standard diagnostic");}
  std::wstring expected=sourceText.texts[0];const std::vector<std::wstring> replacements{L"C09235",L"待确认",sourceText.texts[0]};int index=0;
  for(const auto& next:replacements){
   const auto stem=p.output+L"\\public-text-state-"+std::to_wstring(++index);const auto before=symbolPublicReadback(target);
   if(!before.completed||!before.ownershipSafe||before.texts!=std::vector<std::wstring>{expected})throw std::runtime_error("public patch independent before text differs");
   const auto api=nativePublicTextSet(target,expected,next,stem+L"-attempt.json",true);const auto live=symbolPublicReadback(target);
   originalAfter=businessGeometryJson(originals);std::wstring liveType;{auto root=open<AcDbEntity>(target);liveType=root->isA()->name();}
   const bool changed=live.completed&&live.ownershipSafe&&live.texts==std::vector<std::wstring>{next}&&liveType==L"TDbText"&&modelCount(*db)==214&&originalAfter==originalBefore&&symbolPublicReadback(source).texts==sourceText.texts;
   std::string saved="null",savedOriginal="[]",savedSource="null",savedBag="null";std::wstring savedType;int count=-1,save=-9999;bool reopened=false;
   if(changed){
    requireNewFile(stem+L".dwg");save=db->saveAs((stem+L".dwg").c_str(),false,AcDb::kDHL_CURRENT);check(static_cast<Acad::ErrorStatus>(save),"save public text state");
    auto reader=lifecycleRead(stem+L".dwg");WorkingDatabaseScope fresh(*reader);const auto freshId=nsrFind(*reader,targetHandle.c_str());{auto root=open<AcDbEntity>(freshId);savedType=root->isA()->name();}count=modelCount(*reader);savedOriginal=businessGeometryJson(nsrResolve(*reader,originalHandles));const auto read=symbolPublicReadback(freshId);saved=read.json;const auto readSource=symbolPublicReadback(nsrFind(*reader,L"CDC0"));savedSource=readSource.json;savedBag=publicAxisDxf(freshId);
    reopened=read.completed&&read.ownershipSafe&&read.texts==std::vector<std::wstring>{next}&&savedType==L"TDbText"&&count==214&&savedOriginal==originalBefore&&readSource.completed&&readSource.ownershipSafe&&readSource.texts==sourceText.texts;
   }
   states.push_back("{\"expected\":"+quote(next)+",\"api\":"+api+",\"before\":"+before.json+",\"live\":"+live.json+",\"liveRootType\":"+quote(liveType)+",\"liveModelCount\":"+std::to_string(modelCount(*db))+",\"changed\":"+plotBool(changed)+",\"saveStatus\":"+std::to_string(save)+",\"saved\":"+saved+",\"savedRootType\":"+quote(savedType)+",\"savedModelCount\":"+std::to_string(count)+",\"savedOriginal\":"+savedOriginal+",\"savedSource\":"+savedSource+",\"savedPublicBag\":"+savedBag+",\"reopened\":"+plotBool(reopened)+"}");
   if(!changed||!reopened)throw std::runtime_error("public EntMod TDbText actual content or saved readback failed");expected=next;
  }
  passed=states.size()==3;
 }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown public text patch failure");}
 writeNewReport(p.output+L"\\native-public-text-patch.json","{\"contract\":\"stage1.native-public-text-patch.v1\",\"originalBefore\":"+originalBefore+",\"originalAfter\":"+originalAfter+",\"sourceText\":"+sourceTextJson+",\"targetHandle\":"+quote(targetHandle)+",\"standardControl\":"+control+",\"states\":"+axisTraceArray(states)+",\"businessContentWritePassed\":"+plotBool(passed&&error=="null")+",\"commandTrace\":"+trace.json()+",\"sourceDrawingWritten\":false,\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"error\":"+error+"}");
}
void nativePublicTextPatch()noexcept{pureEntry(nativePublicTextPatchImpl);}
