std::string publicNativeDatabaseInventory(AcDbDatabase& db){
 wchar_t endHandle[32]{};if(!db.handseed().getIntoAsciiBuffer(endHandle,32))throw std::runtime_error("native database handseed unavailable");const auto end=std::stoull(endHandle,nullptr,16);if(end>250000)throw std::runtime_error("native handle inventory exceeds explicit250000 bound");
 std::map<std::wstring,int> types;std::vector<std::string> proxies,failures;int found=0,opened=0,erased=0;
 for(Adesk::UInt64 n=1;n<end;++n){AcDbObjectId id;const auto resolved=db.getAcDbObjectId(id,false,AcDbHandle(n));if(resolved!=Acad::eOk||id.isNull())continue;++found;if(id.isErased()){++erased;continue;}AcDbObject* raw{};const auto status=acdbOpenObject(raw,id,AcDb::kForRead);if(status!=Acad::eOk||!raw){failures.push_back("{\"handle\":"+quote(handleOf(id))+",\"status\":"+std::to_string(status)+"}");continue;}
  Opened<AcDbObject> object(raw);++opened;++types[object->isA()->name()];
  const ACHAR* original=nullptr;const ACHAR* dxf=nullptr;if(const auto* p=AcDbProxyObject::cast(object.get())){original=p->originalClassName();dxf=p->originalDxfName();}else if(const auto* p=AcDbProxyEntity::cast(object.get())){original=p->originalClassName();dxf=p->originalDxfName();}
  if(original)proxies.push_back("{\"handle\":"+quote(handleOf(id))+",\"type\":"+quote(object->isA()->name())+",\"originalClass\":"+quote(original)+",\"originalDxf\":"+(dxf?quote(dxf):"null")+"}");
 }
 std::vector<std::string> counts;for(const auto& t:types)counts.push_back("{\"type\":"+quote(t.first)+",\"count\":"+std::to_string(t.second)+"}");
 return "{\"method\":\"public handseed/getAcDbObjectId/openObject; no private fields\",\"handseed\":"+quote(endHandle)+",\"foundCount\":"+std::to_string(found)+",\"erasedCount\":"+std::to_string(erased)+",\"openedCount\":"+std::to_string(opened)+",\"openFailures\":"+std::to_string(failures.size())+",\"failures\":"+axisTraceArray(failures)+",\"proxyCount\":"+std::to_string(proxies.size())+",\"proxies\":"+axisTraceArray(proxies)+",\"types\":"+axisTraceArray(counts)+",\"complete\":"+plotBool(failures.empty())+"}";
}
void nativePublicContractAuditImpl(){
 const auto p=paths();quietRequireIsolatedWorker();PureCommandTrace trace;std::vector<std::string> sources;std::string error="null";
 try{for(const auto& file:{p.scheme,p.reference}){auto db=lifecycleRead(file);WorkingDatabaseScope work(*db);const auto ids=modelIds(*db);const auto before=businessGeometryJson(ids);const auto inventory=publicNativeDatabaseInventory(*db);std::set<std::wstring> types;std::vector<std::string> samples;
  for(int i=0;i<ids.length();++i){std::wstring type;{auto entity=open<AcDbEntity>(ids[i]);type=entity->isA()->name();if(entity->isAProxy()||type.rfind(L"TDb",0)!=0||!types.insert(type).second)continue;}
   std::string dxf="null",visible="null",readError="null";try{dxf=publicAxisDxf(ids[i]);visible=symbolPublicReadback(ids[i]).json;}catch(const std::exception& e){readError=familyError(e);}
   samples.push_back("{\"handle\":"+quote(handleOf(ids[i]))+",\"type\":"+quote(type)+",\"publicDxf\":"+dxf+",\"visible\":"+visible+",\"readError\":"+readError+"}");
  }
  sources.push_back("{\"path\":"+quote(file)+",\"modelCount\":"+std::to_string(ids.length())+",\"originalBefore\":"+before+",\"originalAfter\":"+businessGeometryJson(ids)+",\"databaseInventory\":"+inventory+",\"samples\":"+axisTraceArray(samples)+"}");
 }}catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown native public-contract discovery failure");}
 writeNewReport(p.output+L"\\public-native-contract-audit.json","{\"contract\":\"stage1.public-native-contract-audit.v1\",\"sources\":"+axisTraceArray(sources)+",\"commandTrace\":"+trace.json()+",\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengBusinessCommandsUsed\":false,\"sourceDrawingWritten\":false,\"writeCapabilityProven\":false,\"error\":"+error+"}");
}
void nativePublicContractAudit()noexcept{pureEntry(nativePublicContractAuditImpl);}
