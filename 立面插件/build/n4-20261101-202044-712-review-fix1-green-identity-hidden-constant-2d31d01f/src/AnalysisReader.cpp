#include "AnalysisReader.h"
#include <cmath>
#include <dbHatch.h>
#include <dbapserv.h>
#include <dbdim.h>
#include <dbents.h>
#include <dbidmap.h>
#include <dbmtext.h>
#include <dbpl.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <gearc2d.h>
#include <geell2d.h>
#include <gekvec.h>
#include <gelnsg2d.h>
#include <genurb2d.h>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
namespace bce {
namespace {
void check(Acad::ErrorStatus s, const char *op) {
    if (s != Acad::eOk)
        throw std::runtime_error(std::string(op) + ":" + std::to_string(s));
}
template <class T> class Read final {
  public:
    explicit Read(AcDbObjectId id) { check(acdbOpenObject(p_, id, AcDb::kForRead), "open read"); }
    ~Read() {
        if (p_)
            p_->close();
    }
    Read(const Read &) = delete;
    Read &operator=(const Read &) = delete;
    T *get() const { return p_; }
    T *operator->() const { return p_; }

  private:
    T *p_{};
};
class WorkingDatabaseScope final {
  public:
    explicit WorkingDatabaseScope(AcDbDatabase &database)
        : services_(acdbHostApplicationServices()), previous_(services_->workingDatabase()) {
        services_->setWorkingDatabase(&database);
    }
    ~WorkingDatabaseScope() noexcept { services_->setWorkingDatabase(previous_); }
    WorkingDatabaseScope(const WorkingDatabaseScope &) = delete;
    WorkingDatabaseScope &operator=(const WorkingDatabaseScope &) = delete;

  private:
    AcDbHostApplicationServices *services_;
    AcDbDatabase *previous_;
};
struct Canonical {
    std::ostringstream out;
    Canonical() {
        out.imbue(std::locale::classic());
        out << std::hexfloat;
    }
    void add(double n) {
        if (!std::isfinite(n))
            throw std::runtime_error("nonfinite geometry");
        out << n << ';';
    }
    void add(const std::string &s) { out << s.size() << ':' << s; }
    void add(AcGePoint3d p) {
        add(p.x);
        add(p.y);
        add(p.z);
    }
    void add(AcGeVector3d v) {
        add(v.x);
        add(v.y);
        add(v.z);
    }
    void add(AcGePoint2d p) {
        add(p.x);
        add(p.y);
    }
    void add(AcGeVector2d v) {
        add(v.x);
        add(v.y);
    }
};
Point2 xy(AcGePoint3d p) { return {p.x, p.y}; }
bool planar(AcGePoint3d p) {
    return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) && std::abs(p.z) < 1e-9;
}
bool normal(AcGeVector3d n) {
    return std::abs(n.x) < 1e-9 && std::abs(n.y) < 1e-9 && std::abs(std::abs(n.z) - 1.0) < 1e-9;
}
bool layerVisible(AcDbObjectId layer) {
    Read<AcDbLayerTableRecord> p(layer);
    return !p->isOff() && !p->isFrozen();
}
std::optional<Bounds2> bounds(const AcDbEntity &e, const AcGeMatrix3d *parentTransform = nullptr) {
    AcDbExtents b;
    const auto s = e.getGeomExtents(b);
    if (s == Acad::eNullExtents)
        return {};
    check(s, "bounds");
    if (parentTransform)
        b.transformBy(*parentTransform);
    const auto a = b.minPoint(), z = b.maxPoint();
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(z.x) || !std::isfinite(z.y))
        throw std::runtime_error("nonfinite bounds");
    return Bounds2{xy(a), xy(z)};
}
void curve(Canonical &out, const AcGeCurve2d *c, int type) {
    if (!c)
        throw std::runtime_error("null hatch curve");
    out.add(type);
    if (type == AcDbHatch::kLine) {
        const auto *p = static_cast<const AcGeLineSeg2d *>(c);
        out.add(p->startPoint());
        out.add(p->endPoint());
    } else if (type == AcDbHatch::kCirArc) {
        const auto *p = static_cast<const AcGeCircArc2d *>(c);
        out.add(p->center());
        out.add(p->radius());
        out.add(p->startAng());
        out.add(p->endAng());
        out.add(p->isClockWise());
    } else if (type == AcDbHatch::kEllArc) {
        const auto *p = static_cast<const AcGeEllipArc2d *>(c);
        out.add(p->center());
        out.add(p->majorAxis());
        out.add(p->minorAxis());
        out.add(p->majorRadius());
        out.add(p->minorRadius());
        out.add(p->startAng());
        out.add(p->endAng());
        out.add(p->isClockWise());
    } else if (type == AcDbHatch::kSpline) {
        const auto *p = static_cast<const AcGeNurbCurve2d *>(c);
        int degree{};
        Adesk::Boolean rational{}, periodic{};
        AcGeKnotVector knots;
        AcGePoint2dArray points;
        AcGeDoubleArray weights;
        p->getDefinitionData(degree, rational, periodic, knots, points, weights);
        out.add(degree);
        out.add(rational);
        out.add(periodic);
        out.add(knots.length());
        for (int i = 0; i < knots.length(); ++i)
            out.add(knots[i]);
        out.add(points.length());
        for (int i = 0; i < points.length(); ++i)
            out.add(points[i]);
        out.add(weights.length());
        for (int i = 0; i < weights.length(); ++i)
            out.add(weights[i]);
    } else
        throw std::runtime_error("unsupported hatch curve type");
}
bool geometry(AcDbEntity &e, Primitive2 &p, Canonical &out,
              const std::function<void(const char *)> &observe) {
    observe("common-getters");
    out.add(native_scene::utf8(e.isA()->name()));
    out.add(e.visibility());
    out.add(e.color().color());
    out.add(e.linetypeScale());
    out.add(e.lineWeight());
    observe("getGeomExtents");
    p.bounds = bounds(e);
    observe("type-specific-getters");
    const auto nativeType = e.isA();
    if (nativeType != AcDbLine::desc() && nativeType != AcDbText::desc() &&
        nativeType != AcDbAttribute::desc() && nativeType != AcDbAttributeDefinition::desc() &&
        nativeType != AcDbMText::desc() && nativeType != AcDbAlignedDimension::desc() &&
        nativeType != AcDbPolyline::desc() && nativeType != AcDbArc::desc() &&
        nativeType != AcDbCircle::desc() && nativeType != AcDbHatch::desc())
        return false;
    if (auto *line = AcDbLine::cast(&e)) {
        p.kind = PrimitiveKind::line;
        const auto a = line->startPoint(), b = line->endPoint();
        p.start = xy(a);
        p.end = xy(b);
        out.add(a);
        out.add(b);
        out.add(line->normal());
        out.add(line->thickness());
        return planar(a) && planar(b) && line->thickness() == 0;
    }
    if (auto *text = AcDbText::cast(&e)) {
        p.kind = PrimitiveKind::text;
        AcString s;
        check(text->textString(s), "text");
        p.text = native_scene::utf8(s.kACharPtr());
        p.textPosition = xy(text->position());
        p.textHeight = text->height();
        AcGeVector3d textDirection(std::cos(text->rotation()), std::sin(text->rotation()), 0);
        textDirection.transformBy(AcGeMatrix3d::planeToWorld(text->normal()));
        p.textRotation = std::atan2(textDirection.y, textDirection.x);
        p.textMirroredX = text->isMirroredInX() != 0;
        p.textMirroredY = (text->isMirroredInY() != 0) != (text->normal().z < 0);
        out.add(*p.text);
        out.add(text->position());
        out.add(text->alignmentPoint());
        out.add(text->normal());
        out.add(text->height());
        out.add(text->rotation());
        out.add(text->widthFactor());
        out.add(text->oblique());
        out.add(text->thickness());
        out.add(text->horizontalMode());
        out.add(text->verticalMode());
        out.add(text->isMirroredInX());
        out.add(text->isMirroredInY());
        out.add(native_scene::symbolName(text->textStyle()));
        return planar(text->position()) && normal(text->normal()) && text->thickness() == 0;
    }
    if (auto *text = AcDbMText::cast(&e)) {
        p.kind = PrimitiveKind::text;
        AcString raw, plain;
        check(text->contents(raw), "mtext contents");
        check(text->text(plain), "mtext plain");
        p.text = native_scene::utf8(plain.kACharPtr());
        p.textPosition = xy(text->location());
        p.textHeight = text->textHeight();
        p.textRotation = text->rotation();
        out.add(native_scene::utf8(raw.kACharPtr()));
        out.add(text->location());
        out.add(text->normal());
        out.add(text->direction());
        out.add(text->textHeight());
        out.add(text->width());
        out.add(text->attachment());
        out.add(native_scene::symbolName(text->textStyle()));
        return false;
    }
    if (auto *dim = AcDbAlignedDimension::cast(&e)) {
        p.kind = PrimitiveKind::dimension;
        const auto a = dim->xLine1Point(), b = dim->xLine2Point();
        p.start = xy(a);
        p.end = xy(b);
        p.textPosition = xy(dim->textPosition());
        double value{};
        // Only the mapped, database-resident sandbox dimension may evaluate its
        // measurement. A nonresident transformed dimension must not borrow the
        // ambient working database; its world measurement stays explicitly unread.
        const bool measured = dim->database() != nullptr;
        if (measured) {
            observe("dimension-sandbox-measurement");
            check(dim->measurement(value), "dimension measurement");
            p.dimensionMeasurement = value;
        }
        observe("dimension-remaining-getters");
        AcString s;
        check(dim->dimensionText(s), "dimension override");
        p.text = native_scene::utf8(s.kACharPtr());
        out.add(a);
        out.add(b);
        out.add(dim->dimLinePoint());
        out.add(dim->textPosition());
        out.add(dim->normal());
        out.add(dim->elevation());
        if (measured)
            out.add(value);
        else
            out.add(std::string("nonresident-world-measurement-unavailable"));
        out.add(*p.text);
        out.add(native_scene::symbolName(dim->dimensionStyle()));
        return measured && planar(a) && planar(b) && planar(dim->dimLinePoint()) && normal(dim->normal());
    }
    if (auto *poly = AcDbPolyline::cast(&e)) {
        p.kind = PrimitiveKind::polyline;
        out.add(poly->numVerts());
        out.add(poly->isClosed());
        out.add(poly->normal());
        out.add(poly->elevation());
        out.add(poly->thickness());
        out.add(poly->hasPlinegen());
        bool ok = normal(poly->normal()) && poly->elevation() == 0 && poly->thickness() == 0;
        for (unsigned i = 0; i < poly->numVerts(); ++i) {
            AcGePoint3d point;
            double bulge{}, sw{}, ew{};
            check(poly->getPointAt(i, point), "poly vertex");
            check(poly->getBulgeAt(i, bulge), "poly bulge");
            check(poly->getWidthsAt(i, sw, ew), "poly widths");
            out.add(point);
            out.add(bulge);
            out.add(sw);
            out.add(ew);
            ok = ok && planar(point);
            if (i == 0)
                p.start = xy(point);
            if (i + 1 == poly->numVerts())
                p.end = xy(point);
        }
        return ok;
    }
    if (auto *arc = AcDbArc::cast(&e)) {
        p.kind = PrimitiveKind::arc;
        const auto c = arc->center();
        AcGePoint3d a, b;
        check(arc->getStartPoint(a), "arc start");
        check(arc->getEndPoint(b), "arc end");
        p.start = xy(a);
        p.end = xy(b);
        double sweep = arc->endAngle() - arc->startAngle();
        if (sweep < 0)
            sweep += 2 * 3.14159265358979323846;
        if (arc->normal().z < 0)
            sweep = -sweep;
        p.arc = Arc2{xy(c), arc->radius(), std::atan2(a.y - c.y, a.x - c.x), sweep};
        out.add(c);
        out.add(arc->normal());
        out.add(arc->radius());
        out.add(arc->startAngle());
        out.add(arc->endAngle());
        out.add(arc->thickness());
        return planar(c) && normal(arc->normal()) && arc->thickness() == 0;
    }
    if (auto *circle = AcDbCircle::cast(&e)) {
        p.kind = PrimitiveKind::arc;
        p.arc = Arc2{xy(circle->center()), circle->radius(), 0,
                     circle->normal().z < 0 ? -2 * 3.14159265358979323846 : 2 * 3.14159265358979323846};
        out.add(circle->center());
        out.add(circle->normal());
        out.add(circle->radius());
        out.add(circle->thickness());
        return planar(circle->center()) && normal(circle->normal()) && circle->thickness() == 0;
    }
    if (auto *hatch = AcDbHatch::cast(&e)) {
        out.add(hatch->normal());
        out.add(hatch->elevation());
        out.add(hatch->originPoint());
        out.add(hatch->backgroundColor().color());
        out.add(hatch->isGradient());
        out.add(native_scene::utf8(hatch->patternName()));
        out.add(hatch->patternType());
        out.add(hatch->patternAngle());
        out.add(hatch->patternScale());
        out.add(hatch->patternSpace());
        out.add(hatch->patternDouble());
        out.add(hatch->hatchStyle());
        out.add(hatch->numPatternDefinitions());
        for (int i = 0; i < hatch->numPatternDefinitions(); ++i) {
            double a{}, x{}, y{}, dx{}, dy{};
            AcGeDoubleArray d;
            check(hatch->getPatternDefinitionAt(i, a, x, y, dx, dy, d), "hatch pattern");
            out.add(a);
            out.add(x);
            out.add(y);
            out.add(dx);
            out.add(dy);
            out.add(d.length());
            for (int j = 0; j < d.length(); ++j)
                out.add(d[j]);
        }
        out.add(hatch->numLoops());
        for (int i = 0; i < hatch->numLoops(); ++i) {
            Adesk::Int32 type = hatch->loopTypeAt(i);
            out.add(type);
            if (type & AcDbHatch::kPolyline) {
                AcGePoint2dArray v;
                AcGeDoubleArray b;
                check(hatch->getLoopAt(i, type, v, b), "hatch poly loop");
                out.add(v.length());
                for (int j = 0; j < v.length(); ++j)
                    out.add(v[j]);
                out.add(b.length());
                for (int j = 0; j < b.length(); ++j)
                    out.add(b[j]);
            } else {
                AcGeVoidPointerArray edges;
                AcGeIntArray types;
                check(hatch->getLoopAt(i, type, edges, types), "hatch edge loop");
                if (edges.length() != types.length())
                    throw std::runtime_error("hatch edge mismatch");
                out.add(edges.length());
                for (int j = 0; j < edges.length(); ++j)
                    curve(out, static_cast<const AcGeCurve2d *>(edges[j]), types[j]);
            }
        }
        return !hatch->isGradient() && normal(hatch->normal()) && hatch->elevation() == 0;
    }
    return false;
}
struct Reader {
    NativeAnalysis output;
    AcDbDatabase &db;
    std::set<std::string> protectedRoots;
    std::unique_ptr<AcDbDatabase> sandbox;
    std::map<std::string, std::string> sourceHandles;
    std::function<void(const char *, const EntityKey &)> observer;
    explicit Reader(AcDbDatabase &database) : db(database) {}
    std::string sourceHandle(AcDbObjectId id) const {
        const auto found = sourceHandles.find(native_scene::handle(id));
        if (found == sourceHandles.end())
            throw std::runtime_error("cloned object missing original identity mapping");
        return found->second;
    }
    EntityKey key(AcDbObjectId id) const {
        return {output.scene.databaseIdentity, sourceHandle(id), output.scene.revision};
    }
    void issue(const EntityKey &root, const std::string &path, const std::string &message) {
        protectedRoots.insert(root.handle);
        output.readIssues.push_back({"NATIVE_READ_INCOMPLETE",
                                     message + " [" + path + "]",
                                     {output.scene.documentIdentity, {}, root},
                                     true});
    }
    std::string walk(AcDbObjectId id, const EntityKey &root, const std::string &path,
                     const AcGeMatrix3d &matrix, AcDbObjectId inheritedLayer, bool ancestorVisible,
                     std::set<std::string> blocks, Primitive2 *rootPrimitive) {
        Read<AcDbEntity> e(id);
        Canonical all;
        NativeGeometryRecord record;
        record.root = root;
        record.leaf = key(id);
        record.instancePath = path;
        record.nativeType = native_scene::utf8(e->isA()->name());
        auto layer = e->layerId();
        if (native_scene::symbolName(layer) == "0" && !inheritedLayer.isNull())
            layer = inheritedLayer;
        record.effectiveVisible = ancestorVisible && e->visibility() == AcDb::kVisible && layerVisible(layer);
        if (auto *attr = AcDbAttribute::cast(e.get()))
            record.effectiveVisible = record.effectiveVisible && !attr->isInvisible();
        if (auto *definition = AcDbAttributeDefinition::cast(e.get()))
            record.effectiveVisible = record.effectiveVisible && !definition->isInvisible();
        Primitive2 p;
        p.key = key(id);
        p.layer = native_scene::symbolName(layer);
        p.sourceRootHandle = root.handle;
        p.instancePath = path;
        p.effectiveVisible = record.effectiveVisible;
        all.add(path);
        all.add(p.layer);
        all.add(record.effectiveVisible);
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                all.add(matrix(r, c));
        try {
            if (auto *block = AcDbBlockReference::cast(e.get())) {
                if (e->isA() != AcDbBlockReference::desc())
                    throw std::runtime_error("unsupported derived block/array instance");
                const auto definition = sourceHandle(block->blockTableRecord());
                all.add(definition);
                const auto next = matrix * block->blockTransform();
                for (int r = 0; r < 4; ++r)
                    for (int c = 0; c < 4; ++c)
                        all.add(next(r, c));
                if (blocks.size() > 64 || !blocks.insert(definition).second)
                    throw std::runtime_error("cyclic/deep block");
                Read<AcDbBlockTableRecord> b(block->blockTableRecord());
                AcDbBlockTableRecordIterator *raw{};
                check(b->newIterator(raw), "block iterator");
                std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);
                for (; !it->done(); it->step()) {
                    AcDbObjectId child;
                    check(it->getEntityId(child), "block child");
                    Read<AcDbEntity> ce(child);
                    if (auto *def = AcDbAttributeDefinition::cast(ce.get())) {
                        if (!def->isConstant())
                            continue;
                    }
                    all.add(walk(child, root, path + "/" + sourceHandle(child), next, layer,
                                 record.effectiveVisible, blocks, nullptr));
                }
                std::unique_ptr<AcDbObjectIterator> attrs(block->attributeIterator());
                for (; attrs && !attrs->done(); attrs->step()) {
                    const auto child = attrs->objectId();
                    all.add(walk(child, root, path + "/attrib:" + sourceHandle(child), matrix, layer,
                                 record.effectiveVisible, blocks, nullptr));
                }
                p.bounds = bounds(*block, &matrix);
                issue(root, path, "嵌套块实例另存；块内标题未接入视图识别，根实体禁止自动移动。");
            } else {
                const auto trace = [&](const char *stage) {
                    if (observer)
                        observer(stage, key(id));
                };
                Canonical local;
                Primitive2 raw;
                const bool localComplete = geometry(*e.get(), raw, local, trace);
                all.add(local.out.str());
                if (path == root.handle) {
                    p = raw;
                    p.key = key(id);
                    p.layer = native_scene::symbolName(layer);
                    p.sourceRootHandle = root.handle;
                    p.instancePath = path;
                    p.effectiveVisible = record.effectiveVisible;
                    record.complete = localComplete;
                } else {
                    trace("getTransformedCopy");
                    AcDbEntity *transformedCopy{};
                    const auto transformStatus = e->getTransformedCopy(matrix, transformedCopy);
                    std::unique_ptr<AcDbEntity> copy(transformedCopy);
                    check(transformStatus, "WCS transformed copy");
                    if (!copy)
                        throw std::runtime_error("null transformed copy");
                    Canonical world;
                    const bool worldComplete = geometry(*copy, p, world, trace);
                    record.complete = localComplete && worldComplete;
                    all.add(world.out.str());
                    if (raw.kind == PrimitiveKind::arc || raw.kind == PrimitiveKind::line ||
                        raw.kind == PrimitiveKind::text) {
                        const Affine2 affine{matrix(0, 0),
                                             matrix(0, 1),
                                             matrix(1, 0),
                                             matrix(1, 1),
                                             matrix(0, 3),
                                             matrix(1, 3),
                                             std::abs(matrix(2, 0)) < 1e-12 &&
                                                 std::abs(matrix(2, 1)) < 1e-12 &&
                                                 std::abs(matrix(2, 3)) < 1e-12};
                        const auto oriented = bce::transformed(raw, affine);
                        if (!oriented.value)
                            throw std::runtime_error("unsupported oriented WCS transform");
                        p = *oriented.value;
                    }
                }
                if (!record.complete)
                    issue(root, path, "类型、平面或完整显示数据未支持，保留原对象。");
            }
        } catch (const std::exception &failure) {
            record.complete = false;
            issue(root, path, failure.what());
            all.add(std::string("incomplete:") + failure.what());
            if (rootPrimitive && !p.bounds) {
                try {
                    p.bounds = bounds(*e.get());
                } catch (...) {
                }
            }
        }
        if (!record.effectiveVisible)
            issue(root, path, "有效可见性为隐藏；保持归属证据并禁止自动写入。");
        p.key = key(id);
        p.layer = native_scene::symbolName(layer);
        p.sourceRootHandle = root.handle;
        p.instancePath = path;
        p.effectiveVisible = record.effectiveVisible;
        p.topologyComplete = record.complete;
        p.nativeGeometrySignature = all.out.str();
        record.canonicalGeometry = p.nativeGeometrySignature;
        record.worldGeometry = p;
        output.geometry.push_back(record);
        if (rootPrimitive)
            *rootPrimitive = std::move(p);
        return record.canonicalGeometry;
    }
    NativeAnalysis run(const std::string &documentIdentity, const std::string &identity, std::uint64_t revision,
                       const AnalysisReadOptions &options) {
        auto &scene = output.scene;
        scene.databaseIdentity = identity;
        scene.documentIdentity = documentIdentity;
        scene.revision = revision;
        scene.sourceSha256 = options.sourceSha256;
        scene.units.insunits = static_cast<int>(db.insunits());
        scene.units.confirmedMmPerInputUnit = options.confirmedMmPerInputUnit;
        observer = options.diagnosticObserver;
        if (observer)
            observer("readResources", {});
        scene.resources = native_scene::readResources(db);
        AcDbObjectIdArray originalRoots;
        {
            Read<AcDbBlockTableRecord> model(acdbSymUtil()->blockModelSpaceId(&db));
            AcDbBlockTableRecordIterator *raw{};
            check(model->newIterator(raw), "model iterator");
            std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);
            for (; !it->done(); it->step()) {
                AcDbObjectId id;
                check(it->getEntityId(id), "model id");
                originalRoots.append(id);
                Read<AcDbEntity> e(id);
                ++output.rootTypeCounts[native_scene::utf8(e->isA()->name())];
            }
        }
        // Text extents can populate the source's AcDbFontTable even through a const
        // getter. Clone the current in-memory graph and all referenced resources once;
        // every geometry/font operation below is confined to this owned database.
        sandbox = std::make_unique<AcDbDatabase>(true, true);
        WorkingDatabaseScope working(*sandbox);
        if (observer)
            observer("sandbox-working-database-bound", {});
        AcDbIdMapping mapping;
        if (observer)
            observer("clone-memory-graph", {});
        const auto cloneStatus =
            originalRoots.isEmpty()
                ? Acad::eOk
                : db.wblockCloneObjects(originalRoots, acdbSymUtil()->blockModelSpaceId(sandbox.get()),
                                        mapping, AcDb::kDrcReplace);
        if (cloneStatus == Acad::eOk) {
            AcDbIdMappingIter mapped(mapping);
            for (; !mapped.done(); mapped.next()) {
                AcDbIdPair pair;
                if (mapped.getMap(pair) && !pair.value().isNull())
                    sourceHandles.emplace(native_scene::handle(pair.value()),
                                          native_scene::handle(pair.key()));
            }
            for (int i = 0; i < originalRoots.length(); ++i) {
                AcDbIdPair pair;
                pair.setKey(originalRoots[i]);
                if (!mapping.compute(pair) || pair.value().isNull())
                    throw std::runtime_error("model root missing cloned identity");
                Primitive2 p;
                const auto root = key(pair.value());
                walk(pair.value(), root, root.handle, AcGeMatrix3d::kIdentity, AcDbObjectId::kNull, true, {},
                     &p);
                scene.primitives.push_back(std::move(p));
            }
        } else {
            // A proxy or custom object can prohibit cloning. Preserve every actual
            // root and fail closed, without calling its geometry getters on the source.
            for (int i = 0; i < originalRoots.length(); ++i) {
                Read<AcDbEntity> e(originalRoots[i]);
                Primitive2 p;
                p.key = {identity, native_scene::handle(originalRoots[i]), revision};
                p.layer = native_scene::symbolName(e->layerId());
                p.sourceRootHandle = p.key.handle;
                p.instancePath = p.key.handle;
                issue(p.key, p.instancePath,
                      "独立内存图克隆失败，全部源根保留；status=" + std::to_string(cloneStatus));
                scene.primitives.push_back(std::move(p));
            }
        }
        output.recognition = analyse(scene, options.plan, options.project);
        std::set<std::string> preserved;
        for (const auto &k : output.recognition.preserved)
            preserved.insert(k.handle);
        preserved.insert(protectedRoots.begin(), protectedRoots.end());
        const double unavailable = std::numeric_limits<double>::quiet_NaN();
        for (const auto &p : scene.primitives) {
            std::string view;
            for (const auto &v : output.recognition.views)
                if (std::find(v.members.begin(), v.members.end(), p.key) != v.members.end()) {
                    view = v.identity;
                    break;
                }
            // SceneEntity predates optional bounds. Nonfinite is an explicit unavailable
            // sentinel; it can never masquerade as a zero-sized obstacle at the origin.
            SceneEntity entity{
                p.key, p.layer,
                p.bounds.value_or(Bounds2{{unavailable, unavailable}, {unavailable, unavailable}}), view};
            if (preserved.count(p.key.handle))
                scene.preserved.push_back(entity);
            else
                scene.targets.push_back(entity);
        }
        for (const auto &p : scene.primitives)
            if (protectedRoots.count(p.key.handle) &&
                std::find(output.recognition.preserved.begin(), output.recognition.preserved.end(), p.key) ==
                    output.recognition.preserved.end())
                output.recognition.preserved.push_back(p.key);
        output.recognition.issues.insert(output.recognition.issues.end(), output.readIssues.begin(),
                                         output.readIssues.end());
        return std::move(output);
    }
};
Result<NativeAnalysis> readAnalysis(AcDbDatabase &db, const std::string &documentIdentity,
                                   const std::string &databaseIdentity, std::uint64_t revision,
                                   const AnalysisReadOptions &options) {
    if (documentIdentity.empty() || databaseIdentity.empty() || revision == 0)
        return {std::nullopt, {{"INVALID_DATABASE_IDENTITY", "数据库读取须提供显式身份和版本。", {}, true}}};
    try {
        auto value = Reader(db).run(documentIdentity, databaseIdentity, revision, options);
        auto issues = value.readIssues;
        return {std::move(value), std::move(issues)};
    } catch (const std::exception &e) {
        return {std::nullopt, {{"NATIVE_READ_FAILED", e.what(), {documentIdentity, {}, {}}, true}}};
    }
}
} // namespace
Result<NativeAnalysis> readDatabaseAnalysis(AcDbDatabase &db, const std::string &identity,
                                            std::uint64_t revision, const AnalysisReadOptions &options) {
    return readAnalysis(db, identity, identity, revision, options);
}
Result<NativeAnalysis> readCurrentAnalysis(NativeSceneContext &context, const AnalysisReadOptions &options) {
    auto *db = context.currentDatabase();
    if (!db)
        return {std::nullopt, {{diagnostic::staleDocument, "当前文档上下文失效。", {}, true}}};
    const auto revision = context.revision();
    const auto documentIdentity = context.documentIdentity();
    auto result = readAnalysis(*db, documentIdentity, documentIdentity + "/database", revision, options);
    if (db != context.currentDatabase() || revision != context.revision())
        return {std::nullopt, {{diagnostic::staleDocument, "读取过程中当前文档内容变化。", {}, true}}};
    return result;
}
} // namespace bce
