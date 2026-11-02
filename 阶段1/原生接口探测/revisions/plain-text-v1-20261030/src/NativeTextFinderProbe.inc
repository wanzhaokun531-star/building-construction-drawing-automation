// Public 2020 SDK acdbTextFind, explicitly restricted to one owned clone.
void nativeTextFinderImpl(){
 const auto p=paths();quietRequireIsolatedWorker();PureCommandTrace trace;
 std::string error="null",originalBefore="[]",originalAfter="[]",originalSaved="[]",before="null",after="null",saved="null",restored="null";
 bool controlFound=false,controlReplaced=false,found=false,replaced=false,reverse=false,passed=false;
 int save=-9999,restoreSave=-9999,savedCount=-1;std::wstring targetHandle,savedType;
 try{
  auto db=lifecycleRead(p.scheme);WorkingDatabaseScope work(*db);const auto originals=modelIds(*db);if(originals.length()!=213)throw std::runtime_error("text-finder source count differs");originalBefore=businessGeometryJson(originals);
  const auto source=nsrFind(*db,L"CDC0");const auto sourceText=symbolPublicReadback(source);if(!sourceText.completed||!sourceText.ownershipSafe||sourceText.texts.size()!=1||sourceText.texts[0].empty())throw std::runtime_error("finder needs one verified source string");
  AcDbObjectIdArray toClone;toClone.append(source);AcDbIdMapping map;check(db->deepCloneObjects(toClone,modelId(*db),map),"finder clone source text");AcDbIdPair pair(source,AcDbObjectId::kNull,false);if(!map.compute(pair)||!pair.isCloned()||pair.value()==source||pair.value().isNull())throw std::runtime_error("finder clone mapping differs");const auto target=pair.value();targetHandle=handleOf(target);before=symbolPublicReadback(target).json;
  AcDbObjectId controlId;
  {auto model=open<AcDbBlockTableRecord>(modelId(*db),AcDb::kForWrite);auto control=std::make_unique<AcDbText>();control->setDatabaseDefaults(db.get());check(control->setTextString(L"S1_NATIVE_FIND_CONTROL"),"finder control text");check(model->appendAcDbEntity(controlId,control.get()),"finder append control");auto* resident=control.release();check(resident->close(),"finder close control");}
  const Adesk::UInt8 flags=AC_SRCH_DEFAULT|AC_SRCH_MATCH_CASE;AcDbObjectIdArray controlSelection,hits;controlSelection.append(controlId);
  controlFound=acdbTextFind(db.get(),hits,L"S1_NATIVE_FIND_CONTROL",nullptr,flags,controlSelection)&&hits.length()==1&&hits[0]==controlId;
  hits.setLogicalLength(0);controlReplaced=acdbTextFind(db.get(),hits,L"S1_NATIVE_FIND_CONTROL",L"S1_NATIVE_FIND_OK",flags,controlSelection);
  {auto control=open<AcDbText>(controlId);AcString value;check(control->textString(value),"finder control read");controlReplaced=controlReplaced&&std::wstring(value.kACharPtr())==L"S1_NATIVE_FIND_OK";}
  {auto control=open<AcDbText>(controlId,AcDb::kForWrite);check(control->erase(),"finder erase diagnostic standard control");}
  if(!controlFound||!controlReplaced)throw std::runtime_error("native text-finder standard positive control failed");
  AcDbObjectIdArray selection;selection.append(target);hits.setLogicalLength(0);
  found=acdbTextFind(db.get(),hits,sourceText.texts[0].c_str(),nullptr,flags,selection)&&hits.length()==1&&hits[0]==target;
  if(found){hits.setLogicalLength(0);replaced=acdbTextFind(db.get(),hits,sourceText.texts[0].c_str(),L"C09235",flags,selection);}
  const auto changed=symbolPublicReadback(target);after=changed.json;
  {auto root=open<AcDbEntity>(target);passed=found&&replaced&&std::wstring(root->isA()->name())==L"TDbText"&&changed.completed&&changed.ownershipSafe&&changed.texts==std::vector<std::wstring>{L"C09235"};}
  originalAfter=businessGeometryJson(originals);if(originalAfter!=originalBefore||symbolPublicReadback(source).texts!=sourceText.texts)throw std::runtime_error("finder altered protected originals");
  if(passed){
   const auto path=p.output+L"\\native-finder-forward.dwg";requireNewFile(path);save=db->saveAs(path.c_str(),false,AcDb::kDHL_CURRENT);check(static_cast<Acad::ErrorStatus>(save),"finder forward save");
   {auto reader=lifecycleRead(path);WorkingDatabaseScope fresh(*reader);const auto id=nsrFind(*reader,targetHandle.c_str());{auto root=open<AcDbEntity>(id);savedType=root->isA()->name();}savedCount=modelCount(*reader);originalSaved=businessGeometryJson(nsrResolve(*reader,nsrHandles(originals)));const auto value=symbolPublicReadback(id);saved=value.json;passed=value.completed&&value.ownershipSafe&&value.texts==changed.texts&&savedType==L"TDbText"&&savedCount==214&&originalSaved==originalBefore;}
   hits.setLogicalLength(0);reverse=acdbTextFind(db.get(),hits,L"C09235",sourceText.texts[0].c_str(),flags,selection);const auto reverseValue=symbolPublicReadback(target);restored=reverseValue.json;passed=passed&&reverse&&reverseValue.completed&&reverseValue.ownershipSafe&&reverseValue.texts==sourceText.texts;
   const auto restorePath=p.output+L"\\native-finder-restored.dwg";requireNewFile(restorePath);restoreSave=db->saveAs(restorePath.c_str(),false,AcDb::kDHL_CURRENT);check(static_cast<Acad::ErrorStatus>(restoreSave),"finder restored save");
   {auto reader=lifecycleRead(restorePath);WorkingDatabaseScope fresh(*reader);const auto id=nsrFind(*reader,targetHandle.c_str());auto root=open<AcDbEntity>(id);const bool typeSame=std::wstring(root->isA()->name())==L"TDbText";root.reset();const auto value=symbolPublicReadback(id);passed=passed&&typeSame&&modelCount(*reader)==214&&businessGeometryJson(nsrResolve(*reader,nsrHandles(originals)))==originalBefore&&value.completed&&value.ownershipSafe&&value.texts==sourceText.texts;}
  }
 }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown native text-finder failure");}
 writeNewReport(p.output+L"\\native-text-finder.json","{\"contract\":\"stage1.native-text-finder.v1\",\"api\":\"AutoCAD2020 acdbTextFind; no FIND command\",\"originalBefore\":"+originalBefore+",\"originalAfter\":"+originalAfter+",\"originalSaved\":"+originalSaved+",\"targetHandle\":"+quote(targetHandle)+",\"standardControlFound\":"+plotBool(controlFound)+",\"standardControlReplaced\":"+plotBool(controlReplaced)+",\"targetFound\":"+plotBool(found)+",\"targetReplaced\":"+plotBool(replaced)+",\"reverseSucceeded\":"+plotBool(reverse)+",\"before\":"+before+",\"after\":"+after+",\"saved\":"+saved+",\"restored\":"+restored+",\"saveStatus\":"+std::to_string(save)+",\"restoreSaveStatus\":"+std::to_string(restoreSave)+",\"savedRootType\":"+quote(savedType)+",\"savedModelCount\":"+std::to_string(savedCount)+",\"businessContentWritePassed\":"+plotBool(passed&&error=="null")+",\"commandTrace\":"+trace.json()+",\"sourceDrawingWritten\":false,\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"error\":"+error+"}");
}
void nativeTextFinder()noexcept{pureEntry(nativeTextFinderImpl);}
