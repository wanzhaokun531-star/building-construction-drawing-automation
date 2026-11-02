#pragma once
#include "NativeEvidence.h"
#include <acdocman.h>
#include <aced.h>
#include <dbents.h>
#include <dbsymtb.h>
#include <dbtrans.h>
#include <acutads.h>
#include <acutmem.h>
#include <adscodes.h>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include "WriteContracts.h"

namespace bce::inplace_test {
using namespace bce::native;
inline void ok(Acad::ErrorStatus status, const char* operation) {
    require(status == Acad::eOk, std::string(operation) + ": " + std::to_string(status));
}
template<class T> class Opened final {
public:
    explicit Opened(AcDbObjectId id, AcDb::OpenMode mode = AcDb::kForRead) {
        auto* manager=id.database()->transactionManager();
        if(manager->numActiveTransactions()>0) {
            AcDbObject* object{}; ok(manager->getObject(object,id,mode),"audit transaction getObject");
            value_=T::cast(object); require(value_!=nullptr,"audit object type mismatch"); transactionOwned_=true;
        } else ok(acdbOpenObject(value_, id, mode), "open");
    }
    ~Opened() noexcept { if (value_&&!transactionOwned_) value_->close(); }
    Opened(const Opened&) = delete;
    Opened& operator=(const Opened&) = delete;
    T* operator->() const noexcept { return value_; }
    T* get() const noexcept { return value_; }
private:
    T* value_{};
    bool transactionOwned_{false};
};
inline std::string number(double value) {
    std::ostringstream out; out << std::setprecision(17) << value; return out.str();
}
inline std::string handle(AcDbObjectId id) {
    if (id.isNull()) return "";
    const AcDbHandle value = id.handle(); wchar_t buffer[32]{}; value.getIntoAsciiBuffer(buffer); return utf8(buffer);
}
inline std::string point(const AcGePoint3d& p) { return array({number(p.x), number(p.y), number(p.z)}); }
inline std::string vector(const AcGeVector3d& p) { return array({number(p.x), number(p.y), number(p.z)}); }
inline std::string name(const AcDbSymbolTableRecord& record) { AcString value; ok(record.getName(value), "symbol name"); return utf8(value.kACharPtr()); }
inline std::string entity(AcDbEntity& e) {
    AcDbObjectId plotStyle; const auto plotType=e.getPlotStyleNameId(plotStyle);
    Fields fields{{"handle", quote(handle(e.objectId()))}, {"class", quote(std::wstring(e.isA()->name()))},
        {"layer", quote(handle(e.layerId()))}, {"linetype", quote(handle(e.linetypeId()))},
        {"color", std::to_string(e.colorIndex())}, {"linetypeScale", number(e.linetypeScale())},
        {"lineWeight", std::to_string(e.lineWeight())}, {"visibility", std::to_string(e.visibility())},
        {"colorRaw",std::to_string(e.color().color())},{"transparency",std::to_string(e.transparency().serializeOut())},
        {"material",quote(handle(e.materialId()))},{"plotStyleType",std::to_string(plotType)},{"plotStyle",quote(handle(plotStyle))},
        {"visualStyle",quote(handle(e.visualStyleId()))},{"castShadows",e.castShadows()?"true":"false"},
        {"receiveShadows",e.receiveShadows()?"true":"false"},{"extensionDictionary",quote(handle(e.extensionDictionary()))}};
    if (auto* line = AcDbLine::cast(&e)) {
        fields.insert(fields.end(), {{"start", point(line->startPoint())}, {"end", point(line->endPoint())},
            {"normal", vector(line->normal())}, {"thickness", number(line->thickness())}});
    } else if (auto* text = AcDbText::cast(&e)) {
        AcString content; ok(text->textString(content), "textString");
        fields.insert(fields.end(), {{"text", quote(std::wstring(content.kACharPtr()))}, {"position", point(text->position())},
            {"alignment", point(text->alignmentPoint())}, {"height", number(text->height())}, {"rotation", number(text->rotation())},
            {"width", number(text->widthFactor())}, {"oblique", number(text->oblique())},
            {"style", quote(handle(text->textStyle()))}, {"normal", vector(text->normal())},
            {"thickness", number(text->thickness())}, {"horizontalMode", std::to_string(text->horizontalMode())},
            {"verticalMode", std::to_string(text->verticalMode())}, {"mirrorX", text->isMirroredInX() ? "true" : "false"},
            {"mirrorY", text->isMirroredInY() ? "true" : "false"}});
    } else { require(false, "UNSUPPORTED_AUDIT_ENTITY: refusing incomplete protection"); }
    return object(fields);
}
inline std::string resource(AcDbSymbolTableRecord& record) {
    Fields fields{{"handle", quote(handle(record.objectId()))}, {"name", quote(name(record))}};
    if (auto* layer = AcDbLayerTableRecord::cast(&record)) {
        fields.insert(fields.end(), {{"kind", quote(std::string("layer"))}, {"color", std::to_string(layer->color().color())},
            {"linetype", quote(handle(layer->linetypeObjectId()))}, {"lineWeight", std::to_string(layer->lineWeight())},
            {"off", layer->isOff() ? "true" : "false"}, {"frozen", layer->isFrozen() ? "true" : "false"},
            {"locked", layer->isLocked() ? "true" : "false"}, {"plottable", layer->isPlottable() ? "true" : "false"}});
    } else if (auto* style = AcDbTextStyleTableRecord::cast(&record)) {
        AcString font, bigfont; ok(style->fileName(font), "font"); ok(style->bigFontFileName(bigfont), "bigfont");
        fields.insert(fields.end(), {{"kind", quote(std::string("textstyle"))}, {"font", quote(std::wstring(font.kACharPtr()))},
            {"bigfont", quote(std::wstring(bigfont.kACharPtr()))}, {"size", number(style->textSize())},
            {"width", number(style->xScale())}, {"oblique", number(style->obliquingAngle())},
            {"vertical", style->isVertical() ? "true" : "false"}, {"flags", std::to_string(style->flagBits())}});
    } else if (auto* linetype = AcDbLinetypeTableRecord::cast(&record)) {
        std::vector<std::string> dashes;
        for (int i = 0; i < linetype->numDashes(); ++i) dashes.push_back(object({{"length", number(linetype->dashLengthAt(i))},
            {"shapeStyle", quote(handle(linetype->shapeStyleAt(i)))}, {"shapeNumber", std::to_string(linetype->shapeNumberAt(i))},
            {"shapeScale", number(linetype->shapeScaleAt(i))}, {"shapeRotation", number(linetype->shapeRotationAt(i))}}));
        fields.insert(fields.end(), {{"kind", quote(std::string("linetype"))}, {"length", number(linetype->patternLength())}, {"dashes", array(dashes)}});
    } else { require(false, "UNSUPPORTED_AUDIT_RESOURCE"); }
    return object(fields);
}
inline AcDbObjectId modelSpace(AcDbDatabase& db) {
    Opened<AcDbBlockTable> table(db.blockTableId()); AcDbObjectId id;
    ok(table->getAt(ACDB_MODEL_SPACE,id), "model space id"); return id;
}
inline std::vector<std::string> roots(AcDbDatabase& db) {
    Opened<AcDbBlockTableRecord> space(modelSpace(db));
    AcDbBlockTableRecordIterator* raw{}; ok(space->newIterator(raw), "model iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw);
    std::vector<std::string> result;
    for (; !iterator->done(); iterator->step()) {
        AcDbObjectId id; ok(iterator->getEntityId(id), "entity id"); Opened<AcDbEntity> e(id); result.push_back(entity(*e.get()));
    }
    std::sort(result.begin(), result.end()); return result;
}
inline std::vector<std::string> resources(AcDbDatabase& db) {
    std::vector<std::string> result;
    for (const auto id : {db.layerTableId(), db.textStyleTableId(), db.linetypeTableId()}) {
        Opened<AcDbSymbolTable> table(id); AcDbSymbolTableIterator* raw{}; ok(table->newIterator(raw), "resource iterator");
        std::unique_ptr<AcDbSymbolTableIterator> iterator(raw);
        for (; !iterator->done(); iterator->step()) {
            AcDbObjectId rid; ok(iterator->getRecordId(rid), "resource id");
            Opened<AcDbSymbolTableRecord> record(rid); result.push_back(resource(*record.get()));
        }
    }
    std::sort(result.begin(), result.end()); return result;
}
inline std::string sysvar(const wchar_t* nameValue) {
    resbuf value{}; require(acedGetVar(nameValue, &value) == RTNORM, "get system variable");
    if (value.restype == RTSHORT) return std::to_string(value.resval.rint);
    if (value.restype == RTLONG) return std::to_string(value.resval.rlong);
    if (value.restype == RTREAL) return number(value.resval.rreal);
    if (value.restype == RTSTR) { const auto result = quote(std::wstring(value.resval.rstring)); acutDelString(value.resval.rstring); return result; }
    require(false, "unsupported system variable type"); return {};
}
inline std::string globals(AcDbDatabase& db) {
    return object({{"INSUNITS", std::to_string(db.insunits())}, {"LTSCALE", number(db.ltscale())},
        {"CELTSCALE", number(db.celtscale())}, {"DIMSTYLE", quote(handle(db.dimstyle()))},
        {"WIPEOUTFRAME", sysvar(L"WIPEOUTFRAME")}, {"FRAME", sysvar(L"FRAME")},
        {"IMAGEFRAME", sysvar(L"IMAGEFRAME")}, {"XCLIPFRAME", sysvar(L"XCLIPFRAME")}});
}
struct Audit {
    std::string filename, dbmod{"null"}, globalsJson{"null"};
    std::vector<std::string> entities, resourceRecords;
    std::string json() const { return object({{"filename", quote(filename)}, {"DBMOD", dbmod},
        {"globals", globalsJson}, {"entities", array(entities)}, {"resources", array(resourceRecords)}}); }
    bool sameContents(const Audit& other) const { return filename == other.filename && globalsJson == other.globalsJson &&
        entities == other.entities && resourceRecords == other.resourceRecords; }
};
inline Audit capture() {
    auto* doc = acDocManager->curDocument(); require(doc && doc->database(), "current document missing");
    return {utf8(doc->fileName()), sysvar(L"DBMOD"), globals(*doc->database()), roots(*doc->database()), resources(*doc->database())};
}
inline AcDbObjectId append(AcDbDatabase& db, std::unique_ptr<AcDbEntity> e) {
    Opened<AcDbBlockTableRecord> space(modelSpace(db), AcDb::kForWrite);
    AcDbObjectId id; ok(space->appendAcDbEntity(id, e.get()), "append fixture");
    auto* owned = e.release(); ok(owned->close(), "close appended fixture"); return id;
}
inline bool recordHasHandle(const std::string& record,const std::string& h) {
    const auto prefix="{\"handle\":"+quote(h)+","; return record.compare(0,prefix.size(),prefix)==0;
}
inline std::vector<std::string> withoutHandles(const std::vector<std::string>& records,const std::vector<std::string>& handles) {
    std::vector<std::string> result;
    for(const auto& record:records) {
        if(std::none_of(handles.begin(),handles.end(),[&record](const std::string& h){return recordHasHandle(record,h);})) result.push_back(record);
    }
    return result;
}
inline AcDbObjectId auditObjectId(AcDbDatabase& db,const std::string& h) {
    require(!h.empty()&&h.find_first_not_of("0123456789abcdefABCDEF")==std::string::npos,"receipt handle invalid");
    const std::wstring wide(h.begin(),h.end()); AcDbObjectId id;
    ok(db.getAcDbObjectId(id,false,AcDbHandle(wide.c_str())),"audit resolve receipt");
    require(!id.isNull()&&!id.isErased(),"receipt object missing"); return id;
}
inline bool committedEvidenceMatches(const Audit& baseline,const Audit& after,AcDbDatabase& db,
    AcDbObjectId targetId,AcDbObjectId preservedId,AcDbObjectId oldTextId,AcDbObjectId unsavedTextId,
    const CommitReceipt& receipt) {
    if(receipt.createdEntities.size()!=1||receipt.createdResourceHandles.size()!=1||!receipt.committed) return false;
    const auto newLineId=auditObjectId(db,receipt.createdEntities.front().handle);
    const auto newLayerId=auditObjectId(db,receipt.createdResourceHandles.front());
    Opened<AcDbLine> created(newLineId); Opened<AcDbLayerTableRecord> layer(newLayerId);
    Opened<AcDbLinetypeTable> linetypes(db.linetypeTableId()); AcDbObjectId byLayer,continuous;
    ok(linetypes->getAt(L"ByLayer",byLayer),"audit ByLayer"); ok(linetypes->getAt(L"Continuous",continuous),"audit Continuous");
    // Literal, hand-derived fixture expectations. No production geometry builder or layout math.
    const bool newLineCorrect=created->startPoint()==AcGePoint3d(2000.0,0.0,0.0)&&created->endPoint()==AcGePoint3d(3000.0,0.0,0.0)&&
        created->normal()==AcGeVector3d::kZAxis&&created->thickness()==0.0&&created->layerId()==newLayerId&&created->linetypeId()==byLayer&&
        created->colorIndex()==256&&created->linetypeScale()==1.0&&created->lineWeight()==AcDb::kLnWtByLayer&&
        created->visibility()==AcDb::kVisible&&created->transparency().isByLayer()&&created->materialId()==db.byLayerMaterial()&&
        created->extensionDictionary().isNull();
    const bool newLayerCorrect=name(*layer.get())=="N2NewLayer"&&layer->linetypeObjectId()==continuous&&layer->color().colorIndex()==3&&
        layer->lineWeight()==AcDb::kLnWtByLwDefault&&!layer->isOff()&&!layer->isFrozen()&&!layer->isLocked()&&layer->isPlottable()&&
        layer->transparency().serializeOut()==0;
    Opened<AcDbLine> target(targetId),preserved(preservedId);
    bool matches=target->startPoint()==AcGePoint3d(500.0,200.0,0.0)&&target->endPoint()==AcGePoint3d(1500.0,200.0,0.0)&&
        preserved->startPoint()==AcGePoint3d(0.0,100.0,0.0)&&preserved->endPoint()==AcGePoint3d(1250.0,100.0,0.0);
    for(const auto id:{oldTextId,unsavedTextId}) { Opened<AcDbEntity> e(id); const auto value=entity(*e.get());
        matches=matches&&std::find(baseline.entities.begin(),baseline.entities.end(),value)!=baseline.entities.end(); }
    const auto targetHandle=handle(targetId);
    auto prior=std::find_if(baseline.entities.begin(),baseline.entities.end(),[&targetHandle](const std::string& r){return recordHasHandle(r,targetHandle);});
    if(prior==baseline.entities.end()) return false;
    auto expectedTarget=*prior;
    const auto replaceLiteral=[&expectedTarget](const char* oldValue,const char* newValue) {
        const auto position=expectedTarget.find(oldValue); if(position==std::string::npos) return false;
        expectedTarget.replace(position,std::char_traits<char>::length(oldValue),newValue); return true;
    };
    if(!replaceLiteral("\"start\":[0,0,0]","\"start\":[500,200,0]")||
        !replaceLiteral("\"end\":[1000,0,0]","\"end\":[1500,200,0]")||entity(*target.get())!=expectedTarget) return false;
    const bool protectedEqual=withoutHandles(baseline.entities,{targetHandle})==
        withoutHandles(after.entities,{targetHandle,receipt.createdEntities.front().handle});
    const bool resourcesEqual=baseline.resourceRecords==withoutHandles(after.resourceRecords,receipt.createdResourceHandles);
    return matches&&newLineCorrect&&newLayerCorrect&&protectedEqual&&resourcesEqual&&after.entities.size()==baseline.entities.size()+1&&
        after.resourceRecords.size()==baseline.resourceRecords.size()+1&&baseline.filename==after.filename&&baseline.globalsJson==after.globalsJson;
}
}
