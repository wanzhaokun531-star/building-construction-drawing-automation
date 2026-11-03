// Public modern-grip operations; borrowed opaque appData is passed unchanged.
namespace approvedgrips {
constexpr double eps=1e-6;
struct Geometry {std::vector<AcGePoint3d> grips,stretch;};
Geometry geometry(AcDbEntity& e){Geometry out;AcGePoint3dArray p;AcDbIntArray a,b;status(e.getGripPoints(p,a,b),"read native legacy grip points");for(int i=0;i<p.length();++i)out.grips.push_back(p[i]);p.setLogicalLength(0);status(e.getStretchPoints(p),"read native stretch points");for(int i=0;i<p.length();++i)out.stretch.push_back(p[i]);return out;}
bool same(const std::vector<AcGePoint3d>& a,const std::vector<AcGePoint3d>& b){if(a.size()!=b.size())return false;for(std::size_t i=0;i<a.size();++i)if(a[i].distanceTo(b[i])>eps)return false;return true;}
bool same(const Geometry& a,const Geometry& b){return same(a.grips,b.grips)&&same(a.stretch,b.stretch);}
Geometry readGeometry(AcDbObjectId id){AcDbEntity* e{};status(acdbOpenObject(e,id,AcDb::kForRead),"open native geometry");struct Close {AcDbEntity* e;~Close(){if(e)e->close();}} close{e};return geometry(*e);}
std::string points(const std::vector<AcGePoint3d>& p){std::ostringstream s;s<<std::setprecision(17)<<'[';for(std::size_t i=0;i<p.size();++i){if(i)s<<',';s<<'['<<p[i].x<<','<<p[i].y<<','<<p[i].z<<']';}return s.str()+']';}
std::string json(const Geometry& g){return "{\"grips\":"+points(g.grips)+",\"stretch\":"+points(g.stretch)+'}';}
struct View {double pixelsPerUnit{};int size{};AcGeVector3d direction;};
View view(){resbuf screen{},height{},size{},dir{};require(acedGetVar(L"SCREENSIZE",&screen)==RTNORM&&acedGetVar(L"VIEWSIZE",&height)==RTNORM&&acedGetVar(L"GRIPSIZE",&size)==RTNORM&&acedGetVar(L"VIEWDIR",&dir)==RTNORM,"native view parameters");require(height.restype==RTREAL&&height.resval.rreal>0&&size.restype==RTSHORT&&size.resval.rint>0,"native view types");View v;v.pixelsPerUnit=screen.resval.rpoint[1]/height.resval.rreal;v.size=size.resval.rint;v.direction={dir.resval.rpoint[0],dir.resval.rpoint[1],dir.resval.rpoint[2]};require(std::isfinite(v.pixelsPerUnit)&&v.pixelsPerUnit>0&&v.direction.z>0&&std::abs(v.direction.x)<eps&&std::abs(v.direction.y)<eps,"top view required");return v;}
struct Grips {
 AcDbEntity& entity;AcDbGripDataPtrArray data;bool acquired{},cleaned{},safe=true;
 explicit Grips(AcDbEntity& e):entity(e){}
 void cleanup() noexcept {if(!acquired||cleaned)return;cleaned=true;try{entity.gripStatus(AcDb::kGripsToBeDeleted);}catch(...){safe=false;}std::set<const void*> alt,wrappers;for(int i=0;i<data.length();++i)if(data[i]){auto* p=data[i]->alternateBasePoint();if(p&&alt.insert(p).second)try{delete p;}catch(...){safe=false;}}for(int i=0;i<data.length();++i)if(data[i]&&wrappers.insert(data[i]).second)try{delete data[i];}catch(...){safe=false;}data.setLogicalLength(0);try{entity.gripStatus(AcDb::kGripsDone);}catch(...){safe=false;}}
 ~Grips(){cleanup();}
};
struct Transaction {
 AcDbTransactionManager* manager;AcTransaction* transaction;bool done{};
 explicit Transaction(AcDbDatabase& db):manager(db.transactionManager()),transaction(nullptr){require(manager&&manager->numActiveTransactions()==0,"idle transaction manager required");transaction=manager->startTransaction();require(transaction!=nullptr,"start native grip transaction");}
 void finish(bool commit){require(!done,"transaction already finished");const auto result=commit?manager->endTransaction():manager->abortTransaction();done=true;status(result,"finish native grip transaction");}
 ~Transaction(){if(!done)manager->abortTransaction();}
 AcDbEntity& entity(AcDbObjectId id){AcDbObject* object{};status(transaction->getObject(object,id,AcDb::kForWrite),"open transaction native axis");auto* e=AcDbEntity::cast(object);require(e&&!e->isAProxy()&&std::wstring(e->isA()->name())==L"TDbAxisLabelSet","real native axis required");return *e;}
};
struct Attempt {int index{},second=-1,count{},status=-1;bool matched{},committed{},hasAppData{},innerMatched{};Geometry after,firstAfter;};
int move(AcDbEntity& e,int index,const AcGeVector3d& offset,const AcGePoint3d* target,const View& v,int& count,bool& hasData){Grips owned(e);owned.acquired=true;status(e.getGripPoints(owned.data,v.pixelsPerUnit,v.size,v.direction,0),"modern native getGripPoints");count=owned.data.length();require(count>0&&count<=64,"modern grip count");int status=-1;if(index<count&&owned.data[index]&&owned.data[index]->appData()){hasData=true;const auto delta=target?*target-owned.data[index]->gripPoint():offset;void* const identity=owned.data[index]->appData();AcDbVoidPtrArray selected;selected.append(identity);status=static_cast<int>(e.moveGripPointsAt(selected,delta,0));}owned.cleanup();require(owned.safe,"modern grip lifecycle cleanup");return status;}
Attempt apply(AcDbDatabase& db,AcDbObjectId id,int index,int second,std::size_t anchor,const Geometry& before,const Geometry& expected,const View& v,bool commit,const AcGeVector3d& calibration=AcGeVector3d(0,0,0)){Attempt out;out.index=index;out.second=second;Transaction tx(db);auto& e=tx.entity(id);const auto target=expected.grips[anchor]+calibration;out.status=move(e,index,AcGeVector3d::kIdentity,&target,v,out.count,out.hasAppData);out.firstAfter=geometry(e);out.after=out.firstAfter;out.innerMatched=out.status==0&&out.after.grips.size()==before.grips.size()&&out.after.grips[anchor].distanceTo(expected.grips[anchor])<eps;
 if(out.innerMatched&&!same(out.after,expected)&&second>=0){const auto restore=before.grips[anchor+1]-out.after.grips[anchor+1];out.status=move(e,second,restore,nullptr,v,out.count,out.hasAppData);out.after=geometry(e);}out.matched=out.status==0&&same(out.after,expected);out.committed=commit&&out.matched;tx.finish(out.committed);return out;}

void endpoint(AcDbDatabase& db,AcDbObjectId id,const AcGePoint3d& old,const AcGePoint3d& target,const fs::path& diagnostic={}){
 std::vector<Attempt> attempts;const auto before=readGeometry(id);auto expected=before;std::size_t anchor=0;int g=0,s=0;
 for(std::size_t i=0;i<expected.grips.size();++i)if(expected.grips[i].distanceTo(old)<eps){anchor=i;expected.grips[i]=target;++g;}
 for(auto& p:expected.stretch)if(p.distanceTo(old)<eps){p=target;++s;}
 require(g==1&&s==1&&anchor%4==0,"unique axis lead endpoint");const auto v=view();int count=1,chosen=-1,second=-1;AcGeVector3d calibration(0,0,0);
 for(int i=0;i<count&&chosen<0;++i){auto t=apply(db,id,i,-1,anchor,before,expected,v,false);count=t.count;attempts.push_back(t);require(same(readGeometry(id),before),"axis trial rollback");AcGeVector3d cal(0,0,0);
  if(!t.innerMatched&&t.status==0&&t.after.grips.size()==before.grips.size()&&t.after.grips[anchor].distanceTo(before.grips[anchor])>eps){cal=target-t.after.grips[anchor];t=apply(db,id,i,-1,anchor,before,expected,v,false,cal);attempts.push_back(t);require(same(readGeometry(id),before),"axis calibrated rollback");}
  if(t.matched){chosen=i;calibration=cal;break;}if(!t.innerMatched)continue;
  for(int j=0;j<count;++j){auto pair=apply(db,id,i,j,anchor,before,expected,v,false,cal);attempts.push_back(pair);require(same(readGeometry(id),before),"axis paired rollback");if(pair.matched){chosen=i;second=j;calibration=cal;break;}}
 }
 if(chosen<0&&!diagnostic.empty()){std::ostringstream out;out<<"{\"nativeHandle\":\""<<axisHandle(id)<<"\",\"before\":"<<json(before)<<",\"expected\":"<<json(expected)<<",\"attempts\":[";bool first=true;for(const auto& a:attempts){if(!first)out<<',';first=false;out<<"{\"index\":"<<a.index<<",\"second\":"<<a.second<<",\"status\":"<<a.status<<",\"inner\":"<<(a.innerMatched?"true":"false")<<",\"after\":"<<json(a.after)<<'}';}out<<"]}";writeNew(diagnostic,out.str());}
 require(chosen>=0,"native axis lead cannot be positioned without moving circle");const auto final=apply(db,id,chosen,second,anchor,before,expected,v,true,calibration);require(final.committed&&same(readGeometry(id),expected),"axis lead readback");
}
}
