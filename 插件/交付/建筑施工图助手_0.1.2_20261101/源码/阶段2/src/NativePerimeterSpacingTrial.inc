// Opt-in layout spike. All distances are model millimetres for the frozen sample.
// The borrowed modern-grip appData is passed back unchanged, never decoded/freed.
struct TrialGripData {
 AcDbEntity& entity;AcDbGripDataPtrArray grips;bool started=false,cleaned=false,safe=true;
 explicit TrialGripData(AcDbEntity& e):entity(e){}
 void cleanup()noexcept{
  if(!started||cleaned)return;cleaned=true;
  try{entity.gripStatus(AcDb::kGripsToBeDeleted);}catch(...){safe=false;}
  for(int i=0;i<grips.length();++i)if(grips[i]){auto* p=grips[i]->alternateBasePoint();bool first=p!=nullptr;for(int j=0;j<i&&first;++j)if(grips[j]&&grips[j]->alternateBasePoint()==p)first=false;if(first)delete p;}
  for(int i=0;i<grips.length();++i)if(grips[i]){bool first=true;for(int j=0;j<i&&first;++j)if(grips[j]==grips[i])first=false;if(first)delete grips[i];}grips.setLogicalLength(0);
  try{entity.gripStatus(AcDb::kGripsDone);}catch(...){safe=false;}
 }
 ~TrialGripData(){cleanup();}
};
void trialAxisJunction(AcDbObjectId id,const AcGePoint3d& junction,const AcGeVector3d& offset){
 resbuf screen{},size{},grip{},direction{};
 require(acedGetVar(L"SCREENSIZE",&screen)==RTNORM&&(screen.restype==RTPOINT||screen.restype==RT3DPOINT)&&acedGetVar(L"VIEWSIZE",&size)==RTNORM&&size.restype==RTREAL&&acedGetVar(L"GRIPSIZE",&grip)==RTNORM&&grip.restype==RTSHORT&&acedGetVar(L"VIEWDIR",&direction)==RTNORM&&direction.restype==RT3DPOINT,"trial modern grip view unavailable");
 require(screen.resval.rpoint[1]>0&&size.resval.rreal>0&&grip.resval.rint>0,"trial modern grip view invalid");
 require(std::abs(direction.resval.rpoint[0])<1e-9&&std::abs(direction.resval.rpoint[1])<1e-9&&direction.resval.rpoint[2]>0,"trial requires actual top view");
 auto e=editObject<AcDbEntity>(id);TrialGripData owned(*e);owned.started=true;
 status(e->getGripPoints(owned.grips,screen.resval.rpoint[1]/size.resval.rreal,grip.resval.rint,{direction.resval.rpoint[0],direction.resval.rpoint[1],direction.resval.rpoint[2]},0),"trial modern axis grips");
 void* selected=nullptr;int matches=0;for(int i=0;i<owned.grips.length();++i){auto* g=owned.grips[i];if(g&&(g->gripPoint()-junction).length()<1e-6){++matches;selected=g->appData();}}
 require(matches==1&&selected,"trial axis junction is not unique");AcDbVoidPtrArray data;data.append(selected);status(e->moveGripPointsAt(data,offset,0),"trial axis longitudinal move");owned.cleanup();require(owned.safe,"trial modern grip cleanup unresolved");
}
void trialPositionAxis(AcDbObjectId id,bool vertical,double lowWall,double highWall,double totalOffset,bool alignCentre=false){
 // Keep endpoint ordering while stretching far-translated building panels.
 if(alignCentre){const auto original=axisInk(id,vertical);const double oldMiddle=vertical?(original.circles[0].centre.y+original.circles[1].centre.y)/2:(original.circles[0].centre.x+original.circles[1].centre.x)/2;const double shift=(lowWall+highWall)/2-oldMiddle;{auto e=editObject<AcDbEntity>(id);status(e->transformBy(AcGeMatrix3d::translation(vertical?AcGeVector3d(0,shift,0):AcGeVector3d(shift,0,0))),"align axis material longitudinal centre before endpoint stretch");}}
 for(int side=0;side<2;++side){
  const auto before=axisInk(id,vertical);const auto circle=before.circles[side];require(std::abs(circle.radius-400)<1e-6,"trial expects measured 400 radius");
  const double sign=side?1:-1;const AcGeVector3d outward=vertical?AcGeVector3d(0,sign,0):AcGeVector3d(sign,0,0);
  const double wall=side?highWall:lowWall;const double desired=wall+sign*(totalOffset+600+circle.radius);const double old=vertical?circle.centre.y:circle.centre.x;const auto offset=vertical?AcGeVector3d(0,desired-old,0):AcGeVector3d(desired-old,0,0);
  trialAxisJunction(id,circle.centre-outward*circle.radius,offset);
  const auto after=axisInk(id,vertical);require((after.circles[side].centre-circle.centre-offset).length()<1e-6&&std::abs(after.circles[side].radius-400)<1e-6,"trial circle failed exact longitudinal placement");
  require((after.circles[1-side].centre-before.circles[1-side].centre).length()<1e-6&&std::abs(after.circles[1-side].radius-before.circles[1-side].radius)<1e-6,"trial changed opposite axis end");
 }
}
void trialGuide(AcDbDatabase& db,DrawnAnnotations& drawn,const AcGePoint3d& a,const AcGePoint3d& b){
 auto e=std::make_unique<AcDbLine>(a,b);e->setDatabaseDefaults(&db);status(e->setLayer(L"AXIS"),"trial guide layer");status(e->setColorIndex(8),"trial guide grey colour");AcDbObjectId id;{auto model=editObject<AcDbBlockTableRecord>(modelId(db));status(model->appendAcDbEntity(id,e.get()),"append explicit trial guide");}e.release()->close();drawn.guides.push_back({id,a,b});
}
