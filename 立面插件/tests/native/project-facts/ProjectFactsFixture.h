#pragma once
#include "ProjectFactsReader.h"
#include "NativeEvidence.h"
#include <acdocman.h>
#include <aced.h>
#include <adscodes.h>
#include <dbapserv.h>
#include <dbdim.h>
#include <dbents.h>
#include <dbfield.h>
#include <dbidmap.h>
#include <dbmtext.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <cmath>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
namespace bce::facts_test {
inline void ok(Acad::ErrorStatus s, const char* action) { native::require(s == Acad::eOk, std::string(action)+":"+std::to_string(s)); }
template<class T> class Open final {
public:
    explicit Open(AcDbObjectId id, AcDb::OpenMode mode=AcDb::kForRead) { ok(acdbOpenObject(value_,id,mode),"fixture open"); }
    ~Open() noexcept { if(value_) value_->close(); }
    Open(const Open&)=delete;
    Open& operator=(const Open&)=delete;
    T* operator->() const noexcept { return value_; }
private: T* value_{};
};
inline AcDbObjectId append(AcDbObjectId owner, std::unique_ptr<AcDbEntity> entity) {
    Open<AcDbBlockTableRecord> space(owner,AcDb::kForWrite); AcDbObjectId id;
    const auto detail=std::string("fixture append class=")+native_scene::utf8(entity->isA()->name())+
        " owner="+native_scene::handle(owner)+" ownerClass="+native_scene::utf8(space->isA()->name());
    const auto status=space->appendAcDbEntity(id,entity.get());
    native::require(status==Acad::eOk,detail+" status="+std::to_string(status)); entity.release()->close(); return id;
}
inline AcDbObjectId layer(AcDbDatabase& db,const wchar_t* name,bool off,bool frozen) {
    Open<AcDbLayerTable> table(db.layerTableId(),AcDb::kForWrite);
    auto value=std::make_unique<AcDbLayerTableRecord>(); ok(value->setName(name),"layer name");
    value->setIsOff(off); ok(value->setIsFrozen(frozen),"layer frozen"); AcDbObjectId id;
    ok(table->add(id,value.get()),"layer append"); value.release()->close(); return id;
}
inline AcDbObjectId block(AcDbDatabase& db) {
    Open<AcDbBlockTable> table(db.blockTableId(),AcDb::kForWrite);
    auto value=std::make_unique<AcDbBlockTableRecord>(); ok(value->setName(L"I3bNested"),"block name");
    AcDbObjectId id; ok(table->add(id,value.get()),"block append"); value.release()->close(); return id;
}
inline std::string number(double v) { std::ostringstream out; out.imbue(std::locale::classic()); out<<std::setprecision(17)<<v; return out.str(); }
inline std::string pointJson(FactPoint3 p) { return native::array({number(p.x),number(p.y),number(p.z)}); }
inline bool point(FactPoint3 p,double x,double y,double z=0) { return std::abs(p.x-x)<1e-8 && std::abs(p.y-y)<1e-8 && std::abs(p.z-z)<1e-8; }
inline std::string sourceJson(const FactSource& s) {
    using namespace native;
    std::string definition="null";
    if(const auto* p=std::get_if<FactLine>(&s.definition))
        definition=object({{"kind",quote(std::string("line"))},{"start",pointJson(p->start)},{"end",pointJson(p->end)},{"normal",pointJson(p->normal)},{"thickness",number(p->thickness)}});
    if(const auto* p=std::get_if<FactAlignedDimension>(&s.definition))
        definition=object({{"kind",quote(std::string("alignedDimension"))},{"xLine1",pointJson(p->xLine1Point)},{"xLine2",pointJson(p->xLine2Point)},{"normal",pointJson(p->normal)}});
    if(const auto* p=std::get_if<FactText>(&s.definition))
        definition=object({{"kind",quote(std::string("text"))},{"text",quote(p->text)},{"position",pointJson(p->position)},{"alignmentPoint",pointJson(p->alignmentPoint)},{"normal",pointJson(p->normal)},
            {"rotation",number(p->rotation)},{"height",number(p->height)},{"widthFactor",number(p->widthFactor)},{"oblique",number(p->oblique)},
            {"horizontalMode",std::to_string(p->horizontalMode)},{"verticalMode",std::to_string(p->verticalMode)},{"mirroredX",p->mirroredX?"true":"false"},{"mirroredY",p->mirroredY?"true":"false"},{"thickness",number(p->thickness)},{"hasFields",p->hasFields?"true":"false"}});
    return object({{"handle",quote(s.rootHandle)},{"owner",quote(s.ownerHandle)},{"class",quote(s.exactClass)},{"direct",s.directRoot?"true":"false"},{"visible",s.effectiveVisible?"true":"false"},
        {"documentIdentity",quote(s.read.documentIdentity)},{"databaseIdentity",quote(s.read.databaseIdentity)},{"revision",std::to_string(s.read.revision)},{"definition",definition}});
}
inline const FactSource* find(const Result<ProjectFactSourceRead>& r,const std::string& h) {
    if(r.value) for(const auto& s:r.value->sources) if(s.rootHandle==h) return &s;
    return nullptr;
}
inline bool issue(const Result<ProjectFactSourceRead>& r,const std::string& code,const std::string& h={}) {
    for(const auto& i:r.issues) if(i.code==code && (h.empty() || (i.source.entity && i.source.entity->handle==h))) return true;
    return false;
}
struct Monitor final : AcDbDatabaseReactor {
    explicit Monitor(AcDbDatabase& value):db(value) { ok(db.addReactor(this),"monitor attach"); }
    ~Monitor() noexcept { db.removeReactor(this); }
    void event(const char* kind,const AcDbObject* value) noexcept {
        ++changes;
        try { events.push_back(native::object({{"event",native::quote(std::string(kind))},{"handle",native::quote(value?native_scene::handle(value->objectId()):std::string{})},
            {"class",native::quote(value?native_scene::utf8(value->isA()->name()):std::string{})}})); }
        catch(...) { loggingFailed=true; }
    }
    void objectOpenedForModify(const AcDbDatabase*,const AcDbObject* p) override { event("openWrite",p); }
    void objectModified(const AcDbDatabase*,const AcDbObject* p) override { event("modified",p); }
    void objectAppended(const AcDbDatabase*,const AcDbObject* p) override { event("append",p); }
    void objectErased(const AcDbDatabase*,const AcDbObject* p,Adesk::Boolean) override { event("erase",p); }
    void objectUnAppended(const AcDbDatabase*,const AcDbObject* p) override { event("unappend",p); }
    void objectReAppended(const AcDbDatabase*,const AcDbObject* p) override { event("reappend",p); }
    void headerSysVarChanged(const AcDbDatabase*,const ACHAR*,Adesk::Boolean success) override { if(success) event("header",nullptr); }
    AcDbDatabase& db; unsigned changes{}; bool loggingFailed{}; std::vector<std::string> events;
};
inline std::string inventory(AcDbDatabase& db) {
    Open<AcDbBlockTableRecord> model(acdbSymUtil()->blockModelSpaceId(&db));
    AcDbBlockTableRecordIterator* raw{}; ok(model->newIterator(raw),"inventory iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> it(raw); std::vector<std::string> rows;
    for(;!it->done();it->step()) { AcDbObjectId id; ok(it->getEntityId(id),"inventory id"); Open<AcDbEntity> e(id);
        rows.push_back(native::object({{"handle",native::quote(native_scene::handle(id))},{"owner",native::quote(native_scene::handle(e->ownerId()))},{"class",native::quote(native_scene::utf8(e->isA()->name()))},
            {"layer",native::quote(native_scene::handle(e->layerId()))},{"visible",e->visibility()==AcDb::kVisible?"true":"false"}})); }
    return native::array(rows);
}
inline std::string resources(AcDbDatabase& db) {
    std::vector<std::string> rows;
    for(const auto& r:native_scene::readResources(db)) rows.push_back(native::object({{"kind",native::quote(r.kind)},{"name",native::quote(r.name)},{"signature",native::quote(r.definitionSignature)}}));
    return native::array(rows);
}
inline int dbmod() { resbuf v{}; native::require(acedGetVar(L"DBMOD",&v)==RTNORM && v.restype==RTSHORT,"DBMOD read"); return v.resval.rint; }
struct Boundary {
    explicit Boundary(NativeSceneContext& c,AcDbDatabase& d,Monitor& m):context(c),db(d),monitor(m),current(acDocManager->curDocument()),active(acDocManager->mdiActiveDocument()),working(acdbHostApplicationServices()->workingDatabase()),
        revision(c.revision()),changes(m.changes),mod(dbmod()),roots(inventory(d)),resource(resources(d)) {}
    bool unchanged() const { return current==acDocManager->curDocument() && active==acDocManager->mdiActiveDocument() && working==acdbHostApplicationServices()->workingDatabase() &&
        context.currentDatabase()==&db && context.revision()==revision && monitor.changes==changes && !monitor.loggingFailed && mod==dbmod() && roots==inventory(db) && resource==resources(db); }
    NativeSceneContext& context; AcDbDatabase& db; Monitor& monitor; const AcApDocument* current; const AcApDocument* active; const AcDbDatabase* working;
    std::uint64_t revision; unsigned changes; int mod; std::string roots,resource;
};
}

