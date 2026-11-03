AcDbObjectId btCopyEntity(AcDbDatabase& db,AcDbObjectId source){auto e=open<AcDbEntity>(source);auto clone=std::unique_ptr<AcDbEntity>(AcDbEntity::cast(e->clone()));require(bool(clone),"clone native entity");e.reset();auto model=editObject<AcDbBlockTableRecord>(modelId(db));AcDbObjectId id;status(model->appendAcDbEntity(id,clone.get()),"append native clone");clone.release()->close();return id;}
void btReviewFix() noexcept {try{
 const auto config=iniPath();isolated(ini(config,L"nonce"));const fs::path job=ini(config,L"receiptDirectory"),root=ini(config,L"taskRoot");auto& db=*curDoc()->database();Working work(&db);productDependencies();require(sha(job/L"control.dwg")==ascii(ini(config,L"sourceHash")),"review exact input copy");
 {auto table=open<AcDbLayerTable>(db.layerTableId());AcDbObjectId id;status(table->getAt(L"PUB_DIM",id),"agreed PUB_DIM layer");table.reset();auto layer=editObject<AcDbLayerTableRecord>(id);AcCmColor color;color.setColorIndex(25);layer->setColor(color);require(layer->color().colorIndex()==25,"PUB_DIM colour25");}
 // Move the existing 1000mm door as a rigid native object, preserving its own label.
 wholeTranslate(find(db,"CE2"),{0,19,0});const auto door=chainGrips(find(db,"CE2"));require(std::abs(door[1].y-7820)<1e-5&&std::abs(door[2].y-8820)<1e-5,"door location 600/1000/200");
 {auto id=find(db,"3C42");auto p=chainGrips(id);for(int k:{6,7}){auto q=p[k];q.y+=19;moveDimensionGrip(id,k,q);}for(auto entry:std::vector<std::pair<int,double>>{{12,7510.5+9.5},{13,8320},{14,8920}}){auto q=p[entry.first];q.y=entry.second;moveDimensionGrip(id,entry.first,q);}const auto v=chainValues(id);require(v.size()==6&&std::abs(v[3]-600)<1e-5&&std::abs(v[4]-1000)<1e-5&&std::abs(v[5]-200)<1e-5,"door dimension actual lengths");}
 // A real 900x1800 window cloned from the same top host wall, not vector decoration.
 const auto added=btCopyEntity(db,find(db,"ED5"));wholeTranslate(added,{3460,0,0});
 if(!acedRegCmds->lookupGlobalCmd(L"TChWinLab"))require(acrxDynamicLinker->loadModule(L"C:/Tangent/TArchT20V8/SYS23x64/tch_kerncmd.arx",false,false),"load public numbering");
 {WholeSelection selected(added);require(acedCommandS(RTSTR,L"TChWinLab",RTPICKS,selected.value,RTSTR,L"",RTSTR,L"LC0918",RTNONE)==RTNORM,"missing window own number");auto p=chainGrips(added);moveDimensionGrip(added,static_cast<int>(p.size()-1),{4130,13550,0});require(std::abs(p[1].x-3680)<1e-5&&std::abs(p[2].x-4580)<1e-5,"missing window endpoint position");}
 for(const auto h:{"1CDC","1CDD","1CDE","1CDF","1CE0","1CE1"}){auto e=editObject<AcDbEntity>(find(db,h));status(e->erase(),"replace old PDF window outline");}
 // Rebuild only the affected third-row chain from true one-segment prototypes, then join.
 const auto oldId=find(db,"3C59");const auto old=chainGrips(oldId);const auto oldValues=chainValues(oldId);std::vector<double> stations;for(std::size_t k=2;k<=oldValues.size()+2;++k)stations.push_back(old[k].x);stations.push_back(3680);stations.push_back(4580);std::sort(stations.begin(),stations.end());
 AcDbObjectId prototype;for(const auto& item:modelTypes(db))if(item.second==L"TDbDimension2"){auto id=find(db,ascii(item.first));if(chainValues(id).size()==1){prototype=id;break;}}require(!prototype.isNull(),"native single dimension prototype");
 std::vector<AcDbObjectId> pieces;std::vector<AcGePoint3d> text;
 for(std::size_t k=1;k<stations.size();++k){const double a=stations[k-1],b=stations[k];auto id=btCopyEntity(db,prototype);auto p=chainGrips(id);if(std::abs(p[0].x-p[1].x)<1e-5){auto e=editObject<AcDbEntity>(id);status(e->transformBy(AcGeMatrix3d::rotation(-3.14159265358979323846/2,AcGeVector3d::kZAxis,p[2])),"horizontal chain segment");}
  moveDimensionGrip(id,2,{a,old[2].y,0});moveDimensionGrip(id,3,{b,old[2].y,0});moveDimensionGrip(id,0,{a,old[0].y,0});AcGePoint3d number((a+b)/2,old[0].y+240,0);
  for(std::size_t z=0;z<oldValues.size();++z)if(std::abs(a-old[z+2].x)<1e-5&&std::abs(b-old[z+3].x)<1e-5)number=old[oldValues.size()+3+z];
  if(std::abs(a-4580)<1e-5&&std::abs(b-4800)<1e-5)number.y=old[0].y-300;
  moveDimensionGrip(id,4,number);moveDimensionGrip(id,5,{a,old[old.size()-3].y,0});moveDimensionGrip(id,7,{b,old.back().y,0});{auto e=editObject<AcDbEntity>(id);status(e->setLayer(L"PUB_DIM"),"third dimension layer");status(e->setColorIndex(256),"dimension ByLayer");}pieces.push_back(id);text.push_back(number);
 }
 auto result=pieces[0];for(std::size_t k=1;k<pieces.size();++k){const auto before=wholeModelIds(db);WholeSelection first(result),second(pieces[k]);require(acedCommandS(RTSTR,L"TMergeDim",RTPICKS,first.value,RTPICKS,second.value,RTSTR,L"",RTNONE)==RTNORM,"join replacement third-row chain");const auto after=wholeModelIds(db);std::set<AcDbObjectId> candidates;for(auto id:after)if(!before.count(id)||id==result||id==pieces[k])candidates.insert(id);require(candidates.size()==1&&after.size()+1==before.size(),"one whole replacement chain");result=*candidates.begin();}
 {auto e=editObject<AcDbEntity>(oldId);status(e->erase(),"replace superseded whole chain");}auto values=chainValues(result);require(values.size()==stations.size()-1,"new chain count");for(std::size_t k=0;k<values.size();++k){require(std::abs(values[k]-(stations[k+1]-stations[k]))<1e-5,"new native window dimension");moveDimensionGrip(result,static_cast<int>(values.size()+3+k),text[k]);}
 // Single-sided axis collections must start outside that same facade, not cross the plan.
 int trimmed=0;std::ostringstream axisReport;axisReport<<'[';bool firstAxis=true;
 for(const auto& item:modelTypes(db))if(item.second==L"TDbAxisLabelSet"){
  auto id=find(db,ascii(item.first));auto g=chainGrips(id);if(g.size()!=4||std::abs(g[0].y-g[1].y)>1e-5)continue;
  const double cx=(g[0].x+g[1].x)/2;const int floor=cx<25000?0:cx<70000?1:2;double lo=1e100,hi=-1e100;
  for(const auto& w:modelTypes(db))if(w.second==L"TDbWall"){auto p=chainGrips(find(db,ascii(w.first)));if((p[0].x<25000?0:p[0].x<70000?1:2)!=floor)continue;lo=std::min(lo,std::min(p[0].x,p[1].x)-120);hi=std::max(hi,std::max(p[0].x,p[1].x)+120);}
  auto target=g[0];target.x=g[1].x>(lo+hi)/2?hi+1000:lo-1000;if(target.distanceTo(g[0])<1e-5)continue;
  approvedgrips::endpoint(db,id,g[0],target,job/(L"axis-"+item.first+L".json"));if(!firstAxis)axisReport<<',';firstAxis=false;axisReport<<"{\"handle\":\""<<ascii(item.first)<<"\",\"before\":"<<wholePoints(g)<<",\"after\":"<<wholePoints(chainGrips(id))<<'}';++trimmed;
 }axisReport<<']';
 int names=0;std::ostringstream nameReport;nameReport<<'[';bool firstName=true;
 if(GetPrivateProfileIntW(L"stage2",L"allowRoomTextConversion",0,config.c_str())==1){
  require(acedRegCmds->lookupGlobalCmd(L"TTextConv")!=nullptr,"public text conversion not loaded");std::ifstream lines(root/L"room-text-plan.tsv");std::string hex,erase;double x,y;
  while(lines>>hex>>x>>y>>erase){std::string utf;for(std::size_t k=0;k<hex.size();k+=2)utf.push_back(static_cast<char>(std::stoi(hex.substr(k,2),nullptr,16)));const auto label=bcpproduct::decodeMessage(utf);auto textId=btText(db,label,x,y,350,.8);
   const auto before=wholeModelIds(db);{WholeSelection selected(textId);require(acedCommandS(RTSTR,L"TTextConv",RTPICKS,selected.value,RTSTR,L"",RTNONE)==RTNORM,"public native room text conversion");}auto after=wholeModelIds(db);std::vector<AcDbObjectId> candidates;for(auto id:after)if(!before.count(id)||id==textId){auto e=open<AcDbEntity>(id);if(std::wstring(e->isA()->name())==L"TDbText")candidates.push_back(id);}require(candidates.size()==1,"actual TDbText required");const auto native=candidates[0];AcDbExtents box;{auto e=open<AcDbEntity>(native);status(e->getGeomExtents(box),"native room name bounds");}wholeTranslate(native,{x-(box.minPoint().x+box.maxPoint().x)/2,y-(box.minPoint().y+box.maxPoint().y)/2,0});
   std::istringstream ids(erase);std::string h;while(std::getline(ids,h,',')){auto e=editObject<AcDbEntity>(find(db,h));status(e->erase(),"replace source room glyph");}
   if(!firstName)nameReport<<',';firstName=false;nameReport<<"{\"handle\":\""<<axisHandle(native)<<"\",\"utf8Hex\":\""<<hex<<"\",\"x\":"<<x<<",\"y\":"<<y<<'}';++names;
  }
 }nameReport<<']';
 approvedResultView(db);const auto output=job/L"百塘村邹总_三层平面_门窗轴线文字修订_v5.dwg";status(db.saveAs(output.c_str(),false,AcDb::kDHL_CURRENT),"save review fixes");
 std::ostringstream report;report<<"{\"rootCount\":"<<modelTypes(db).size()<<",\"addedWindow\":\""<<axisHandle(added)<<"\",\"newTopChain\":\""<<axisHandle(result)<<"\",\"trimmedAxes\":"<<trimmed<<",\"axes\":"<<axisReport.str()<<",\"roomTextCount\":"<<names<<",\"roomTexts\":"<<nameReport.str()<<",\"outputSHA256\":\""<<sha(output)<<"\"}";writeNew(job/L"tidy.done",report.str());
 }catch(const std::exception& e){writeNew(fs::path(ini(iniPath(),L"receiptDirectory"))/L"tidy.error",e.what());}}
