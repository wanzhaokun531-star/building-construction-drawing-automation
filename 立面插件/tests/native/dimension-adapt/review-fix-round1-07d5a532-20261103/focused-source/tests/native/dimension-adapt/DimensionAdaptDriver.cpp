// N8a isolated native probe. RED deliberately leaves the real TCH seed unadapted.
#include "NativeEvidence.h"
#include "DimensionTopology.h"
#include "RejectionAcceptance.h"
#include <aced.h>
#include <adscodes.h>
#include <acutmem.h>
#include <acutads.h>
#include <acdocman.h>
#include <dbapserv.h>
#include <dbdim.h>
#include <dbents.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <dbmtext.h>
#include <rxregsvc.h>
#include <algorithm>
#include <cmath>
#include <exception>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <locale>
#include <memory>
#include <map>
#include <set>
#include <sstream>

namespace {
using namespace bce::native;
HMODULE moduleHandle{}; // Borrowed module identity; never FreeLibrary.
constexpr double tolerance = 1e-6;
void check(Acad::ErrorStatus status, const char* name) {
    require(status == Acad::eOk, std::string(name) + " status=" + std::to_string(status));
}
std::string number(double value) {
    require(std::isfinite(value), "nonfinite observed number");
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(17) << value; return out.str();
}
std::string point(const AcGePoint3d& value) { return array({number(value.x), number(value.y), number(value.z)}); }
std::string text(const wchar_t* value) { return value ? quote(std::wstring(value)) : "null"; }
struct Close { void operator()(AcDbObject* value) const noexcept { if (value) value->close(); } };
template<class T> using Opened = std::unique_ptr<T, Close>;
template<class T> Opened<T> opened(AcDbObjectId id, AcDb::OpenMode mode = AcDb::kForRead) {
    T* value{}; const auto status = acdbOpenObject(value, id, mode); Opened<T> result(value);
    check(status, "open database object"); require(value != nullptr, "null opened object"); return result;
}
class Working final {
public:
    explicit Working(AcDbDatabase* value) : previous_(acdbHostApplicationServices()->workingDatabase()) {
        acdbHostApplicationServices()->setWorkingDatabase(value);
    }
    ~Working() noexcept { acdbHostApplicationServices()->setWorkingDatabase(previous_); }
    Working(const Working&) = delete; Working& operator=(const Working&) = delete;
private: AcDbDatabase* previous_;
};
struct Exploded final {
    AcDbVoidPtrArray values;
    std::vector<std::unique_ptr<AcDbObject>> owned;
    std::vector<AcDbEntity*> accepted;
    Exploded() = default; Exploded(const Exploded&) = delete; Exploded& operator=(const Exploded&) = delete;
};
struct ObservationLog { const Config& config; Evidence& evidence; unsigned sequence{}; };
std::string handle(AcDbObjectId id) {
    if (id.isNull()) return "null";
    wchar_t value[32]{}; id.handle().getIntoAsciiBuffer(value); return text(value);
}
bool capture_returns(Exploded& exploded, const AcDbEntity* source, ObservationLog& log) {
    bool safe = true; std::set<const void*> seen; std::vector<std::string> records;
    for (int i = 0; i < exploded.values.length(); ++i) {
        auto* raw = exploded.values[i];
        const bool duplicate = raw && !seen.insert(raw).second;
        const bool self = raw == source;
        auto* object = raw && !self && !duplicate ? AcDbObject::cast(static_cast<AcRxObject*>(raw)) : nullptr;
        auto* entity = object ? AcDbEntity::cast(object) : nullptr;
        // Actual 7d6a8f4a: exact AcDbAlignedDimension returns have all three AcDb
        // ownership fields null while isPersistent is true. SDK2020
        // dbobjptr.h:564-580 and N1 use AcDb objectId/ownerId/database for lifetime.
        // Retain isPersistent as raw evidence; it does not veto this proven
        // detached AcDbObject class. Unknown types and foreign objects stay borrowed.
        const bool detached = object && object->objectId().isNull() && object->ownerId().isNull() && object->database() == nullptr;
        const bool persistent = object && object->isPersistent();
        const bool expectedType = entity && entity->isA() == AcDbAlignedDimension::desc();
        const bool owned = detached && expectedType;
        records.push_back(object ? bce::native::object({{"index", std::to_string(i)}, {"acDbObjectCast", "true"},
            {"acDbEntityCast", entity ? "true" : "false"}, {"class", text(object->isA()->name())},
            {"objectId", handle(object->objectId())}, {"ownerId", handle(object->ownerId())},
            {"databaseNull", object->database() == nullptr ? "true" : "false"},
            {"databaseMatchesSource", object->database() == source->database() ? "true" : "false"},
            {"isPersistent", persistent ? "true" : "false"},
            {"detachedByAcDbOwnership", detached ? "true" : "false"}, {"exactExpectedSegmentType", expectedType ? "true" : "false"},
            {"deleteOwned", owned ? "true" : "false"}})
            : bce::native::object({{"index", std::to_string(i)}, {"null", raw ? "false" : "true"},
                {"self", self ? "true" : "false"}, {"duplicate", duplicate ? "true" : "false"},
                {"acDbObjectCast", "false"}, {"deleteOwned", "false"}}));
        if (owned) exploded.owned.emplace_back(object);
        if (owned && entity && !self && !duplicate) exploded.accepted.push_back(entity); else safe = false;
    }
    ++log.sequence;
    const auto path = log.config.output + L"\\explode-ownership-" + std::to_wstring(log.sequence) + L".json";
    auto file = create_new(path);
    write_flushed(file.get(), object({{"contract", quote(std::string("bce.dimension-explode-ownership.v1"))},
        {"sourceClass", text(source->isA()->name())}, {"sourceId", handle(source->objectId())},
        {"returns", array(records)}, {"ownershipSafe", safe ? "true" : "false"},
        {"ownedDetachedCount", std::to_string(exploded.owned.size())}}));
    log.evidence.step("explodeOwnership" + std::to_string(log.sequence), safe ? "passed" : "failed", utf8(path));
    return safe;
}
std::string dimension_resources(const AcDbDimension& dimension);
std::string public_dimension_text(const AcDbDimension& dimension);
std::string textstyle_shape(AcDbObjectId id);
struct Segment { AcGePoint3d first, second, baseline; double measurement{}, factor{}, textHeight{}; std::wstring formatted, overrideText; AcDbObjectId layerId,dimStyleId,textStyleId; std::string json, comparisonJson,textStyleShape; };
struct Snapshot { std::vector<Segment> segments; std::vector<AcGePoint3d> nodes, grips; AcDbObjectId rootLayerId; std::string json, comparisonJson; };
bool points_close(const AcGePoint3d& first, const AcGePoint3d& second) { return first.distanceTo(second) <= tolerance; }
Snapshot observe(AcDbObjectId id, ObservationLog& log) {
    auto root = opened<AcDbEntity>(id);
    require(!root->isAProxy() && std::wstring(root->isA()->name()) == L"TDbDimension2", "root must be real TDbDimension2");
    require(root->ownerId() == acdbSymUtil()->blockModelSpaceId(root->database()), "root must be directly in model space");
    AcGePoint3dArray grips; AcDbIntArray osnap, geometry;
    const auto gripStatus = root->getGripPoints(grips, osnap, geometry); check(gripStatus, "getGripPoints");
    std::vector<std::string> gripJson;
    for (int i = 0; i < grips.length(); ++i) gripJson.push_back(point(grips[i]));
    Exploded exploded;
    log.evidence.checkpoint("before-public-dimension-explode-" + std::to_string(log.sequence + 1));
    Acad::ErrorStatus explodeStatus = Acad::eNotApplicable;
    try { explodeStatus = root->explode(exploded.values); }
    catch (...) { capture_returns(exploded, root.get(), log); throw; }
    require(capture_returns(exploded, root.get(), log), "explode returned unresolved/duplicate/self/non-detached object; foreign objects not deleted");
    check(explodeStatus, "explode real dimension");
    Snapshot result; result.rootLayerId = root->layerId();
    for (int i=0;i<grips.length();++i) result.grips.push_back(grips[i]);
    std::vector<std::string> segmentJson, comparisonSegments;
    for (auto* entity : exploded.accepted) {
        auto* dim = AcDbAlignedDimension::cast(entity); require(dim != nullptr, "explode segment not AcDbAlignedDimension");
        Segment segment; segment.first = dim->xLine1Point(); segment.second = dim->xLine2Point(); segment.baseline = dim->dimLinePoint();
        const auto measurementStatus = dim->measurement(segment.measurement); check(measurementStatus, "measurement");
        AcString overrideText, formatted, layer;
        check(dim->layer(layer), "segment layer");
        const auto textStatus = dim->dimensionText(overrideText); check(textStatus, "dimensionText");
        const auto formatStatus = dim->formatMeasurement(formatted, segment.measurement, overrideText.kACharPtr()); check(formatStatus, "formatMeasurement");
        segment.formatted = formatted.kACharPtr(); segment.overrideText = overrideText.kACharPtr();
        segment.factor = dim->dimlfac(); segment.textHeight = dim->dimtxt();
        segment.textStyleShape=textstyle_shape(dim->dimtxsty());
        segment.layerId = dim->layerId();
        segment.dimStyleId=dim->dimensionStyle();segment.textStyleId=dim->dimtxsty();
        for(const auto reference:{segment.layerId,segment.dimStyleId,segment.textStyleId})require(!reference.isNull()&&reference.database()==id.database(),"segment_public_resource_reference_foreign_or_null");
        Fields fields{{"first", point(segment.first)}, {"second", point(segment.second)}, {"baseline", point(segment.baseline)},
            {"measurement", number(segment.measurement)}, {"measurementStatus", std::to_string(measurementStatus)},
            {"formatMeasurement", text(formatted.kACharPtr())}, {"formatStatus", std::to_string(formatStatus)},
            {"dimensionText", text(overrideText.kACharPtr())}, {"dimensionTextStatus", std::to_string(textStatus)},
            {"dimlfac", number(dim->dimlfac())}, {"dimdec", std::to_string(dim->dimdec())}, {"dimscale", number(dim->dimscale())},
            {"dimtxt", number(dim->dimtxt())}, {"dimasz", number(dim->dimasz())}, {"dimexo", number(dim->dimexo())},
            {"dimexe", number(dim->dimexe())}, {"dimgap", number(dim->dimgap())}, {"dimtad", std::to_string(dim->dimtad())},
            {"dimjust", std::to_string(dim->dimjust())},{"paperTextStyle",segment.textStyleShape}};
        segment.comparisonJson = object(fields); comparisonSegments.push_back(segment.comparisonJson);
        fields.emplace_back("layer", text(layer.kACharPtr()));
        fields.emplace_back("readResources", dimension_resources(*dim));
        fields.emplace_back("publicReadableText", public_dimension_text(*dim)); segment.json = object(fields);
        result.segments.push_back(segment); segmentJson.push_back(segment.json);
        for (const auto& candidate : {segment.first, segment.second}) {
            if (std::none_of(result.nodes.begin(), result.nodes.end(), [&](const AcGePoint3d& existing) { return points_close(candidate, existing); })) result.nodes.push_back(candidate);
        }
    }
    require(!result.segments.empty() && result.nodes.size() == result.segments.size()+1, "segments must form a full node path");
    std::sort(result.nodes.begin(), result.nodes.end(), [](const AcGePoint3d& a, const AcGePoint3d& b) { return a.x==b.x ? a.y<b.y : a.x<b.x; });
    std::vector<std::string> nodes;
    for (const auto& node : result.nodes) nodes.push_back(point(node));
    result.comparisonJson = object({{"grips", array(gripJson)}, {"nodes", array(nodes)}, {"segments", array(comparisonSegments)}});
    AcGePoint3dArray stretch; const auto stretchStatus=root->getStretchPoints(stretch); std::vector<std::string> stretchJson;
    if(stretchStatus==Acad::eOk) for(int i=0;i<stretch.length();++i)stretchJson.push_back(point(stretch[i]));
    result.json = object({{"rootClass", text(root->isA()->name())}, {"proxy", "false"}, {"directModelRoot", "true"},
        {"publicAcDbDimensionCast",AcDbDimension::cast(root.get())?"true":"false"},
        {"rootLayerId", handle(root->layerId())}, {"rootColorIndex", std::to_string(root->colorIndex())},
        {"rootLinetypeId", handle(root->linetypeId())}, {"rootLinetypeScale", number(root->linetypeScale())},
        {"rootLineWeight", std::to_string(root->lineWeight())}, {"rootVisibility", std::to_string(root->visibility())},
        {"stretchStatus", std::to_string(stretchStatus)}, {"stretchPoints", array(stretchJson)},
        {"gripStatus", std::to_string(gripStatus)}, {"grips", array(gripJson)}, {"explodeStatus", std::to_string(explodeStatus)},
        {"nodes", array(nodes)}, {"segments", array(segmentJson)}, {"nativeGlyphObservation", "null"},
        {"nativeGlyphGap", quote(std::string("No host text request captured in this RED probe; formatted segment text is not a glyph observation"))}});
    return result;
}
struct LayerObservation { std::string name, definition; AcDbObjectId linetype; };
LayerObservation layer_observation(AcDbObjectId id) {
    require(!id.isNull() && id.database() != nullptr, "layer reference must resolve to persistent symbol");
    Working working(id.database()); auto layer = opened<AcDbLayerTableRecord>(id);
    AcString name; check(layer->getName(name), "layer getName");
    const auto color = layer->color();
    return {text(name.kACharPtr()), object({{"colorMethod", std::to_string(color.colorMethod())},
        {"colorIndex", std::to_string(color.colorIndex())}, {"red", std::to_string(color.red())},
        {"green", std::to_string(color.green())}, {"blue", std::to_string(color.blue())},
        {"lineWeight", std::to_string(layer->lineWeight())}, {"off", layer->isOff() ? "true" : "false"},
        {"frozen", layer->isFrozen() ? "true" : "false"}, {"locked", layer->isLocked() ? "true" : "false"},
        {"plottable", layer->isPlottable() ? "true" : "false"}}), layer->linetypeObjectId()};
}
std::string validate_layer_map(AcDbObjectId source, AcDbObjectId target, AcDbIdMapping& mapping) {
    AcDbIdPair pair; pair.setKey(source);
    require(mapping.compute(pair) && pair.value() == target, "actual clone layer mapping mismatch");
    const auto before = layer_observation(source), after = layer_observation(target);
    require(before.definition == after.definition, "mapped layer public definition mismatch");
    AcDbIdPair linetype; linetype.setKey(before.linetype);
    require(mapping.compute(linetype) && linetype.value() == after.linetype, "mapped layer linetype dependency mismatch");
    return object({{"sourceId", handle(source)}, {"targetId", handle(target)}, {"sourceName", before.name}, {"targetName", after.name},
        {"isCloned", pair.isCloned() ? "true" : "false"}, {"isOwnerTranslated", pair.isOwnerXlated() ? "true" : "false"},
        {"sourceDefinition", before.definition}, {"targetDefinition", after.definition},
        {"sourceLinetypeId", handle(before.linetype)}, {"targetLinetypeId", handle(after.linetype)}, {"linetypeMappingVerified", "true"}});
}
#include "DimensionAdaptGreen.inc"
#include "RejectionFocus.inc"
void run_case() noexcept {
    Config config; std::unique_ptr<Evidence> evidence;
    auto* originalWorking = acdbHostApplicationServices()->workingDatabase();
    auto* originalCurrent = acDocManager->curDocument(); auto* originalActive = acDocManager->mdiActiveDocument();
    try {
        config = load_config(moduleHandle); evidence = std::make_unique<Evidence>(config);
        evidence->checkpoint("before-identity-validation"); validate_identity(config);
        require(config.caseName == L"DimensionAdaptMm100Nontrivial"||config.caseName==L"DimensionAdaptSourceBindingFocused", "unsupported N8a case");
        const bool passed = config.caseName==L"DimensionAdaptSourceBindingFocused"?rejection_focus(config,*evidence):probe(config, *evidence);
        require(originalWorking == acdbHostApplicationServices()->workingDatabase() && originalCurrent == acDocManager->curDocument() && originalActive == acDocManager->mdiActiveDocument(), "ambient current/active/working changed");
        evidence->step("ambientPointers", "passed", "current document, active document and working database preserved; no globals are set");
        evidence->capability(object({{"dimensionAdapt", quote(std::string("partial"))}, {"glyphComplete", "false"}, {"capabilityUpgradeAllowed", "false"}}));
        evidence->finish(config, passed ? "partial" : "failed");
        acutPrintf(passed ? L"\nBCE_NATIVE_CASE_PARTIAL\n" : L"\nBCE_NATIVE_CASE_FAILED; inspect dimension-adapt.json\n");
    } catch (const std::exception& failure) {
        try { if (evidence) { evidence->error(failure.what()); evidence->finish(config, "failed"); } } catch (...) {}
        acutPrintf(L"\nBCE_NATIVE_CASE_FAILED; inspect native-case/checkpoints\n");
    } catch (...) {
        try { if (evidence) { evidence->error("Unknown C++ exception at command ABI boundary"); evidence->finish(config, "failed"); } } catch (...) {}
        acutPrintf(L"\nBCE_NATIVE_CASE_FAILED_UNKNOWN\n");
    }
}
}
BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { moduleHandle = instance; DisableThreadLibraryCalls(instance); } return TRUE;
}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void* id) {
    try {
        if (message == AcRx::kInitAppMsg) {
            acrxDynamicLinker->unlockApplication(id); acrxRegisterAppMDIAware(id);
            return acedRegCmds->addCommand(L"BCE_DIMENSION_ADAPT", L"BCE_TEST_RUN", L"BCE_TEST_RUN", ACRX_CMD_MODAL, run_case) == Acad::eOk ? AcRx::kRetOK : AcRx::kRetError;
        }
        if (message == AcRx::kUnloadAppMsg) return acedRegCmds->removeGroup(L"BCE_DIMENSION_ADAPT") == Acad::eOk ? AcRx::kRetOK : AcRx::kRetError;
        return AcRx::kRetOK;
    } catch (...) { return AcRx::kRetError; }
}
