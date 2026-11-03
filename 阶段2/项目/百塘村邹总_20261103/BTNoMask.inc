void btNoMask() noexcept {
 try {
  const auto config=iniPath(); isolated(ini(config,L"nonce"));
  const fs::path job=ini(config,L"receiptDirectory"),root=ini(config,L"taskRoot");
  auto& db=*curDoc()->database(); Working working(&db); productDependencies();
  require(sha(job/L"control.dwg")==ascii(ini(config,L"sourceHash")),"no-mask exact input copy");
  std::vector<AcDbObjectId> masks,dimensions;
  {auto table=open<AcDbBlockTable>(db.blockTableId());AcDbBlockTableIterator* raw{};status(table->newIterator(raw),"scan all block definitions for wipeouts");std::unique_ptr<AcDbBlockTableIterator> blocks(raw);
   for(;!blocks->done();blocks->step()){AcDbObjectId block;status(blocks->getRecordId(block),"mask block id");auto record=open<AcDbBlockTableRecord>(block);AcDbBlockTableRecordIterator* entitiesRaw{};status(record->newIterator(entitiesRaw),"mask entity iterator");std::unique_ptr<AcDbBlockTableRecordIterator> entities(entitiesRaw);
    for(;!entities->done();entities->step()){AcDbObjectId id;status(entities->getEntityId(id),"mask entity id");auto entity=open<AcDbEntity>(id);if(AcDbWipeout::cast(entity.get()))masks.push_back(id);else if(block==modelId(db)&&std::wstring(entity->isA()->name())==L"TDbDimension2")dimensions.push_back(id);}
   }
  }
  std::set<std::string> editable;for(auto id:masks)editable.insert(axisHandle(id));for(auto id:dimensions)editable.insert(axisHandle(id));const auto protectedBefore=wholeProtected(db,editable);
  for(auto id:masks){auto entity=editObject<AcDbEntity>(id);status(entity->erase(),"remove region coverage entity");}
  struct Envelope{double x0=1e100,y0=1e100,x1=-1e100,y1=-1e100;};std::vector<Envelope> envelopes;
  for(int n=1;n<=3;++n){Envelope e;std::ifstream in(root/(L"envelope-"+std::to_wstring(n)+L".tsv"));double x,y;while(in>>x>>y){e.x0=std::min(e.x0,x);e.x1=std::max(e.x1,x);e.y0=std::min(e.y0,y);e.y1=std::max(e.y1,y);}require(e.x1>e.x0&&e.y1>e.y0,"known actual envelope");envelopes.push_back(e);}
  std::ostringstream checks;checks<<'[';bool first=true;int adjusted=0;
  for(auto id:dimensions){const auto before=chainGrips(id);const auto values=chainValues(id);const auto n=values.size();require(before.size()==2*n+6,"whole native dimension grip layout");const bool vertical=std::abs(before[0].x-before[1].x)<1e-5;const double centre=(before[0].x+before[1].x)/2;const auto envelope=*std::min_element(envelopes.begin(),envelopes.end(),[&](const auto& a,const auto& b){return std::abs(centre-(a.x0+a.x1)/2)<std::abs(centre-(b.x0+b.x1)/2);});
   double normal=0;bool exterior=false;
   if(vertical&&before[0].x<envelope.x0){normal=envelope.x0-1000;exterior=true;}else if(vertical&&before[0].x>envelope.x1){normal=envelope.x1+1000;exterior=true;}else if(!vertical&&before[0].y<envelope.y0){normal=envelope.y0-1000;exterior=true;}else if(!vertical&&before[0].y>envelope.y1){normal=envelope.y1+1000;exterior=true;}
   if(exterior){
    // The native tail grip is a length control, not the visible witness-line start.
    // Read public exploded DIMEXO and project the measuring stations normally;
    // their along-wall coordinates and all measured distances remain unchanged.
    double gap=0;{auto entity=open<AcDbEntity>(id);TemplateParts parts(entity.get());status(entity->explode(parts.values),"public dimension witness offset");require(parts.values.length()>0,"dimension display parts");auto* d=AcDbAlignedDimension::cast(static_cast<AcRxObject*>(parts.values[0]));require(d!=nullptr,"public aligned dimension display");gap=d->dimexo()*d->dimscale();require(std::isfinite(gap)&&gap>=0&&gap<1000,"reference witness gap");}
    const double baseline=vertical?before[0].x:before[0].y;const double sign=baseline>normal?1:-1;const double datum=normal-sign*gap;
    for(std::size_t k=2;k<=n+2;++k){auto p=before[k];if(vertical)p.x=datum;else p.y=datum;moveDimensionGrip(id,static_cast<int>(k),p);}
    moveDimensionGrip(id,0,before[0]);for(std::size_t k=n+3;k<2*n+3;++k)moveDimensionGrip(id,static_cast<int>(k),before[k]);++adjusted;
   }
   const auto after=chainGrips(id);const auto read=chainValues(id);require(read.size()==n,"no-mask dimension count preserved");for(std::size_t k=0;k<n;++k)require(std::abs(values[k]-read[k])<1e-5,"no-mask dimension measurements preserved");for(std::size_t k=0;k<2*n+3;++k){if(exterior&&k>=2&&k<=n+2)require(std::abs((vertical?before[k].y:before[k].x)-(vertical?after[k].y:after[k].x))<1e-5,"no-mask along-wall measurement station preserved");else require(before[k].distanceTo(after[k])<1e-5,"no-mask baseline and text preserved");}
   if(!first)checks<<',';first=false;checks<<"{\"handle\":\""<<axisHandle(id)<<"\",\"exteriorAdjusted\":"<<(exterior?"true":"false")<<",\"values\":"<<chainProbeNumbers(read)<<",\"grips\":"<<wholePoints(after)<<'}';
  }
  checks<<']';require(wholeProtected(db,editable)==protectedBefore,"no-mask protected architecture or labels changed");approvedResultView(db);
  const auto output=job/L"百塘村邹总_三层平面_取消区域覆盖_v2.dwg";status(db.saveAs(output.c_str(),false,AcDb::kDHL_CURRENT),"save no region coverage drawing");
  AcDbDatabase saved(false,true);status(saved.readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false),"reopen no-mask result");status(saved.closeInput(true),"finish no-mask input");{Working read(&saved);require(wholeProtected(saved,editable)==protectedBefore,"saved no-mask protected geometry");for(auto id:masks){AcDbObjectId absent;const auto result=saved.getAcDbObjectId(absent,false,id.handle());require(result!=Acad::eOk||absent.isErased(),"erased region coverage reappeared");}}
  std::ostringstream report;report<<"{\"rootCount\":"<<modelTypes(db).size()<<",\"removedWipeouts\":"<<masks.size()<<",\"adjustedExteriorChains\":"<<adjusted<<",\"dimensionChecks\":"<<checks.str()<<",\"protectedGeometryIdentical\":true,\"savedReopened\":true,\"outputSHA256\":\""<<sha(output)<<"\"}";writeNew(job/L"tidy.done",report.str());
 }catch(const std::exception& e){writeNew(fs::path(ini(iniPath(),L"receiptDirectory"))/L"tidy.error",e.what());}
}
