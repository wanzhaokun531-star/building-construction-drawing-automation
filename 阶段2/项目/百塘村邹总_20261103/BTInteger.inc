// One-off drawing correction using public entity transforms and native grips.
void btInteger() noexcept {
 try {
  const auto config=iniPath();isolated(ini(config,L"nonce"));
  const fs::path job=ini(config,L"receiptDirectory"),root=ini(config,L"taskRoot");
  auto& db=*curDoc()->database();Working working(&db);productDependencies();
  require(sha(job/L"control.dwg")==ascii(ini(config,L"sourceHash")),"integer input copy changed");
  const auto original=modelTypes(db);
  std::map<std::string,std::vector<AcGePoint3d>> before,expected;
  std::vector<AcDbObjectId> dimensions;
  std::set<AcDbObjectId> locks;
  for(const auto& item:original){const auto h=ascii(item.first);auto id=find(db,h);auto e=open<AcDbEntity>(id);require(!AcDbWipeout::cast(e.get()),"region coverage prohibited");const auto layerId=e->layerId();e.reset();{auto layer=open<AcDbLayerTableRecord>(layerId);if(layer->isLocked())locks.insert(layerId);}if(item.second==L"TDbWall"||item.second==L"TDbOpening"||item.second==L"TDbGroupOpening"||item.second==L"TDbAxisLabelSet")before[h]=chainGrips(id);if(item.second==L"TDbDimension2")dimensions.push_back(id);}
  for(auto id:locks){auto layer=editObject<AcDbLayerTableRecord>(id);layer->setIsLocked(false);}
  // Read all absolute targets before any wall reactors can reposition an opening.
  struct Action{char kind{};std::string h;int index{};AcGePoint3d a,b;double area{},scaleY{};bool total{};};std::vector<Action> actions;
  std::ifstream input(root/L"integer-actions.tsv");std::string line;
  while(std::getline(input,line)){std::istringstream in(line);Action a;in>>a.kind>>a.h;
   if(a.kind=='W')in>>a.a.x>>a.a.y>>a.a.z>>a.b.x>>a.b.y>>a.b.z;
   else if(a.kind=='T'){in>>a.a.x>>a.a.y>>a.a.z;auto points=before.at(a.h);const AcGeVector3d delta(a.a.x,a.a.y,a.a.z);for(auto& p:points)p+=delta;expected[a.h]=std::move(points);}
   else if(a.kind=='G')in>>a.index>>a.a.x>>a.a.y>>a.a.z;
   else if(a.kind=='A')in>>a.area>>a.total;
   else if(a.kind=='S')in>>a.a.x>>a.a.y>>a.a.z>>a.b.x>>a.b.y>>a.b.z>>a.area>>a.scaleY;
   else require(false,"unknown integer edit");require(!in.fail(),"invalid integer contract");actions.push_back(a);
  }
  for(const auto& a:actions)if(a.kind=='W'){
   const auto id=find(db,a.h);for(int k=0;k<2;++k){auto e=editObject<AcDbEntity>(id);AcGePoint3dArray points;status(e->getStretchPoints(points),"native wall stretch points");require(points.length()==2,"wall endpoint layout");AcDbIntArray selected;selected.append(k);status(e->moveStretchPointsAt(selected,(k?a.b:a.a)-points[k]),"native wall endpoint correction");}
  }
  for(const auto& a:actions)if(a.kind=='T'){
   const auto id=find(db,a.h);auto now=chainGrips(id);const auto target=expected.at(a.h);require(now.size()==target.size(),"native object grip count changed");auto e=editObject<AcDbEntity>(id);status(e->transformBy(AcGeMatrix3d::translation(target[0]-now[0])),"position native opening or axis");
  }
  for(const auto& a:actions)if(a.kind=='G')moveDimensionGrip(find(db,a.h),a.index,a.a);
  for(const auto& a:actions)if(a.kind=='S'){
   auto e=editObject<AcDbEntity>(find(db,a.h));const auto mapped=[&](AcGePoint2d p){return AcGePoint2d(a.b.x+a.area*(p.x-a.a.x),a.b.y+a.scaleY*(p.y-a.a.y));};
   if(auto* poly=AcDbPolyline::cast(e.get())){for(unsigned k=0;k<poly->numVerts();++k){AcGePoint2d p;status(poly->getPointAt(k,p),"column vertex read");status(poly->setPointAt(k,mapped(p)),"column vertex correction");}}
   else if(auto* hatch=AcDbHatch::cast(e.get())){
    struct Loop{Adesk::Int32 type{};AcGePoint2dArray points;AcGeDoubleArray bulges;};std::vector<Loop> loops;
    for(int k=0;k<hatch->numLoops();++k){Loop loop;loop.type=hatch->loopTypeAt(k);
     if(loop.type&AcDbHatch::kPolyline)status(hatch->getLoopAt(k,loop.type,loop.points,loop.bulges),"column fill polygon");
     else{AcGeVoidPointerArray edges;AcGeIntArray types;status(hatch->getLoopAt(k,loop.type,edges,types),"column fill edges");for(int z=0;z<edges.length();++z){require(types[z]==AcDbHatch::kLine,"column fill must contain straight edges");auto* curve=static_cast<AcGeCurve2d*>(edges[z]);AcGePoint2d start;require(curve->hasStartPoint(start),"column fill edge start");loop.points.append(start);loop.bulges.append(0);}loop.type|=AcDbHatch::kPolyline;}
     for(int z=0;z<loop.points.length();++z)loop.points[z]=mapped(loop.points[z]);loops.push_back(loop);
    }
    while(hatch->numLoops())status(hatch->removeLoopAt(hatch->numLoops()-1),"replace column fill loop");for(const auto& loop:loops)status(hatch->appendLoop(loop.type,loop.points,loop.bulges),"append corrected column fill");status(hatch->evaluateHatch(),"evaluate corrected column fill");
   }else require(false,"unexpected column outline type");
  }
  for(const auto& a:actions)if(a.kind=='A'){
   auto text=editObject<AcDbText>(find(db,a.h));const auto formatted=bcpproduct::formatArea(a.area);std::wstring label(formatted.begin(),formatted.end());if(a.total)label=L"本层建筑面积："+label;status(text->setTextString(label.c_str()),"recomputed area text");
  }
  int walls=0,native=0;double maxWallMove=0;
  for(const auto& a:actions)if(a.kind=='W'){
   const auto now=chainGrips(find(db,a.h));require(now.size()==3&&now[0].distanceTo(a.a)<1e-5&&now[1].distanceTo(a.b)<1e-5,"actual wall endpoints differ");maxWallMove=std::max(maxWallMove,std::max(before.at(a.h)[0].distanceTo(a.a),before.at(a.h)[1].distanceTo(a.b)));++walls;
  }
  for(const auto& pair:expected){const auto actual=chainGrips(find(db,pair.first));require(actual.size()==pair.second.size(),"native correction count");for(std::size_t k=0;k<actual.size();++k)require(actual[k].distanceTo(pair.second[k])<1e-5,"opening shape or native label changed");++native;}
  std::ostringstream checks;checks<<'[';bool first=true;int segments=0;
  for(auto id:dimensions){const auto values=chainValues(id);const auto points=chainGrips(id);double sum=0;for(double v:values){require(v>0&&std::abs(v-std::round(v))<1e-5,"noninteger native dimension");sum+=v;++segments;}require(std::abs(sum-points[2].distanceTo(points[values.size()+2]))<1e-5,"integer dimension closure");if(!first)checks<<',';first=false;checks<<"{\"handle\":\""<<axisHandle(id)<<"\",\"values\":"<<chainProbeNumbers(values)<<",\"grips\":"<<wholePoints(points)<<'}';}checks<<']';
  for(auto id:locks){auto layer=editObject<AcDbLayerTableRecord>(id);layer->setIsLocked(true);}
  require(modelTypes(db)==original,"integer correction changed native entity types");approvedResultView(db);
  const auto output=job/L"百塘村邹总_三层平面_尺寸归整_v4.dwg";status(db.saveAs(output.c_str(),false,AcDb::kDHL_CURRENT),"save integer geometry sample");
  AcDbDatabase saved(false,true);status(saved.readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false),"reopen integer sample");status(saved.closeInput(true),"close integer sample input");{Working read(&saved);require(modelTypes(saved)==original,"saved native classes changed");for(const auto& p:expected){const auto actual=chainGrips(find(saved,p.first));require(approvedgrips::same(actual,p.second),"saved opening or native axis differs");}}
  std::ostringstream report;report<<"{\"rootCount\":"<<original.size()<<",\"wallCount\":"<<walls<<",\"nativeOpeningAndAxisCount\":"<<native<<",\"maxWallEndpointMove\":"<<maxWallMove<<",\"dimensionCount\":"<<dimensions.size()<<",\"integerSegments\":"<<segments<<",\"dimensionChecks\":"<<checks.str()<<",\"savedReopened\":true,\"outputSHA256\":\""<<sha(output)<<"\"}";writeNew(job/L"tidy.done",report.str());
 }catch(const std::exception& e){writeNew(fs::path(ini(iniPath(),L"receiptDirectory"))/L"tidy.error",e.what());}
}
