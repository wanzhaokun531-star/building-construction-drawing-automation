#pragma once
#include "AnalysisReader.h"
#include "NativeEvidence.h"
#include <cmath>
#include <dbHatch.h>
#include <dbapserv.h>
#include <dbdim.h>
#include <dbents.h>
#include <dbmtext.h>
#include <dbpl.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
namespace bce::analysis_test {
using Expect = std::function<void(const std::string &, bool, const std::string &)>;
inline void ok(Acad::ErrorStatus s, const char *message) { native::require(s == Acad::eOk, message); }
template <class T> class Open final {
  public:
    Open(const Open &) = delete;
    Open &operator=(const Open &) = delete;
    Open(AcDbObjectId id, AcDb::OpenMode mode = AcDb::kForRead) {
        ok(acdbOpenObject(p, id, mode), "fixture open");
    }
    ~Open() {
        if (p)
            p->close();
    }
    T *operator->() const { return p; }
    T *p{};
};
inline AcDbObjectId append(AcDbObjectId space, std::unique_ptr<AcDbEntity> entity) {
    Open<AcDbBlockTableRecord> model(space, AcDb::kForWrite);
    AcDbObjectId id;
    ok(model->appendAcDbEntity(id, entity.get()), "fixture append");
    entity.release()->close();
    return id;
}
inline AcDbObjectId block(AcDbDatabase &db, const wchar_t *name) {
    Open<AcDbBlockTable> table(db.blockTableId(), AcDb::kForWrite);
    auto b = std::make_unique<AcDbBlockTableRecord>();
    ok(b->setName(name), "block name");
    AcDbObjectId id;
    ok(table->add(id, b.get()), "block add");
    b.release()->close();
    return id;
}
inline AcDbObjectId layer(AcDbDatabase &db, const wchar_t *name, bool off, bool frozen) {
    Open<AcDbLayerTable> table(db.layerTableId(), AcDb::kForWrite);
    auto p = std::make_unique<AcDbLayerTableRecord>();
    ok(p->setName(name), "layer name");
    p->setIsOff(off);
    ok(p->setIsFrozen(frozen), "layer frozen");
    AcDbObjectId id;
    ok(table->add(id, p.get()), "layer add");
    p.release()->close();
    return id;
}
inline bool nearValue(double a, double b) { return std::abs(a - b) < 1e-8; }
inline bool point(const std::optional<Point2> &p, double x, double y) {
    return p && nearValue(p->x, x) && nearValue(p->y, y);
}
inline const NativeGeometryRecord *leaf(const NativeAnalysis &a, AcDbObjectId root, AcDbObjectId child) {
    const auto r = native_scene::handle(root), c = native_scene::handle(child);
    for (const auto &record : a.geometry)
        if (record.root.handle == r && record.leaf.handle == c)
            return &record;
    return nullptr;
}
inline bool protectedRoot(const NativeAnalysis &a, AcDbObjectId id) {
    const auto h = native_scene::handle(id);
    return std::any_of(a.scene.preserved.begin(), a.scene.preserved.end(),
                       [&](const SceneEntity &e) { return e.key.handle == h; }) &&
           std::none_of(a.scene.targets.begin(), a.scene.targets.end(),
                        [&](const SceneEntity &e) { return e.key.handle == h; });
}
inline std::string number(double value) {
    native::require(std::isfinite(value), "nonfinite evidence number");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << value;
    return out.str();
}
inline std::string pointJson(const std::optional<Point2> &p) {
    return p ? native::array({number(p->x), number(p->y)}) : "null";
}
inline std::string worldJson(const Primitive2 &p) {
    using namespace native;
    std::string arc = "null";
    if (p.arc)
        arc = object({{"center", pointJson(p.arc->center)},
                      {"radius", number(p.arc->radius)},
                      {"start", number(p.arc->startAngle)},
                      {"sweep", number(p.arc->sweepAngle)}});
    return object({{"start", pointJson(p.start)},
                   {"end", pointJson(p.end)},
                   {"textPosition", pointJson(p.textPosition)},
                   {"arc", arc},
                   {"textRotation", p.textRotation ? number(*p.textRotation) : "null"},
                   {"mirroredX", p.textMirroredX ? "true" : "false"},
                   {"mirroredY", p.textMirroredY ? "true" : "false"}});
}
struct Monitor final : AcDbDatabaseReactor {
    explicit Monitor(AcDbDatabase &d) : db(d) { ok(db.addReactor(this), "monitor attach"); }
    ~Monitor() { db.removeReactor(this); }
    void record(const char *event, const AcDbObject *p) noexcept {
        ++changes;
        try {
            if (p && std::wstring(p->isA()->name()).find(L"AcDbFontTable") == 0)
                ++fontEvents;
            events.push_back(native::object(
                {{"event", native::quote(std::string(event))},
                 {"phase", native::quote(phase)},
                 {"readingHandle", native::quote(readingHandle)},
                 {"handle", native::quote(p ? native_scene::handle(p->objectId()) : std::string{})},
                 {"type", native::quote(p ? native_scene::utf8(p->isA()->name()) : std::string{})}}));
        } catch (...) {
            loggingFailed = true;
        }
    }
    void objectModified(const AcDbDatabase *, const AcDbObject *p) override { record("modified", p); }
    void objectOpenedForModify(const AcDbDatabase *, const AcDbObject *p) override {
        record("openedForModify", p);
    }
    void objectAppended(const AcDbDatabase *, const AcDbObject *p) override { record("appended", p); }
    void objectErased(const AcDbDatabase *, const AcDbObject *p, Adesk::Boolean) override {
        record("erased", p);
    }
    void headerSysVarChanged(const AcDbDatabase *, const ACHAR *, Adesk::Boolean success) override {
        if (success)
            record("header", nullptr);
    }
    AcDbDatabase &db;
    unsigned changes{};
    unsigned fontEvents{};
    std::string phase, readingHandle;
    std::vector<std::string> events;
    bool loggingFailed{};
};
inline std::string details(const NativeAnalysis &a) {
    using namespace native;
    std::vector<std::string> views, records, issues, preserved;
    Fields types;
    for (const auto &type : a.rootTypeCounts)
        types.push_back({type.first, std::to_string(type.second)});
    for (const auto &v : a.recognition.views) {
        std::vector<std::string> members, unconfirmed;
        for (const auto &k : v.members)
            members.push_back(quote(k.handle));
        for (const auto &i : v.unconfirmed)
            unconfirmed.push_back(object({{"code", quote(i.code)}, {"message", quote(i.message)}}));
        views.push_back(object({{"id", quote(v.identity)},
                                {"kind", quote(std::string(v.kind == ViewKind::elevation ? "elevation"
                                                           : v.kind == ViewKind::section ? "section"
                                                                                         : "ambiguous"))},
                                {"members", array(members)},
                                {"unconfirmed", array(unconfirmed)}}));
    }
    for (const auto &r : a.geometry)
        records.push_back(object({{"root", quote(r.root.handle)},
                                  {"leaf", quote(r.leaf.handle)},
                                  {"path", quote(r.instancePath)},
                                  {"type", quote(r.nativeType)},
                                  {"visible", r.effectiveVisible ? "true" : "false"},
                                  {"complete", r.complete ? "true" : "false"},
                                  {"canonicalGeometry", quote(r.canonicalGeometry)},
                                  {"world", worldJson(r.worldGeometry)}}));
    for (const auto &i : a.readIssues)
        issues.push_back(
            object({{"code", quote(i.code)},
                    {"message", quote(i.message)},
                    {"handle", quote(i.source.entity ? i.source.entity->handle : std::string{})}}));
    for (const auto &e : a.scene.preserved)
        preserved.push_back(quote(e.key.handle));
    return object({{"databaseIdentity", quote(a.scene.databaseIdentity)},
                   {"sourceSha256", quote(a.scene.sourceSha256)},
                   {"types", object(types)},
                   {"views", array(views)},
                   {"preservedRoots", array(preserved)},
                   {"geometry", array(records)},
                   {"readIssues", array(issues)}});
}
inline void synthetic(const Expect &expect, native::Evidence &evidence) {
    AcDbDatabase db(true, true);
    // Fixture setters can themselves ask the ambient font engine for metrics.
    // Keep fixture construction in its own DB. Each production read must then
    // isolate again: the synthetic source reactor below still requires zero writes.
    struct FixtureDatabaseScope {
        AcDbDatabase *prior;
        explicit FixtureDatabaseScope(AcDbDatabase &own)
            : prior(acdbHostApplicationServices()->workingDatabase()) {
            acdbHostApplicationServices()->setWorkingDatabase(&own);
        }
        ~FixtureDatabaseScope() { acdbHostApplicationServices()->setWorkingDatabase(prior); }
    } fixtureScope(db);
    const auto model = acdbSymUtil()->blockModelSpaceId(&db);
    AnalysisReadOptions options;
    options.confirmedMmPerInputUnit = 1.0;
    auto title = std::make_unique<AcDbText>(AcGePoint3d(0, -4, 0), L"合成立面图", AcDbObjectId::kNull, 1);
    append(model, std::move(title));
    auto poly = std::make_unique<AcDbPolyline>();
    ok(poly->addVertexAt(0, {0, 0}), "poly0");
    ok(poly->addVertexAt(1, {4, 3}), "poly1");
    ok(poly->addVertexAt(2, {10, 0}), "poly2");
    ok(poly->addVertexAt(3, {10, 10}), "poly3");
    ok(poly->addVertexAt(4, {0, 10}), "poly4");
    poly->setClosed(true);
    const auto polyId = append(model, std::move(poly));
    const auto first = readDatabaseAnalysis(db, "synthetic-curves", 1, options);
    native::require(first.value.has_value(), "synthetic first read failed");
    {
        Open<AcDbPolyline> p(polyId, AcDb::kForWrite);
        ok(p->setPointAt(1, {5, 3}), "internal vertex edit");
    }
    const auto changed = readDatabaseAnalysis(db, "synthetic-curves", 2, options);
    native::require(changed.value.has_value(), "synthetic changed read failed");
    const auto *before = leaf(*first.value, polyId, polyId);
    const auto *after = leaf(*changed.value, polyId, polyId);
    bool signature =
        before && after && before->complete && after->complete && before->worldGeometry.bounds &&
        after->worldGeometry.bounds &&
        nearValue(before->worldGeometry.bounds->minimum.x, after->worldGeometry.bounds->minimum.x) &&
        nearValue(before->worldGeometry.bounds->maximum.x, after->worldGeometry.bounds->maximum.x) &&
        before->canonicalGeometry != after->canonicalGeometry && first.value->recognition.views.size() == 1 &&
        changed.value->recognition.views.size() == 1 &&
        first.value->recognition.views[0].geometrySignature !=
            changed.value->recognition.views[0].geometrySignature;
    expect("SameBoundsInteriorVertexInvalidatesView", signature,
           "actual SDK polyline interior vertex changed, outer bounds unchanged");
    {
        Open<AcDbPolyline> p(polyId, AcDb::kForWrite);
        ok(p->setBulgeAt(0, 0.1), "bulge edit");
    }
    const auto bulged = readDatabaseAnalysis(db, "synthetic-curves", 3, options);
    native::require(bulged.value.has_value(), "bulge read");
    expect("PolylineBulgeInvalidatesView",
           bulged.value->recognition.views.size() == 1 &&
               changed.value->recognition.views[0].geometrySignature !=
                   bulged.value->recognition.views[0].geometrySignature,
           "full bulge retained in signature");
    const auto inner = block(db, L"N4_inner"), outer = block(db, L"N4_outer");
    const auto lineId = append(inner, std::make_unique<AcDbLine>(AcGePoint3d(1, 2, 0), AcGePoint3d(3, 4, 0)));
    const auto arcId =
        append(inner, std::make_unique<AcDbArc>(AcGePoint3d(0, 0, 0), 2, 0, 1.57079632679489661923));
    const auto textId =
        append(inner, std::make_unique<AcDbText>(AcGePoint3d(1, 2, 0), L"镜像", AcDbObjectId::kNull, 1));
    auto constant = std::make_unique<AcDbAttributeDefinition>(
        AcGePoint3d(2, 2, 0), L"隐藏常量", L"N4_CONST", L"constant");
    ok(constant->setConstant(true), "constant attribute definition");
    ok(constant->setInvisible(true), "hidden constant attribute definition");
    const auto constantId = append(inner, std::move(constant));
    auto nested = std::make_unique<AcDbBlockReference>(AcGePoint3d(10, 20, 0), inner);
    ok(nested->setRotation(1.57079632679489661923), "inner rotate");
    ok(nested->setScaleFactors(AcGeScale3d(2, 2, 2)), "inner scale");
    const auto nestedId = append(outer, std::move(nested));
    auto root = std::make_unique<AcDbBlockReference>(AcGePoint3d(100, 0, 0), outer);
    ok(root->setScaleFactors(AcGeScale3d(-1, 1, 1)), "outer mirror");
    const auto rootId = append(model, std::move(root));
    auto root2 = std::make_unique<AcDbBlockReference>(AcGePoint3d(200, 0, 0), outer);
    ok(root2->setVisibility(AcDb::kInvisible), "hide root");
    const auto root2Id = append(model, std::move(root2));
    const auto unknown = append(model, std::make_unique<AcDbPoint>(AcGePoint3d(30, 30, 0)));
    const auto tilted = append(model, std::make_unique<AcDbLine>(AcGePoint3d(1, 1, 1), AcGePoint3d(2, 2, 1)));
    auto mt = std::make_unique<AcDbMText>();
    ok(mt->setLocation({20, 20, 0}), "mtext location");
    mt->setContents(L"{\\C1;中文}\\P第二行");
    const auto mtext = append(model, std::move(mt));
    const auto circle =
        append(model, std::make_unique<AcDbCircle>(AcGePoint3d(20, 0, 0), AcGeVector3d::kZAxis, 2));
    const auto offLayer = layer(db, L"N4_off", true, false),
               frozenLayer = layer(db, L"N4_frozen", false, true);
    auto root3 = std::make_unique<AcDbBlockReference>(AcGePoint3d(300, 0, 0), outer);
    ok(root3->setLayer(offLayer), "parent layer");
    const auto root3Id = append(model, std::move(root3));
    auto frozenLine = std::make_unique<AcDbLine>(AcGePoint3d(0, 0, 0), AcGePoint3d(1, 0, 0));
    ok(frozenLine->setLayer(frozenLayer), "frozen child layer");
    const auto frozenId = append(inner, std::move(frozenLine));
    auto hatch = std::make_unique<AcDbHatch>();
    ok(hatch->setPattern(AcDbHatch::kPreDefined, L"SOLID"), "solid hatch");
    AcGePoint2dArray hv;
    AcGeDoubleArray hb;
    for (const auto &p : {AcGePoint2d(40, 0), AcGePoint2d(50, 0), AcGePoint2d(50, 10), AcGePoint2d(40, 10)}) {
        hv.append(p);
        hb.append(0);
    }
    ok(hatch->appendLoop(AcDbHatch::kExternal | AcDbHatch::kPolyline, hv, hb), "hatch exterior");
    hv.setLogicalLength(0);
    hb.setLogicalLength(0);
    for (const auto &p : {AcGePoint2d(43, 3), AcGePoint2d(43, 7), AcGePoint2d(47, 7), AcGePoint2d(47, 3)}) {
        hv.append(p);
        hb.append(0);
    }
    ok(hatch->appendLoop(AcDbHatch::kPolyline, hv, hb), "hatch hole");
    const auto hatchId = append(model, std::move(hatch));
    auto dimension = std::make_unique<AcDbAlignedDimension>();
    ok(dimension->setXLine1Point({400, 0, 0}), "dimension first point");
    ok(dimension->setXLine2Point({400, 3000, 0}), "dimension second point");
    ok(dimension->setDimLinePoint({410, 0, 0}), "dimension baseline");
    ok(dimension->setDimensionText(L"自定义<>"), "dimension override");
    const auto dimensionId = append(model, std::move(dimension));
    Monitor syntheticMonitor(db);
    const auto nestedResult = readDatabaseAnalysis(db, "synthetic-curves", 4, options);
    native::require(nestedResult.value.has_value(), "nested read");
    const auto &a = *nestedResult.value;
    expect("SyntheticReadDoesNotModifyDatabase", syntheticMonitor.changes == 0,
           "cold dimensions, blocks, curves, hatch, unknown and formatted text produce zero source write "
           "events");
    expect("IncompleteReadCannotAuthorizeCommit", !nestedResult.canCommit() && !nestedResult.issues.empty(),
           "partial value remains inspectable; blocking read diagnostics reach Result::canCommit");
    const auto *line = leaf(a, rootId, lineId);
    const auto *arc = leaf(a, rootId, arcId);
    const auto *text = leaf(a, rootId, textId);
    const auto *second = leaf(a, root2Id, lineId);
    const auto *hiddenConstant = leaf(a, rootId, constantId);
    expect("InvisibleConstantAttributeDefinitionPreserved",
           hiddenConstant && hiddenConstant->complete && !hiddenConstant->effectiveVisible &&
               !hiddenConstant->worldGeometry.effectiveVisible && protectedRoot(a, rootId),
           "constant ATTDEF is read as hidden geometry under a visible block, root remains preserved");
    const auto *nestedRecord = leaf(a, rootId, nestedId);
    const bool worldBlockBounds = nestedRecord && nestedRecord->worldGeometry.bounds &&
                                  nestedRecord->worldGeometry.bounds->minimum.x <= 94 &&
                                  nestedRecord->worldGeometry.bounds->maximum.x >= 98 &&
                                  nestedRecord->worldGeometry.bounds->minimum.y <= 22 &&
                                  nestedRecord->worldGeometry.bounds->maximum.y >= 26 &&
                                  nestedRecord->worldGeometry.bounds->minimum.x > 80;
    expect("NestedMirrorRotationWcs",
           worldBlockBounds && line && line->complete && point(line->worldGeometry.start, 94, 22) &&
               point(line->worldGeometry.end, 98, 26) && arc && arc->complete && arc->worldGeometry.arc &&
               nearValue(arc->worldGeometry.arc->center.x, 90) &&
               nearValue(arc->worldGeometry.arc->center.y, 20) &&
               nearValue(arc->worldGeometry.arc->radius, 4) &&
               nearValue(arc->worldGeometry.arc->sweepAngle, -1.57079632679489661923),
           "manual two-level WCS line endpoints 94,22 / 98,26; arc 90,20 R4 clockwise");
    expect("NestedInstanceIdentityAndVisibility",
           line && second && line->leaf.handle == second->leaf.handle &&
               line->instancePath != second->instancePath && line->effectiveVisible &&
               !second->effectiveVisible && protectedRoot(a, rootId) && protectedRoot(a, root2Id),
           "shared definition handle distinct instance paths; hidden ancestor preserved");
    const auto *inherited = leaf(a, root3Id, lineId);
    const auto *frozen = leaf(a, rootId, frozenId);
    expect("LayerZeroInheritanceOffAndFrozen",
           inherited && !inherited->effectiveVisible && inherited->worldGeometry.layer == "N4_off" &&
               frozen && !frozen->effectiveVisible && frozen->worldGeometry.layer == "N4_frozen" &&
               protectedRoot(a, root3Id),
           "layer0 inherits off parent; named frozen child remains hidden under visible parent");
    bool handed = false;
    if (text && text->worldGeometry.textRotation) {
        const auto &p = text->worldGeometry;
        const double det = (p.textMirroredX ? -1.0 : 1.0) * (p.textMirroredY ? -1.0 : 1.0);
        handed = det < 0 && point(p.textPosition, 94, 22);
    }
    expect("NestedTextChirality", handed,
           "mirrored text retains negative basis determinant and actual WCS insertion");
    const auto *cr = leaf(a, circle, circle);
    const auto *mr = leaf(a, mtext, mtext);
    expect("UnsupportedAndNonplanarPreserved",
           protectedRoot(a, unknown) && protectedRoot(a, tilted) && protectedRoot(a, mtext) && mr &&
               mr->worldGeometry.text && mr->worldGeometry.text->find(u8"中文") != std::string::npos && cr &&
               cr->complete,
           "unknown point, z=1 line and formatted mtext preserve; circle full geometry");
    const auto *h = leaf(a, hatchId, hatchId);
    expect("HatchOuterAndInnerLoopsRead", h && h->complete && !h->canonicalGeometry.empty(),
           "two actual public polyline loops (outer and hole), pattern, normal and origin retained");
    const auto *d = leaf(a, dimensionId, dimensionId);
    expect("DimensionDefinitionMeasurementAndOverride",
           d && d->complete && point(d->worldGeometry.start, 400, 0) &&
               point(d->worldGeometry.end, 400, 3000) && d->worldGeometry.dimensionMeasurement &&
               nearValue(*d->worldGeometry.dimensionMeasurement, 3000) && d->worldGeometry.text &&
               *d->worldGeometry.text == u8"自定义<>",
           "actual SDK definition points, measurement and override remain independent");
    {
        Open<AcDbHatch> edit(hatchId, AcDb::kForWrite);
        ok(edit->removeLoopAt(1), "remove hole");
        hv[0] = AcGePoint2d(44, 3);
        ok(edit->appendLoop(AcDbHatch::kPolyline, hv, hb), "modified hole");
    }
    const auto changesBeforeHoleRead = syntheticMonitor.changes;
    const auto holeChanged = readDatabaseAnalysis(db, "synthetic-curves", 5, options);
    native::require(holeChanged.value.has_value(), "changed hatch read");
    const auto *hc = leaf(*holeChanged.value, hatchId, hatchId);
    expect("HatchInnerLoopAffectsSignature",
           h && hc && h->complete && hc->complete && h->canonicalGeometry != hc->canonicalGeometry &&
               syntheticMonitor.changes == changesBeforeHoleRead,
           "inner hole changed behind identical outer envelope");
    {
        Open<AcDbAttributeDefinition> edit(constantId, AcDb::kForWrite);
        ok(edit->setInvisible(false), "show constant attribute definition");
    }
    const auto changesBeforeVisibilityRead = syntheticMonitor.changes;
    const auto shown = readDatabaseAnalysis(db, "synthetic-curves", 6, options);
    native::require(shown.value.has_value(), "constant visibility read");
    const auto *shownConstant = leaf(*shown.value, rootId, constantId);
    const auto *hiddenRoot = leaf(a, rootId, rootId);
    const auto *shownRoot = leaf(*shown.value, rootId, rootId);
    expect("ConstantAttributeVisibilityChangesDetailedSignature",
           hiddenConstant && shownConstant && hiddenRoot && shownRoot &&
               !hiddenConstant->effectiveVisible && shownConstant->effectiveVisible &&
               hiddenConstant->canonicalGeometry != shownConstant->canonicalGeometry &&
               hiddenRoot->canonicalGeometry != shownRoot->canonicalGeometry &&
               hiddenRoot->worldGeometry.nativeGeometrySignature !=
                   shownRoot->worldGeometry.nativeGeometrySignature &&
               protectedRoot(*shown.value, rootId) &&
               syntheticMonitor.changes == changesBeforeVisibilityRead,
           "only ATTDEF invisible flag toggled; leaf and aggregate root signatures change without read writes");
    evidence.capability(details(a));
}
} // namespace bce::analysis_test
