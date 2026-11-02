// User-authorized independent text type migration, exclusively public SDK.
std::string plainTextInfo(const AcDbText& t){
 AcString v;check(t.textString(v),"plain replacement text getter");
 return "{\"type\":"+quote(t.isA()->name())+",\"text\":"+quote(v.kACharPtr())+",\"position\":"+point(t.position())+",\"alignmentPoint\":"+point(t.alignmentPoint())+",\"height\":"+number(t.height())+",\"rotation\":"+number(t.rotation())+",\"normal\":"+axisExplodeVector(t.normal())+",\"widthFactor\":"+number(t.widthFactor())+",\"oblique\":"+number(t.oblique())+",\"thickness\":"+number(t.thickness())+",\"horizontalMode\":"+std::to_string(t.horizontalMode())+",\"verticalMode\":"+std::to_string(t.verticalMode())+",\"mirroredX\":"+plotBool(t.isMirroredInX()!=0)+",\"mirroredY\":"+plotBool(t.isMirroredInY()!=0)+",\"styleHandle\":"+quote(handleOf(t.textStyle()))+",\"layerHandle\":"+quote(handleOf(t.layerId()))+",\"color\":"+styleAuditColor(t.color())+",\"lineWeight\":"+std::to_string(t.lineWeight())+",\"visibility\":"+std::to_string(t.visibility())+"}";
}
std::string replaceOneIndependentTchText(AcDbObjectId id){
 std::unique_ptr<AcDbText> replacement;std::string glyph,expected,result;int status=-9999;
 {
  auto root=open<AcDbEntity>(id);if(root->isAProxy()||std::wstring(root->isA()->name())!=L"TDbText")throw std::runtime_error("replacement refuses non-independent TDbText");
  AxisExplodePool pool;AxisExplodeReturns parts(pool,root.get());check(root->explode(parts.values),"independent TCH public visible text extraction");if(parts.values.length()!=1)throw std::runtime_error("replacement refuses multiple or nontext components");
  auto* leaf=AcDbText::cast(static_cast<AcRxObject*>(parts.values[0]));if(!leaf||leaf==root.get()||!axisExplodeTransient(*leaf)||std::wstring(leaf->isA()->name())!=L"AcDbText")throw std::runtime_error("replacement needs one owned transient native Text leaf");
  glyph=plainTextInfo(*leaf);auto* copy=AcDbText::cast(leaf->clone());if(!copy)throw std::runtime_error("native visible text clone failed");replacement.reset(copy);if(!axisExplodeTransient(*replacement))throw std::runtime_error("replacement clone unexpectedly resident");
  if(replacement->textStyle().isNull()||replacement->textStyle().database()!=id.database())throw std::runtime_error("replacement font style belongs to another database");
  check(replacement->setPropertiesFrom(root.get(),false),"inherit standalone root graphical properties");expected=plainTextInfo(*replacement);
 }
 {
  auto old=open<AcDbEntity>(id,AcDb::kForWrite);status=old->handOverTo(replacement.get(),true,true);
  if(status==Acad::eObjectToBeDeleted){
   // 2020 SDK samples/entity/polysamp/command.cpp:958-965 ownership protocol.
   auto* detached=old.release();auto* resident=replacement.release();delete detached;check(resident->close(),"close installed plain-text replacement");
  }else{
   if(!replacement->objectId().isNull()||replacement->database()!=nullptr){auto* resident=replacement.release();resident->close();}
   throw std::runtime_error("native handOverTo rejected documented ownership status: "+std::to_string(status));
  }
 }
 {auto actual=open<AcDbText>(id);if(actual->isAProxy()||actual->ownerId()!=modelId(*id.database()))throw std::runtime_error("replacement root ownership differs");result=plainTextInfo(*actual);}
 if(expected!=result)throw std::runtime_error("replacement changed cloned native typography");
 return "{\"handle\":"+quote(handleOf(id))+",\"sourceType\":\"TDbText\",\"sourceVisibleGlyph\":"+glyph+",\"expectedNative\":"+expected+",\"actualNative\":"+result+",\"handOverStatus\":"+std::to_string(status)+",\"handleRetained\":true,\"keepXData\":true,\"keepExtensionDictionary\":true}";
}
void nativePlainTextReplacementImpl(){
 const auto p=paths();quietRequireIsolatedWorker();PureCommandTrace trace;std::string error="null",before="[]",after="[]",conversion="null",baseline="null",batch="null";std::vector<std::string> states;bool passed=false;
 try{
  auto db=lifecycleRead(p.scheme);WorkingDatabaseScope work(*db);const auto originals=modelIds(*db);if(originals.length()!=213)throw std::runtime_error("plain replacement requires scheme213");const auto target=nsrFind(*db,L"CDC0");AcDbObjectIdArray protectedIds;for(int i=0;i<originals.length();++i)if(originals[i]!=target)protectedIds.append(originals[i]);const auto handles=nsrHandles(protectedIds);before=businessGeometryJson(protectedIds);
  conversion=replaceOneIndependentTchText(target);std::wstring old;{auto root=open<AcDbText>(target);AcString value;check(root->textString(value),"plain replacement baseline");old=value.kACharPtr();baseline=plainTextInfo(*root);}
  int index=0;for(const auto& value:std::vector<std::wstring>{old,L"C09235",L"待确认",old}){
   std::string live;{auto root=open<AcDbText>(target,AcDb::kForWrite);check(root->setTextString(value.c_str()),"plain text native content setter");live=plainTextInfo(*root);}after=businessGeometryJson(protectedIds);if(before!=after||modelCount(*db)!=213)throw std::runtime_error("plain replacement altered protected212 objects");
   const auto stem=p.output+L"\\native-plain-text-state-"+std::to_wstring(index++);requireNewFile(stem+L".dwg");const auto save=db->saveAs((stem+L".dwg").c_str(),false,AcDb::kDHL_CURRENT);check(save,"save plain replacement state");
   std::string saved,savedOthers;int count{};{auto reader=lifecycleRead(stem+L".dwg");WorkingDatabaseScope fresh(*reader);auto text=open<AcDbText>(nsrFind(*reader,L"CDC0"));saved=plainTextInfo(*text);count=modelCount(*reader);savedOthers=businessGeometryJson(nsrResolve(*reader,handles));}
   const bool ok=live==saved&&count==213&&savedOthers==before;states.push_back("{\"expected\":"+quote(value)+",\"live\":"+live+",\"saved\":"+saved+",\"saveStatus\":"+std::to_string(save)+",\"savedModelCount\":"+std::to_string(count)+",\"savedProtected\":"+savedOthers+",\"passed\":"+plotBool(ok)+"}");if(!ok)throw std::runtime_error("plain replacement saved typography or protected originals differs");
  }
  passed=states.size()==4;
  {auto all=lifecycleRead(p.scheme);WorkingDatabaseScope batchWork(*all);AcDbObjectIdArray textIds,otherIds;const auto allIds=modelIds(*all);for(int i=0;i<allIds.length();++i){auto root=open<AcDbEntity>(allIds[i]);if(std::wstring(root->isA()->name())==L"TDbText")textIds.append(allIds[i]);else otherIds.append(allIds[i]);}const auto nonTextBefore=businessGeometryJson(otherIds);const auto nonTextHandles=nsrHandles(otherIds);std::vector<std::string> conversions,expected;std::vector<std::wstring> textHandles;
   for(int i=0;i<textIds.length();++i){textHandles.push_back(handleOf(textIds[i]));conversions.push_back(replaceOneIndependentTchText(textIds[i]));auto text=open<AcDbText>(textIds[i]);expected.push_back(plainTextInfo(*text));}
   const auto file=p.output+L"\\方案副本_独立文字转原生_内容保持.dwg";requireNewFile(file);check(all->saveAs(file.c_str(),false,AcDb::kDHL_CURRENT),"save batch standalone text replacements");std::vector<std::string> actual;bool verified=false;int count=-1;
   {auto reader=lifecycleRead(file);WorkingDatabaseScope fresh(*reader);for(const auto& h:textHandles){auto text=open<AcDbText>(nsrFind(*reader,h.c_str()));actual.push_back(plainTextInfo(*text));}count=modelCount(*reader);verified=actual==expected&&count==213&&businessGeometryJson(nsrResolve(*reader,nonTextHandles))==nonTextBefore;}
   batch="{\"convertedCount\":"+std::to_string(textIds.length())+",\"nonTextBefore\":"+nonTextBefore+",\"nonTextAfter\":"+businessGeometryJson(otherIds)+",\"conversions\":"+axisTraceArray(conversions)+",\"expected\":"+axisTraceArray(expected)+",\"saved\":"+axisTraceArray(actual)+",\"savedModelCount\":"+std::to_string(count)+",\"savedVerified\":"+plotBool(verified)+"}";if(!verified)throw std::runtime_error("batch plain-text saved readback failed");
  }
 }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown plain-text replacement failure");}
 writeNewReport(p.output+L"\\native-plain-text-replacement.json","{\"contract\":\"stage1.native-plain-text-replacement.v1\",\"authorization\":\"user allows independent TCH text replacement with native text\",\"conversion\":"+conversion+",\"baseline\":"+baseline+",\"protectedBefore\":"+before+",\"protectedAfter\":"+after+",\"states\":"+axisTraceArray(states)+",\"singleReplacementPassed\":"+plotBool(passed)+",\"batch\":"+batch+",\"commandTrace\":"+trace.json()+",\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"sourceDrawingWritten\":false,\"phase2Started\":false,\"error\":"+error+"}");
}
void nativePlainTextReplacement()noexcept{pureEntry(nativePlainTextReplacementImpl);}
