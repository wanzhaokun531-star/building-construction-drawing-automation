#pragma once
// Scoped N7 copy of N5 fixture/guard utilities; provenance.json binds original.
void ok(Acad::ErrorStatus status,const char* operation){require(status==Acad::eOk,std::string(operation)+" status="+std::to_string(status));}
template<class T>class Open final {
public:
    explicit Open(AcDbObjectId id,AcDb::OpenMode mode=AcDb::kForRead){ok(acdbOpenObject(p_,id,mode),"open");}
    ~Open(){if(p_)p_->close();}
    T* operator->() const{return p_;} T& operator*() const{return *p_;}
    Open(const Open&)=delete;Open& operator=(const Open&)=delete;
private:T* p_{};
};
struct Monitor final:AcDbDatabaseReactor {
    AcDbDatabase& db;bool alive{true};bool protect{true};std::vector<std::string> events;bool loggingFailed{};std::string role,phase{"monitor-attached"};
    std::function<void(const std::string&)> log;
    explicit Monitor(AcDbDatabase& d,const char* dbRole,std::function<void(const std::string&)> sink):db(d),role(dbRole),log(std::move(sink)){ok(db.addReactor(this),"attach source monitor");}
    ~Monitor(){if(alive)db.removeReactor(this);}
    void goodbye(const AcDbDatabase*) override {alive=false;}
    void begin(const std::string& operation){check();phase=operation;log(bce::native::object({{"kind",quote(std::string("protectedDbPhaseBegin"))},{"dbRole",quote(role)},{"phase",quote(phase)},{"workingDatabaseIsMonitored",acdbHostApplicationServices()->workingDatabase()==&db?"true":"false"}}));}
    void record(const AcDbDatabase* eventDb,const char* kind,const AcDbObject* object) noexcept {try{events.push_back(bce::native::object({{"kind",quote(std::string("protectedDbEvent"))},{"dbRole",quote(role)},{"event",quote(std::string(kind))},{"phase",quote(phase)},
        {"eventDatabaseMatchesMonitor",eventDb==&db?"true":"false"},{"eventObjectOwnedByMonitor",object&&object->database()==&db?"true":"false"},
        {"workingDatabaseIsMonitored",acdbHostApplicationServices()->workingDatabase()==&db?"true":"false"},
        {"handle",quote(object?handle(object->objectId()):std::string{})},{"class",quote(object?std::wstring(object->isA()->name()):std::wstring{})}}));log(events.back());}catch(...){loggingFailed=true;}}
    void objectModified(const AcDbDatabase* d,const AcDbObject* p) override{record(d,"modified",p);}
    void objectOpenedForModify(const AcDbDatabase* d,const AcDbObject* p) override{record(d,"openedForModify",p);}
    void objectAppended(const AcDbDatabase* d,const AcDbObject* p) override{record(d,"appended",p);}
    void objectErased(const AcDbDatabase* d,const AcDbObject* p,Adesk::Boolean) override{record(d,"erased",p);}
    void headerSysVarChanged(const AcDbDatabase* d,const ACHAR*,Adesk::Boolean success) override{if(success)record(d,"header",nullptr);}
    void check() const {
        log(bce::native::object({{"kind",quote(std::string("protectedDbGuard"))},{"dbRole",quote(role)},{"phase",quote(phase)},{"eventCount",std::to_string(events.size())},{"loggingFailed",loggingFailed?"true":"false"}}));
        require(alive&&(!protect||events.empty())&&!loggingFailed,"protected database changed: role="+role+" phase="+phase+" events="+std::to_string(events.size())+" loggingFailed="+(loggingFailed?"true":"false")+(events.empty()?"":" first="+events.front()));
    }
};
class WorkingDatabase final {
public:
    explicit WorkingDatabase(AcDbDatabase& target):old_(acdbHostApplicationServices()->workingDatabase()) {acdbHostApplicationServices()->setWorkingDatabase(&target);}
    ~WorkingDatabase() noexcept {acdbHostApplicationServices()->setWorkingDatabase(old_);}
    WorkingDatabase(const WorkingDatabase&)=delete;WorkingDatabase& operator=(const WorkingDatabase&)=delete;
private:AcDbDatabase* old_;
};
// Only public identity/render traits: intentionally no source extents, explode, draw or font loads.
std::string fingerprint(AcDbDatabase& db){
    Open<AcDbBlockTableRecord> ms(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};
    ok(ms->newIterator(raw),"source iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);std::vector<std::string> roots;
    for(;!it->done();it->step()){AcDbObjectId id;ok(it->getEntityId(id),"source id");Open<AcDbEntity> e(id);
        roots.push_back(object({{"handle",quote(handle(id))},{"class",quote(std::wstring(e->isA()->name()))},
          {"owner",quote(handle(e->ownerId()))},{"layer",quote(handle(e->layerId()))},
          {"color",std::to_string(e->colorIndex())},{"linetype",quote(handle(e->linetypeId()))},
          {"visible",std::to_string(e->visibility())}}));}
    return array(roots);
}
AcDbObjectId append(AcDbObjectId owner,std::unique_ptr<AcDbEntity> entity){
    Open<AcDbBlockTableRecord> ms(owner,AcDb::kForWrite);AcDbObjectId id;ok(ms->appendAcDbEntity(id,entity.get()),"append control");entity.release()->close();return id;
}
AcDbObjectId newBlock(AcDbDatabase& db,const wchar_t* name){
    Open<AcDbBlockTable> table(db.blockTableId(),AcDb::kForWrite);auto block=std::make_unique<AcDbBlockTableRecord>();
    ok(block->setName(name),"control block name");AcDbObjectId id;ok(table->add(id,block.get()),"control block");block.release()->close();return id;
}
std::vector<AcDbObjectId> controls(AcDbDatabase& db){
    const auto ms=acdbSymUtil()->blockModelSpaceId(&db);std::vector<AcDbObjectId> ids;
    AcDbObjectId styleId;
    {Open<AcDbTextStyleTable> table(db.textStyleTableId(),AcDb::kForWrite);auto style=std::make_unique<AcDbTextStyleTableRecord>();
     ok(style->setName(L"N7_SIMPLEX_GBCBIG"),"style name");ok(style->setFileName(L"simplex.shx"),"font");ok(style->setBigFontFileName(L"gbcbig.shx"),"bigfont");
     ok(table->add(styleId,style.get()),"style append");style.release()->close();}
    ids.push_back(append(ms,std::make_unique<AcDbLine>(AcGePoint3d(1,2,0),AcGePoint3d(4,6,0))));
    ids.push_back(append(ms,std::make_unique<AcDbArc>(AcGePoint3d(10,10,0),5,0,1.5707963267948966)));
    auto numeric=std::make_unique<AcDbText>(AcGePoint3d(0,0,0),L"123.45",styleId,3);
    ok(numeric->setWidthFactor(0.75),"numeric control width");ids.push_back(append(ms,std::move(numeric)));
    ids.push_back(append(ms,std::make_unique<AcDbText>(AcGePoint3d(0,5,0),L"中文立面",styleId,3)));
    auto rotated=std::make_unique<AcDbText>(AcGePoint3d(10,5,0),L"270",styleId,3);
    ok(rotated->setRotation(1.5707963267948966),"rotation");ok(rotated->mirrorInX(true),"mirror");ids.push_back(append(ms,std::move(rotated)));
    auto mtext=std::make_unique<AcDbMText>();ok(mtext->setTextStyle(styleId),"mtext style");ok(mtext->setTextHeight(3),"mtext height");
    mtext->setContents(L"AB\\P{\\H0.5x;2}\\S7/9;");ids.push_back(append(ms,std::move(mtext)));
    auto inner=newBlock(db,L"N7_INNER"),outer=newBlock(db,L"N7_OUTER");
    append(inner,std::make_unique<AcDbText>(AcGePoint3d(1,2,0),L"nested",styleId,2));
    auto ref=std::make_unique<AcDbBlockReference>(AcGePoint3d(10,20,0),inner);ok(ref->setRotation(0.5),"block rotation");ok(ref->setScaleFactors(AcGeScale3d(2,1,1)),"nonuniform scale");append(outer,std::move(ref));
    ids.push_back(append(ms,std::make_unique<AcDbBlockReference>(AcGePoint3d(30,40,0),outer)));return ids;
}
