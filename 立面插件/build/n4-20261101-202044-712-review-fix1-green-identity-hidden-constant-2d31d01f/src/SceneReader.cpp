#include "SceneReader.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <wincrypt.h>
#include <acdocman.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbtrans.h>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace bce {
namespace {
void check(Acad::ErrorStatus status, const char* operation) {
    if (status != Acad::eOk) throw std::runtime_error(std::string(operation) + ": " + std::to_string(status));
}
template<class T> class Read final {
public:
    explicit Read(AcDbObjectId id) { check(acdbOpenObject(value_,id,AcDb::kForRead),"read object"); }
    ~Read() noexcept { if (value_) value_->close(); }
    Read(const Read&) = delete;
    Read& operator=(const Read&) = delete;
    T* operator->() const noexcept { return value_; }
    T* get() const noexcept { return value_; }
private: T* value_{};
};
template<class T> Result<T> fail(const char* code,const std::string& message) { return {std::nullopt,{{code,message,{},true}}}; }
std::string token() {
    HCRYPTPROV provider{};
    if (!CryptAcquireContextW(&provider,nullptr,nullptr,PROV_RSA_AES,CRYPT_VERIFYCONTEXT)) throw std::runtime_error("context identity unavailable");
    struct Guard { HCRYPTPROV p; ~Guard() noexcept { CryptReleaseContext(p,0); } } guard{provider};
    BYTE bytes[16]{};
    if (!CryptGenRandom(provider,16,bytes)) throw std::runtime_error("context identity generation failed");
    std::ostringstream out; out << "native-scene-";
    for (const auto b : bytes) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return out.str();
}
std::string num(double value) {
    if (!std::isfinite(value)) throw std::runtime_error("nonfinite native value");
    std::ostringstream out; out << std::setprecision(17) << value; return out.str();
}
void field(std::string& output,const std::string& value) { output += std::to_string(value.size()) + ':' + value; }
void field(std::string& output,double value) { field(output,num(value)); }
void field(std::string& output,bool value) { field(output,std::string(value ? "true" : "false")); }
std::string recordName(const AcDbSymbolTableRecord& value) {
    AcString name; check(value.getName(name),"get symbol name"); return native_scene::utf8(name.kACharPtr());
}
}
struct NativeSceneContext::Impl final : AcApDocManagerReactor, AcDbDatabaseReactor {
    AcApDocument* document{};
    AcDbDatabase* database{};
    std::string identity{token()};
    std::uint64_t version{1};
    bool valid{true};
    Impl() {
        document = acDocManager->curDocument();
        if (!document || !document->database()) throw std::runtime_error("current document unavailable");
        database = document->database();
        check(database->addReactor(static_cast<AcDbDatabaseReactor*>(this)),"attach scene database reactor");
        acDocManager->addReactor(static_cast<AcApDocManagerReactor*>(this));
    }
    ~Impl() noexcept {
        acDocManager->removeReactor(static_cast<AcApDocManagerReactor*>(this));
        if (database) database->removeReactor(static_cast<AcDbDatabaseReactor*>(this));
    }
    void changed() noexcept { if (version == (std::numeric_limits<std::uint64_t>::max)()) valid = false; else ++version; }
    void objectAppended(const AcDbDatabase*,const AcDbObject*) override { changed(); }
    void objectUnAppended(const AcDbDatabase*,const AcDbObject*) override { changed(); }
    void objectReAppended(const AcDbDatabase*,const AcDbObject*) override { changed(); }
    void objectModified(const AcDbDatabase*,const AcDbObject*) override { changed(); }
    void objectErased(const AcDbDatabase*,const AcDbObject*,Adesk::Boolean) override { changed(); }
    void headerSysVarChanged(const AcDbDatabase*,const ACHAR*,Adesk::Boolean success) override { if (success) changed(); }
    void goodbye(const AcDbDatabase*) override { valid=false; database=nullptr; document=nullptr; }
    void documentToBeDestroyed(AcApDocument* doc) override {
        if (doc == document) { valid=false; if (database) database->removeReactor(static_cast<AcDbDatabaseReactor*>(this)); database=nullptr; document=nullptr; }
    }
    void documentBecameCurrent(AcApDocument* doc) override { if (doc != document) valid=false; }
};
NativeSceneContext::NativeSceneContext() : impl_(std::make_unique<Impl>()) {}
NativeSceneContext::~NativeSceneContext() = default;
NativeSceneContext::Impl& NativeSceneContext::native() noexcept { return *impl_; }
AcDbDatabase* NativeSceneContext::currentDatabase() const noexcept {
    return impl_->valid && acDocManager->curDocument() == impl_->document ? impl_->database : nullptr;
}
std::string NativeSceneContext::documentIdentity() const { return impl_->identity; }
std::uint64_t NativeSceneContext::revision() const noexcept { return impl_->version; }
namespace native_scene {
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    if (value.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)())) throw std::runtime_error("UTF16 too long");
    const auto n=static_cast<int>(value.size());
    const int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),n,nullptr,0,nullptr,nullptr);
    if (size<=0) throw std::runtime_error("invalid UTF16");
    std::string result(static_cast<std::size_t>(size),'\0');
    if (WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),n,result.data(),size,nullptr,nullptr)!=size) throw std::runtime_error("UTF8 conversion");
    return result;
}
std::wstring utf16(const std::string& value) {
    if (value.empty()) return {};
    if (value.find('\0') != std::string::npos || value.size()>static_cast<std::size_t>((std::numeric_limits<int>::max)())) throw std::runtime_error("invalid UTF8 symbol");
    const int n=static_cast<int>(value.size()), size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),n,nullptr,0);
    if(size<=0) throw std::runtime_error("invalid UTF8");
    std::wstring result(static_cast<std::size_t>(size),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),n,result.data(),size)!=size) throw std::runtime_error("UTF16 conversion");
    return result;
}
std::string handle(const AcDbObjectId& id) {
    if(id.isNull()) return {};
    wchar_t buffer[32]{}; id.handle().getIntoAsciiBuffer(buffer); return utf8(buffer);
}
AcDbObjectId objectId(AcDbDatabase& db,const std::string& handleText) {
    if(handleText.empty() || handleText.size()>16 || handleText.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)
        throw std::runtime_error("invalid entity handle");
    const auto wide=utf16(handleText); AcDbHandle h(wide.c_str()); AcDbObjectId id;
    check(db.getAcDbObjectId(id,false,h),"resolve handle");
    if(id.isNull() || id.isErased()) throw std::runtime_error("invalid or erased entity handle");
    return id;
}
std::string symbolName(const AcDbObjectId& id) {
    if(id.isNull()) return {};
    Read<AcDbSymbolTableRecord> record(id); return recordName(*record.get());
}
ResourceDefinition resourceDefinition(const AcDbSymbolTableRecord& record) {
    ResourceDefinition result; result.name=recordName(record); auto& signature=result.definitionSignature;
    if(const auto* layer=AcDbLayerTableRecord::cast(&record)) {
        result.kind="layer";
        field(signature,std::to_string(layer->color().color())); field(signature,symbolName(layer->linetypeObjectId()));
        field(signature,std::to_string(layer->lineWeight())); field(signature,layer->isOff()); field(signature,layer->isFrozen());
        field(signature,layer->isLocked()); field(signature,layer->isPlottable()); field(signature,std::to_string(layer->transparency().alpha()));
    } else if(const auto* style=AcDbTextStyleTableRecord::cast(&record)) {
        result.kind="textstyle"; AcString font,bigfont;
        check(style->fileName(font),"font"); check(style->bigFontFileName(bigfont),"bigfont");
        field(signature,utf8(font.kACharPtr())); field(signature,utf8(bigfont.kACharPtr())); field(signature,style->textSize());
        field(signature,style->xScale()); field(signature,style->obliquingAngle()); field(signature,std::to_string(style->flagBits()));
        field(signature,style->isVertical()!=0); field(signature,style->isShapeFile()!=0); field(signature,style->priorSize());
    } else if(const auto* line=AcDbLinetypeTableRecord::cast(&record)) {
        result.kind="linetype"; field(signature,line->patternLength()); field(signature,line->isScaledToFit());
        field(signature,std::to_string(line->numDashes()));
        for(int i=0;i<line->numDashes();++i) {
            AcString text; const auto textStatus=line->textAt(i,text);
            // SDK2020 acadstrc.h: 2 is eNotApplicable, not invalid input. A plain dash has
            // neither a shape/text style nor a shape number; only that exact case permits no text.
            const bool plainDash=textStatus==Acad::eNotApplicable && line->shapeStyleAt(i).isNull() &&
                line->shapeNumberAt(i)==0 && text.isEmpty();
            if(!plainDash) check(textStatus,"linetype text");
            field(signature,std::to_string(textStatus));
            field(signature,line->dashLengthAt(i)); field(signature,symbolName(line->shapeStyleAt(i)));
            field(signature,std::to_string(line->shapeNumberAt(i))); field(signature,line->shapeScaleAt(i));
            field(signature,line->shapeRotationAt(i)); field(signature,line->shapeOffsetAt(i).x); field(signature,line->shapeOffsetAt(i).y);
            field(signature,line->shapeIsUcsOrientedAt(i)); field(signature,line->shapeIsUprightAt(i)); field(signature,utf8(text.kACharPtr()));
        }
    } else throw std::runtime_error("unsupported resource record");
    return result;
}
std::vector<ResourceDefinition> readResources(AcDbDatabase& db) {
    std::vector<ResourceDefinition> result;
    for(const auto id:{db.layerTableId(),db.textStyleTableId(),db.linetypeTableId()}) {
        Read<AcDbSymbolTable> table(id); AcDbSymbolTableIterator* raw{}; check(table->newIterator(raw),"resource iterator");
        std::unique_ptr<AcDbSymbolTableIterator> iterator(raw);
        for(;!iterator->done();iterator->step()) { AcDbObjectId rid; check(iterator->getRecordId(rid),"resource id");
            Read<AcDbSymbolTableRecord> record(rid); result.push_back(resourceDefinition(*record.get())); }
    }
    return result;
}
}
Result<SceneSnapshot> readScene(const SceneRequest& request) {
    try {
        auto* db=request.context.currentDatabase();
        if(!db) return fail<SceneSnapshot>(diagnostic::staleDocument,"Document was destroyed, switched, or belongs to another context");
        if(acDocManager->curDocument()->isReadOnly()) return fail<SceneSnapshot>("READ_ONLY_DOCUMENT","Current document is read-only");
        SceneSnapshot scene; scene.documentIdentity=request.context.documentIdentity(); scene.databaseIdentity=scene.documentIdentity+"/database";
        scene.revision=request.context.revision(); scene.units.insunits=static_cast<int>(db->insunits());
        scene.units.confirmedMmPerInputUnit=request.confirmedMmPerInputUnit;
        scene.resources=native_scene::readResources(*db);
        std::set<std::string> targets;
        for(const auto& key:request.targetHandles) targets.insert(native_scene::handle(native_scene::objectId(*db,key)));
        Read<AcDbBlockTable> table(db->blockTableId()); AcDbObjectId ms; check(table->getAt(ACDB_MODEL_SPACE,ms),"model space");
        Read<AcDbBlockTableRecord> space(ms); AcDbBlockTableRecordIterator* raw{}; check(space->newIterator(raw),"model iterator");
        std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw);
        std::set<std::string> found;
        for(;!iterator->done();iterator->step()) {
            AcDbObjectId id; check(iterator->getEntityId(id),"entity id"); Read<AcDbEntity> e(id);
            const auto h=native_scene::handle(id); const EntityKey key{scene.databaseIdentity,h,scene.revision};
            Primitive2 primitive; primitive.key=key; primitive.layer=native_scene::symbolName(e->layerId());
            Bounds2 bounds{};
            if(const auto* line=AcDbLine::cast(e.get())) {
                const auto a=line->startPoint(), b=line->endPoint();
                if(a.z!=0.0 || b.z!=0.0) return fail<SceneSnapshot>(diagnostic::unsupportedNativeWrite,"Only planar WCS Line snapshots are supported");
                primitive.kind=PrimitiveKind::line; primitive.start=Point2{a.x,a.y}; primitive.end=Point2{b.x,b.y}; primitive.topologyComplete=true;
                bounds={{(std::min)(a.x,b.x),(std::min)(a.y,b.y)},{(std::max)(a.x,b.x),(std::max)(a.y,b.y)}};
            } else if(const auto* text=AcDbText::cast(e.get())) {
                AcString content; check(text->textString(content),"text content"); const auto position=text->position();
                primitive.kind=PrimitiveKind::text; primitive.text=native_scene::utf8(content.kACharPtr());
                primitive.textPosition=Point2{position.x,position.y}; primitive.textHeight=text->height(); primitive.textRotation=text->rotation();
                primitive.textMirroredX=text->isMirroredInX()!=0; primitive.textMirroredY=text->isMirroredInY()!=0;
                AcDbExtents extents; check(text->getGeomExtents(extents),"text bounds");
                bounds={{extents.minPoint().x,extents.minPoint().y},{extents.maxPoint().x,extents.maxPoint().y}};
            } else return fail<SceneSnapshot>(diagnostic::unsupportedNativeWrite,"Unsupported native entity prevents complete current-scene protection");
            primitive.bounds=bounds; scene.primitives.push_back(std::move(primitive));
            SceneEntity entity{key,native_scene::symbolName(e->layerId()),bounds,""};
            if(targets.count(h)) { found.insert(h); scene.targets.push_back(std::move(entity)); }
            else scene.preserved.push_back(std::move(entity));
        }
        if(found!=targets) return fail<SceneSnapshot>(diagnostic::outsideWriteScope,"Explicit target is not a current model-space root");
        for(auto& resource:scene.resources) resource.referencedByPreserved=!scene.preserved.empty();
        if(scene.revision!=request.context.revision()) return fail<SceneSnapshot>(diagnostic::staleDocument,"Scene changed while reading");
        return {std::move(scene),{}};
    } catch(const std::exception& ex) { return fail<SceneSnapshot>(diagnostic::unsupportedNativeWrite,ex.what()); }
}
}
