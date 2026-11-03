// Confirmed three-floor project. Geometry stays in the input database; only
// annotation prototypes and the explicitly missing window come from the donor.
#include "dbhatch.h"
bcpproduct::ThreeLayout threeContract(const fs::path& resources){
 const auto folder=resources/L"three-floor";std::ifstream manifest(folder/L"index.tsv");std::string tag,name,digest;int n;
 require(bool(manifest>>tag>>n)&&tag=="BCPTHREERES1"&&n==3,"three-floor resources unavailable");
 std::set<std::string> expected{"annotations.dwg","layout.tsv","areas.tsv"};
 for(int i=0;i<n;++i)require(bool(manifest>>name>>digest)&&expected.erase(name)==1&&sha(folder/name)==digest,"three-floor resource fingerprint differs");
 std::ifstream input(folder/L"layout.tsv");auto c=bcpproduct::readThreeLayout(input);require(c.sha==sha(folder/L"annotations.dwg"),"three-floor annotation donor differs");return c;
}
bool productMatchesThree(AcDbDatabase& db,const fs::path& resources){
 if(!fs::exists(resources/L"three-floor/index.tsv"))return false;
 const auto config=threeContract(resources);Working work(&db);const auto types=modelTypes(db);
 for(const auto& entry:config.source){const auto t=types.find(std::wstring(entry.first.begin(),entry.first.end()));if(t==types.end()||t->second!=std::wstring(entry.second.type.begin(),entry.second.type.end()))return false;
  auto actual=chainGrips(find(db,entry.first));if(actual.size()!=entry.second.grips.size())return false;for(std::size_t i=0;i<actual.size();++i)if(actual[i].distanceTo(approvedPoint(entry.second.grips[i]))>1e-5)return false;
 }
 // Additional furniture/notes are retained; an unconfigured architectural change
 // needs a new project decision rather than inheriting another building's layout.
 for(const auto& e:types)if((e.second==L"TDbWall"||e.second==L"TDbOpening"||e.second==L"TDbGroupOpening")&&!config.source.count(ascii(e.first)))return false;
 return true;
}
void threeColumnGeometry(AcDbDatabase& db,const bcpproduct::ThreeColumn& a){
 auto e=editObject<AcDbEntity>(find(db,a.handle));const auto mapped=[&](AcGePoint2d p){return AcGePoint2d(a.newCentre[0]+a.sx*(p.x-a.oldCentre[0]),a.newCentre[1]+a.sy*(p.y-a.oldCentre[1]));};
 if(auto* poly=AcDbPolyline::cast(e.get())){for(unsigned k=0;k<poly->numVerts();++k){AcGePoint2d p;productStatus(poly->getPointAt(k,p),"read column vertex");productStatus(poly->setPointAt(k,mapped(p)),"correct column vertex");}}
 else if(auto* hatch=AcDbHatch::cast(e.get())){
  struct Loop{Adesk::Int32 type{};AcGePoint2dArray points;AcGeDoubleArray bulges;};std::vector<Loop> loops;
  for(int k=0;k<hatch->numLoops();++k){Loop loop;loop.type=hatch->loopTypeAt(k);
   if(loop.type&AcDbHatch::kPolyline)productStatus(hatch->getLoopAt(k,loop.type,loop.points,loop.bulges),"read column fill polygon");
   else{AcGeVoidPointerArray edges;AcGeIntArray types;productStatus(hatch->getLoopAt(k,loop.type,edges,types),"read column fill edges");for(int z=0;z<edges.length();++z){require(types[z]==AcDbHatch::kLine,"column fill requires straight edges");auto* curve=static_cast<AcGeCurve2d*>(edges[z]);AcGePoint2d start;require(curve->hasStartPoint(start),"column edge start");loop.points.append(start);loop.bulges.append(0);}loop.type|=AcDbHatch::kPolyline;}
   for(int z=0;z<loop.points.length();++z)loop.points[z]=mapped(loop.points[z]);loops.push_back(loop);
  }
  while(hatch->numLoops())productStatus(hatch->removeLoopAt(hatch->numLoops()-1),"replace column fill loop");for(const auto& loop:loops)productStatus(hatch->appendLoop(loop.type,loop.points,loop.bulges),"append corrected column fill");productStatus(hatch->evaluateHatch(),"evaluate corrected column fill");
 }else require(false,"column resource association has changed type");
}
void threeAreaTexts(ProductJob& job,AcDbDatabase& db){
 std::ifstream in(job.profile/L"three-floor/areas.tsv");auto polygons=bcpproduct::readThreeAreas(in);std::map<std::string,std::array<double,6>> bounds;std::map<std::string,double> values;std::map<std::string,bool> totals;
 for(const auto& p:polygons)for(const auto& point:p.points)for(const auto& ref:point)if(!bounds.count(ref.handle)){auto e=open<AcDbEntity>(approvedId(job,db,ref.handle));AcDbExtents box;productStatus(e->getGeomExtents(box),"current area boundary extents");bounds[ref.handle]={box.minPoint().x,box.minPoint().y,box.minPoint().z,box.maxPoint().x,box.maxPoint().y,box.maxPoint().z};}
 for(const auto& p:polygons){values[p.text]+=bcpproduct::evaluateThreeArea(p,bounds);totals[p.text]=p.total;}
 std::ostringstream report;report<<std::setprecision(17)<<'[';bool first=true;
 for(const auto& v:values){const auto number=bcpproduct::formatArea(v.second);std::wstring label(number.begin(),number.end());if(totals.at(v.first))label=L"本层建筑面积："+label;auto text=editObject<AcDbText>(approvedId(job,db,v.first));productStatus(text->setTextString(label.c_str()),"write geometry-linked area");if(!first)report<<',';first=false;report<<"{\"sourceBody\":\""<<v.first<<"\",\"text\":\""<<axisHandle(text->objectId())<<"\",\"areaM2\":"<<v.second<<'}';}
 report<<']';job.approvedAreas=report.str();
}
void productGenerateThree(ProductJob& job,AcDbDatabase& db){
 Working work(&db);auto config=threeContract(job.profile);job.threeLayout=config;const auto original=modelTypes(db);std::set<AcDbObjectId> locks;
 const auto fields=productReadProperties(db,job.work/L"foundation/public-properties");
 for(const auto& source:config.source){std::map<std::string,double> actual;for(const auto& p:source.second.parameters)actual[p.first]=bcpproduct::propertyNumber(fields.at(source.first),bcpproduct::decodeMessage(p.first));require(bcpproduct::threeParametersMatch(source.second.parameters,actual),u8"E_SOURCE_PARAMETERS：墙厚或门窗宽高、离地、转角与已确认配置不一致，请核对源对象参数。");}
 for(const auto& item:original){auto e=open<AcDbEntity>(find(db,ascii(item.first)));const auto id=e->layerId();auto layer=open<AcDbLayerTableRecord>(id);if(layer->isLocked())locks.insert(id);}
 for(auto id:locks){auto layer=editObject<AcDbLayerTableRecord>(id);layer->setIsLocked(false);}
 for(const auto& w:config.walls){auto id=find(db,w.first);for(int k=0;k<2;++k){auto e=editObject<AcDbEntity>(id);AcGePoint3dArray points;productStatus(e->getStretchPoints(points),"source wall endpoints");require(points.length()==2,"wall endpoint association changed");AcDbIntArray selected;selected.append(k);productStatus(e->moveStretchPointsAt(selected,approvedPoint(w.second[k])-points[k]),"apply agreed wall positioning correction");}}
 for(const auto& o:config.openings)if(config.keep.count(o.handle)){auto id=find(db,o.handle);const auto now=chainGrips(id);require(now.size()>=o.core.size(),"source opening core missing");auto e=editObject<AcDbEntity>(id);productStatus(e->transformBy(AcGeMatrix3d::translation(approvedPoint(o.core[0])-now[0])),"position source opening without changing its shape");}
 for(const auto& c:config.columns)threeColumnGeometry(db,c);
 for(const auto& h:config.erase){auto e=editObject<AcDbEntity>(find(db,h));productStatus(e->erase(),"replace confirmed vector text/window in work copy");}
 AcDbDatabase donor(false,true);productStatus(donor.readDwgFile((job.profile/L"three-floor/annotations.dwg").c_str(),AcDbDatabase::kForReadAndAllShare,false),"read three-floor native prototypes");productStatus(donor.closeInput(true),"close three-floor resources");approvedCopyLayerProperties(donor,db);productStatus(db.setLtscale(donor.ltscale()),"reference linetype scale");productStatus(db.setDimscale(donor.dimscale()),"reference dimension scale");
 AcDbIdMapping map;productStatus(map.setDestDb(&db),"three-floor clone destination");
 for(const auto& k:config.keep){auto to=find(db,k.first);auto source=open<AcDbEntity>(to);require(!source->isAProxy()&&std::wstring(source->isA()->name())==std::wstring(k.second.begin(),k.second.end()),"source entity association changed");source.reset();AcDbIdPair pair(find(donor,k.first),to,false,false);productStatus(map.assign(pair),"bind native annotations to source objects");job.approvedMap[k.first]=k.first;}
 AcDbObjectIdArray ids;for(const auto& c:config.clone)ids.append(find(donor,c.first));productStatus(donor.wblockCloneObjects(ids,modelId(db),map,AcDb::kDrcIgnore),"create native three-floor annotations");
 for(const auto& c:config.clone){AcDbIdPair pair;pair.setKey(find(donor,c.first));require(map.compute(pair)&&pair.isCloned()&&!pair.value().isNull(),"annotation mapping missing");job.approvedMap[c.first]=axisHandle(pair.value());auto e=open<AcDbEntity>(pair.value());require(e->ownerId()==modelId(db)&&!e->isAProxy()&&std::wstring(e->isA()->name())==std::wstring(c.second.begin(),c.second.end()),"native annotation class differs");}
 for(const auto& k:config.keep){auto from=open<AcDbEntity>(find(donor,k.first));auto to=editObject<AcDbEntity>(find(db,k.first));AcString layer;{auto r=open<AcDbLayerTableRecord>(from->layerId());productStatus(r->getName(layer),"prototype layer name");}productStatus(to->setLayer(layer),"confirmed source layer");productStatus(to->setColor(from->color()),"source drafting colour");productStatus(to->setVisibility(from->visibility()),"suppress superseded source annotations");}
 AcDbObjectIdArray order;{auto m=open<AcDbBlockTableRecord>(modelId(donor));AcDbSortentsTable* raw{};if(m->getSortentsTable(raw,AcDb::kForRead,false)==Acad::eOk){std::unique_ptr<AcDbSortentsTable,Close<AcDbSortentsTable>> table(raw);productStatus(table->getFullDrawOrder(order),"native prototype draw order");}}
 AcDbObjectIdArray targetOrder;for(int i=0;i<order.length();++i){const auto h=axisHandle(order[i]);if(job.approvedMap.count(h))targetOrder.append(approvedId(job,db,h));}
 {auto m=editObject<AcDbBlockTableRecord>(modelId(db));AcDbSortentsTable* raw{};productStatus(m->getSortentsTable(raw,AcDb::kForWrite,true),"three-floor draw order");std::unique_ptr<AcDbSortentsTable,Close<AcDbSortentsTable>> table(raw);m.reset();productStatus(table->setRelativeDrawOrder(targetOrder),"preserve approved label drawing order");}
 for(auto id:locks){auto layer=editObject<AcDbLayerTableRecord>(id);layer->setIsLocked(true);}
 threeAreaTexts(job,db);job.preparedRoots=static_cast<int>(modelTypes(db).size());require(job.preparedRoots==original.size()-config.erase.size()+config.clone.size(),"native import unexpectedly copied source architecture");job.finalRoots=job.preparedRoots;
 const auto control=job.work/L"join/control.dwg";productStatus(db.saveAs(control.c_str(),false,AcDb::kDHL_CURRENT),"save prepared three-floor drawing");job.preparedHash=sha(control);
}
void productFinishThree(){
 auto& job=*productJob;auto& db=*curDoc()->database();Working work(&db);ApprovedCommandVars vars;const auto& config=*job.threeLayout;
 if(!acedRegCmds->lookupGlobalCmd(L"TChWinLab")){const auto module=fs::path(productSetting(L"tangentDirectory",L"C:\\Tangent\\TArchT20V8\\SYS23x64"))/L"tch_kerncmd.arx";require(acrxDynamicLinker->loadModule(module.c_str(),false,false),"load authorized native numbering");}
 for(const auto& o:config.openings){const auto id=approvedId(job,db,o.handle);if(!o.number.empty()){WholeSelection selection(id);const auto number=bcpproduct::decodeMessage(o.number);require(acedCommandS(RTSTR,L"TChWinLab",RTPICKS,selection.value,RTSTR,L"",RTSTR,number.c_str(),RTNONE)==RTNORM,"write native opening number");auto points=chainGrips(id);require(points.size()==o.core.size()+1,"native opening label grip missing");moveDimensionGrip(id,static_cast<int>(points.size()-1),approvedPoint(o.label));++job.captions;}
  const auto actual=chainGrips(id);require(actual.size()==o.core.size()+(o.number.empty()?0:1),"opening shape point count differs");for(std::size_t i=0;i<o.core.size();++i)require(actual[i].distanceTo(approvedPoint(o.core[i]))<1e-5,"numbering or positioning changed physical opening shape");
 }
 threeAreaTexts(job,db);job.chains=job.dimensions=0;for(const auto& e:modelTypes(db))if(e.second==L"TDbDimension2"){auto d=open<AcDbEntity>(find(db,ascii(e.first)));auto layer=open<AcDbLayerTableRecord>(d->layerId());if(d->visibility()!=AcDb::kVisible||layer->isOff()||layer->isFrozen())continue;d.reset();++job.chains;const auto values=chainValues(find(db,ascii(e.first)));for(auto v:values)require(v>0&&std::abs(v-std::round(v))<1e-5,"dimension did not follow integer source geometry");job.dimensions+=static_cast<int>(values.size());}
 productLatestStyle(db);job.finalRoots=static_cast<int>(modelTypes(db).size());approvedResultView(db);vars.restore();require(sha(job.source)==job.sourceHash&&!fs::exists(job.output),"input changed or output exists");
 std::ostringstream mapping;mapping<<'[';bool first=true;for(const auto& m:job.approvedMap){if(!first)mapping<<',';first=false;mapping<<"{\"template\":\""<<m.first<<"\",\"output\":\""<<m.second<<"\"}";}mapping<<']';writeNew(job.root/L"approved-final-object-map.json",mapping.str());
 writeNew(job.root/L"three-floor-generation.json",std::string("{\"sourceWallsReplaced\":false,\"nativeOpeningNumbers\":")+std::to_string(job.captions)+",\"areas\":"+job.approvedAreas+",\"runtimePython\":false,\"roomNamesFromNativePrototypes\":true}");
 const auto fingerprint=productFingerprint(db);productStatus(db.saveAs((job.work/L"join/control.dwg").c_str(),true,AcDb::kDHL_CURRENT),"save three-floor work document");const auto candidate=job.work/L"ready-to-publish.dwg";productStatus(db.saveAs(candidate.c_str(),false,AcDb::kDHL_CURRENT),"save three-floor construction candidate");
 {AcDbDatabase saved(false,true);productStatus(saved.readDwgFile(candidate.c_str(),AcDbDatabase::kForReadAndAllShare,false),"reopen three-floor result");productStatus(saved.closeInput(true),"finish three-floor readback");require(bcpproduct::samePublicGeometry(productFingerprint(saved),fingerprint),"three-floor result changed on save/reopen");}
 bcpproduct::publishNewFile(candidate,job.output);writeNew(job.root/L"待确认清单.csv",std::string("\xEF\xBB\xBF")+u8"类别,对象,状态,说明\r\n散水与明沟,项目,暂停,未生成\r\n正式图框CTB详图库,项目,待补,未作为完成项\r\n图面避让,项目,复核,保留样张中尚未全量验收的密集部位\r\n");job.phase=ProductPhase::OpenFinal;productQueueSession(L"_BCP_INTERNAL_OPEN_FINAL\n");
}
