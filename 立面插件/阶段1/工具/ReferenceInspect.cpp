// AutoCAD 2020 / R23.1 x64, v141, C++17. Read-only SideDB inspection only.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <wincrypt.h>
#include <aced.h>
#include <acutads.h>
#include <dbapserv.h>
#include <dbdim.h>
#include <dbents.h>
#include <dbhatch.h>
#include <dbmtext.h>
#include <dbpl.h>
#include <dbsymtb.h>
#include <gearc2d.h>
#include <gelnsg2d.h>
#include <geell2d.h>
#include <genurb2d.h>
#include <rxregsvc.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "ReferenceCapturePolicy.h"
#pragma comment(lib, "Advapi32.lib")

namespace {
HMODULE module_handle{};
constexpr int maximum_depth = 8;
constexpr std::size_t maximum_nodes = 4096;
constexpr std::size_t maximum_roots = 100000;
constexpr int maximum_vertices = 65536;
using Fields = std::vector<std::pair<std::string, std::string>>;
using Errors = std::vector<std::string>;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void check(Acad::ErrorStatus status, const char* operation) {
    if (status != Acad::eOk) throw std::runtime_error(std::string(operation) + ": " + std::to_string(status));
}
std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    require(size > 0, "UTF-8 conversion failed");
    std::string result(static_cast<std::size_t>(size), '\0');
    require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
        static_cast<int>(value.size()), &result[0], size, nullptr, nullptr) == size, "UTF-8 conversion failed");
    return result;
}
std::string quoted_bytes(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << c;
        else if (c < 32) out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << int(c) << std::dec;
        else out << c;
    }
    out << '"';
    return out.str();
}
std::string quote(const std::wstring& value) { return quoted_bytes(utf8(value)); }
std::string quote(const wchar_t* value) { return value ? quote(std::wstring(value)) : "null"; }
std::string boolean(bool value) { return value ? "true" : "false"; }
std::string number(double value) {
    if (!std::isfinite(value)) throw std::runtime_error("nonfinite public numeric value");
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17) << value;
    return out.str();
}
std::string object(const Fields& fields) {
    std::string result = "{";
    for (const auto& field : fields) {
        if (result.size() > 1) result += ',';
        result += quoted_bytes(field.first) + ':' + field.second;
    }
    return result + '}';
}
std::string array(const std::vector<std::string>& values) {
    std::string result = "[";
    for (const auto& value : values) { if (result.size() > 1) result += ','; result += value; }
    return result + ']';
}
std::string errors_json(const Errors& errors) {
    std::vector<std::string> values;
    for (const auto& error : errors) values.push_back(quoted_bytes(error));
    return array(values);
}
void status_error(Errors& errors, const char* field, Acad::ErrorStatus status) {
    if (status != Acad::eOk) errors.push_back(std::string(field) + ": " + std::to_string(status));
}
std::string point(const AcGePoint3d& p) { return '[' + number(p.x) + ',' + number(p.y) + ',' + number(p.z) + ']'; }
std::string point(const AcGePoint2d& p) { return '[' + number(p.x) + ',' + number(p.y) + ']'; }
std::string vector(const AcGeVector3d& p) { return '[' + number(p.x) + ',' + number(p.y) + ',' + number(p.z) + ']'; }
std::string vector(const AcGeVector2d& p) { return '[' + number(p.x) + ',' + number(p.y) + ']'; }
std::string handle(AcDbObjectId id) {
    if (id.isNull()) return "null";
    wchar_t value[32]{};
    id.handle().getIntoAsciiBuffer(value);
    return quote(value);
}
struct CloseObject { void operator()(AcDbObject* value) const noexcept { if (value) value->close(); } };
template<class T> using Opened = std::unique_ptr<T, CloseObject>;
template<class T> Opened<T> open_read(AcDbObjectId id) {
    T* value{};
    const auto status = acdbOpenObject(value, id, AcDb::kForRead);
    Opened<T> result(value);
    check(status, "open object for read");
    require(value != nullptr, "read open returned null");
    return result;
}
class WorkingDatabaseScope {
public:
    explicit WorkingDatabaseScope(AcDbDatabase& db)
        : services_(acdbHostApplicationServices()), previous_(services_->workingDatabase()) {
        services_->setWorkingDatabase(&db);
    }
    ~WorkingDatabaseScope() noexcept { services_->setWorkingDatabase(previous_); }
    WorkingDatabaseScope(const WorkingDatabaseScope&) = delete;
    WorkingDatabaseScope& operator=(const WorkingDatabaseScope&) = delete;
private:
    AcDbHostApplicationServices* services_;
    AcDbDatabase* previous_;
};
std::string symbol_reference(AcDbObjectId id, Errors& errors) {
    if (id.isNull()) return "null";
    Fields fields{{"handle", handle(id)}};
    try {
        auto record = open_read<AcDbSymbolTableRecord>(id);
        AcString name;
        const auto status = record->getName(name);
        fields.emplace_back("nameStatus", std::to_string(status));
        fields.emplace_back("name", status == Acad::eOk ? quote(name.kACharPtr()) : "null");
        status_error(errors, "symbol name", status);
    } catch (const std::exception& failure) {
        errors.push_back(failure.what()); fields.emplace_back("error", quoted_bytes(failure.what()));
    }
    return object(fields);
}
std::string text_style(AcDbObjectId id, Errors& errors) {
    Fields fields{{"reference", symbol_reference(id, errors)}, {"fontRenderingVerified", "false"}};
    if (id.isNull()) { errors.push_back("text style reference is null"); return object(fields); }
    try {
        auto style = open_read<AcDbTextStyleTableRecord>(id);
        AcString file, big, family;
        const auto file_status = style->fileName(file);
        const auto big_status = style->bigFontFileName(big);
        bool bold{}, italic{};
        Charset charset{};
        Autodesk::AutoCAD::PAL::FontUtils::FontPitch pitch{};
        Autodesk::AutoCAD::PAL::FontUtils::FontFamily font_family{};
        const auto font_status = style->font(family, bold, italic, charset, pitch, font_family);
        status_error(errors, "text style fileName", file_status);
        status_error(errors, "text style bigFontFileName", big_status);
        status_error(errors, "text style font", font_status);
        fields.insert(fields.end(), {{"fontStatus", std::to_string(font_status)},
            {"font", file_status == Acad::eOk ? quote(file.kACharPtr()) : "null"},
            {"fontFileStatus", std::to_string(file_status)},
            {"bigFont", big_status == Acad::eOk ? quote(big.kACharPtr()) : "null"},
            {"bigFontStatus", std::to_string(big_status)},
            {"fontFamily", font_status == Acad::eOk ? quote(family.kACharPtr()) : "null"},
            {"bold", boolean(bold)}, {"italic", boolean(italic)},
            {"charset", std::to_string(static_cast<int>(charset))},
            {"pitch", std::to_string(static_cast<int>(pitch))},
            {"familyClassification", std::to_string(static_cast<int>(font_family))},
            {"size", number(style->textSize())}, {"xScale", number(style->xScale())}});
    } catch (const std::exception& failure) { errors.push_back(failure.what()); }
    return object(fields);
}
std::string common(const AcDbEntity& entity, Errors& errors) {
    AcDbExtents extents;
    const auto status = entity.getGeomExtents(extents);
    status_error(errors, "bounds", status);
    const auto color = entity.color();
    return object({{"handle", handle(entity.objectId())}, {"type", quote(entity.isA()->name())},
        {"layer", symbol_reference(entity.layerId(), errors)},
        {"bounds", object({{"status", std::to_string(status)},
            {"min", status == Acad::eOk ? point(extents.minPoint()) : "null"},
            {"max", status == Acad::eOk ? point(extents.maxPoint()) : "null"}})},
        {"color", object({{"method", std::to_string(static_cast<int>(color.colorMethod()))},
            {"index", std::to_string(color.colorIndex())}, {"value", std::to_string(color.color())},
            {"red", std::to_string(color.red())}, {"green", std::to_string(color.green())},
            {"blue", std::to_string(color.blue())}})},
        {"linetype", symbol_reference(entity.linetypeId(), errors)},
        {"linetypeScale", number(entity.linetypeScale())},
        {"lineWeight", std::to_string(static_cast<int>(entity.lineWeight()))},
        {"visibility", std::to_string(static_cast<int>(entity.visibility()))},
        {"proxy", boolean(entity.isAProxy())}});
}
Fields definition(const AcDbDimension& dim, Errors& errors) {
    Fields fields{{"normal", vector(dim.normal())}, {"elevation", number(dim.elevation())},
        {"textRotation", number(dim.textRotation())}, {"horizontalRotation", number(dim.horizontalRotation())},
        {"usingDefaultTextPosition", boolean(dim.isUsingDefaultTextPosition())},
        {"dimBlockPosition", point(dim.dimBlockPosition())}, {"dimBlockId", handle(dim.dimBlockId())}};
    if (const auto* d = AcDbAlignedDimension::cast(&dim)) fields.insert(fields.end(), {
        {"xLine1Point", point(d->xLine1Point())}, {"xLine2Point", point(d->xLine2Point())},
        {"dimLinePoint", point(d->dimLinePoint())}, {"oblique", number(d->oblique())}});
    else if (const auto* d = AcDbRotatedDimension::cast(&dim)) fields.insert(fields.end(), {
        {"xLine1Point", point(d->xLine1Point())}, {"xLine2Point", point(d->xLine2Point())},
        {"dimLinePoint", point(d->dimLinePoint())}, {"rotation", number(d->rotation())}, {"oblique", number(d->oblique())}});
    else if (const auto* d = AcDb3PointAngularDimension::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d->centerPoint())}, {"xLine1Point", point(d->xLine1Point())},
        {"xLine2Point", point(d->xLine2Point())}, {"arcPoint", point(d->arcPoint())}});
    else if (const auto* d = AcDb2LineAngularDimension::cast(&dim)) fields.insert(fields.end(), {
        {"xLine1Start", point(d->xLine1Start())}, {"xLine1End", point(d->xLine1End())},
        {"xLine2Start", point(d->xLine2Start())}, {"xLine2End", point(d->xLine2End())}, {"arcPoint", point(d->arcPoint())}});
    else if (const auto* d = AcDbRadialDimension::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d->center())}, {"chordPoint", point(d->chordPoint())}, {"leaderLength", number(d->leaderLength())}});
    else if (const auto* d = AcDbDiametricDimension::cast(&dim)) fields.insert(fields.end(), {
        {"chordPoint", point(d->chordPoint())}, {"farChordPoint", point(d->farChordPoint())}, {"leaderLength", number(d->leaderLength())}});
    else if (const auto* d = AcDbOrdinateDimension::cast(&dim)) fields.insert(fields.end(), {
        {"origin", point(d->origin())}, {"definingPoint", point(d->definingPoint())},
        {"leaderEndPoint", point(d->leaderEndPoint())}, {"usingXAxis", boolean(d->isUsingXAxis())}});
    else if (const auto* d = AcDbArcDimension::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d->centerPoint())}, {"xLine1Point", point(d->xLine1Point())},
        {"xLine2Point", point(d->xLine2Point())}, {"arcPoint", point(d->arcPoint())}});
    else if (const auto* d = AcDbRadialDimensionLarge::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d->center())}, {"chordPoint", point(d->chordPoint())},
        {"overrideCenter", point(d->overrideCenter())}, {"jogPoint", point(d->jogPoint())}, {"jogAngle", number(d->jogAngle())}});
    else errors.push_back("dimension subclass defining points unsupported");
    return fields;
}
std::string dimension_style(const AcDbDimension& dim, Errors& errors) {
    return object({{"reference", symbol_reference(dim.dimensionStyle(), errors)},
        {"effectiveValues", object({{"dimscale", number(dim.dimscale())}, {"dimtxt", number(dim.dimtxt())},
            {"dimexo", number(dim.dimexo())}, {"dimexe", number(dim.dimexe())}, {"dimgap", number(dim.dimgap())},
            {"dimlfac", number(dim.dimlfac())}, {"dimdec", std::to_string(dim.dimdec())},
            {"dimlunit", std::to_string(dim.dimlunit())}, {"dimzin", std::to_string(dim.dimzin())},
            {"dimrnd", number(dim.dimrnd())}, {"dimasz", number(dim.dimasz())}, {"dimtsz", number(dim.dimtsz())},
            {"dimtad", std::to_string(dim.dimtad())}, {"dimjust", std::to_string(dim.dimjust())},
            {"dimtmove", std::to_string(dim.dimtmove())}, {"dimpost", quote(dim.dimpost())},
            {"dimalt", boolean(dim.dimalt())}, {"dimaltf", number(dim.dimaltf())},
            {"dimtol", boolean(dim.dimtol())}, {"dimtp", number(dim.dimtp())}, {"dimtm", number(dim.dimtm())},
            {"textStyle", text_style(dim.dimtxsty(), errors)}})}});
}

// Pure getters only: this function never calls explode or dimension measurement.
// It is reused for the public model fingerprint to avoid using a recomputed display as its baseline.
Fields leaf_fields(const AcDbEntity& entity, Errors& errors, bool& known) {
    known = true;
    if (const auto* text = AcDbText::cast(&entity)) {
        AcString value;
        const auto status = text->textString(value);
        status_error(errors, "textString", status);
        return {{"kind", "\"text\""}, {"plainText", status == Acad::eOk ? quote(value.kACharPtr()) : "null"},
            {"rawText", status == Acad::eOk ? quote(value.kACharPtr()) : "null"}, {"textStatus", std::to_string(status)},
            {"position", point(text->position())}, {"alignmentPoint", point(text->alignmentPoint())},
            {"height", number(text->height())}, {"widthFactor", number(text->widthFactor())},
            {"rotation", number(text->rotation())}, {"oblique", number(text->oblique())},
            {"normal", vector(text->normal())}, {"horizontalMode", std::to_string(static_cast<int>(text->horizontalMode()))},
            {"verticalMode", std::to_string(static_cast<int>(text->verticalMode()))},
            {"mirroredX", boolean(text->isMirroredInX())}, {"mirroredY", boolean(text->isMirroredInY())},
            {"textStyle", text_style(text->textStyle(), errors)}};
    }
    if (const auto* text = AcDbMText::cast(&entity)) {
        AcString raw, plain;
        const auto raw_status = text->contents(raw), plain_status = text->text(plain);
        status_error(errors, "MText contents", raw_status); status_error(errors, "MText text", plain_status);
        AcGePoint3dArray bounds;
        text->getBoundingPoints(bounds);
        std::vector<std::string> corners;
        for (int i = 0; i < bounds.length(); ++i) corners.push_back(point(bounds[i]));
        return {{"kind", "\"mtext\""}, {"rawText", raw_status == Acad::eOk ? quote(raw.kACharPtr()) : "null"},
            {"plainText", plain_status == Acad::eOk ? quote(plain.kACharPtr()) : "null"},
            {"rawStatus", std::to_string(raw_status)}, {"plainStatus", std::to_string(plain_status)},
            {"position", point(text->location())}, {"height", number(text->textHeight())},
            {"width", number(text->width())}, {"actualWidth", number(text->actualWidth())},
            {"actualHeight", number(text->actualHeight())}, {"rotation", number(text->rotation())},
            {"normal", vector(text->normal())}, {"direction", vector(text->direction())},
            {"attachment", std::to_string(static_cast<int>(text->attachment()))},
            {"boundingPoints", array(corners)}, {"textStyle", text_style(text->textStyle(), errors)}};
    }
    if (const auto* line = AcDbLine::cast(&entity)) return {{"kind", "\"line\""},
        {"start", point(line->startPoint())}, {"end", point(line->endPoint())}, {"normal", vector(line->normal())}};
    if (const auto* arc = AcDbArc::cast(&entity)) return {{"kind", "\"arc\""},
        {"center", point(arc->center())}, {"radius", number(arc->radius())},
        {"startAngle", number(arc->startAngle())}, {"endAngle", number(arc->endAngle())}, {"normal", vector(arc->normal())}};
    if (const auto* circle = AcDbCircle::cast(&entity)) return {{"kind", "\"circle\""},
        {"center", point(circle->center())}, {"radius", number(circle->radius())}, {"normal", vector(circle->normal())}};
    if (const auto* polyline = AcDbPolyline::cast(&entity)) {
        require(polyline->numVerts() <= maximum_vertices, "polyline vertex budget exceeded");
        std::vector<std::string> vertices;
        for (unsigned int i = 0; i < polyline->numVerts(); ++i) {
            AcGePoint3d position; double start{}, end{}, bulge{};
            check(polyline->getPointAt(i, position), "polyline vertex");
            check(polyline->getWidthsAt(i, start, end), "polyline widths");
            check(polyline->getBulgeAt(i, bulge), "polyline bulge");
            vertices.push_back(object({{"position", point(position)}, {"startWidth", number(start)},
                {"endWidth", number(end)}, {"bulge", number(bulge)}}));
        }
        return {{"kind", "\"polyline\""}, {"vertices", array(vertices)},
            {"closed", boolean(polyline->isClosed())}, {"normal", vector(polyline->normal())}, {"elevation", number(polyline->elevation())}};
    }
    if (const auto* solid = AcDbSolid::cast(&entity)) {
        std::vector<std::string> vertices;
        for (int i = 0; i < 4; ++i) { AcGePoint3d p; check(solid->getPointAt(i, p), "solid vertex"); vertices.push_back(point(p)); }
        return {{"kind", "\"solid\""}, {"vertices", array(vertices)}, {"normal", vector(solid->normal())}};
    }
    if (const auto* dim = AcDbDimension::cast(&entity)) {
        AcString override_text; const auto status = dim->dimensionText(override_text);
        status_error(errors, "dimensionText", status);
        return {{"kind", "\"dimension\""}, {"override", object({{"status", std::to_string(status)},
            {"text", status == Acad::eOk ? quote(override_text.kACharPtr()) : "null"}})},
            {"textPosition", point(dim->textPosition())}, {"definition", object(definition(*dim, errors))},
            {"style", dimension_style(*dim, errors)}};
    }
    if (const auto* block = AcDbBlockReference::cast(&entity)) {
        std::vector<std::string> rows;
        const auto matrix = block->blockTransform();
        for (int i = 0; i < 4; ++i) { std::vector<std::string> row; for (int j = 0; j < 4; ++j) row.push_back(number(matrix(i, j))); rows.push_back(array(row)); }
        return {{"kind", "\"block\""}, {"block", symbol_reference(block->blockTableRecord(), errors)},
            {"transform", array(rows)}, {"position", point(block->position())},
            {"explodedCoordinates", "\"Public explode result is already transformed; no second transform is applied\""}};
    }
    if (const auto* hatch = AcDbHatch::cast(&entity)) {
        require(hatch->numLoops() <= maximum_vertices, "hatch loop budget exceeded");
        bce_capture::LeafBudget hatch_budget(static_cast<std::size_t>(maximum_vertices));
        const int pattern_definition_count = hatch->numPatternDefinitions();
        require(pattern_definition_count >= 0 && hatch_budget.try_consume(static_cast<std::size_t>(pattern_definition_count)),
            "hatch whole-leaf pattern-definition budget exceeded");
        std::vector<std::string> pattern_definitions;
        for (int i = 0; i < pattern_definition_count; ++i) {
            double angle{}, base_x{}, base_y{}, offset_x{}, offset_y{};
            AcGeDoubleArray dashes;
            const auto status = hatch->getPatternDefinitionAt(i, angle, base_x, base_y, offset_x, offset_y, dashes);
            status_error(errors, "hatch pattern definition", status);
            require(hatch_budget.try_consume(static_cast<std::size_t>(dashes.length())), "hatch whole-leaf pattern-dash budget exceeded");
            std::vector<std::string> dash_values;
            if (status == Acad::eOk) for (int j = 0; j < dashes.length(); ++j) dash_values.push_back(number(dashes[j]));
            pattern_definitions.push_back(object({{"index", std::to_string(i)}, {"status", std::to_string(status)},
                {"angle", status == Acad::eOk ? number(angle) : "null"},
                {"base", status == Acad::eOk ? point(AcGePoint2d(base_x, base_y)) : "null"},
                {"offset", status == Acad::eOk ? vector(AcGeVector2d(offset_x, offset_y)) : "null"},
                {"dashes", array(dash_values)}}));
        }
        std::vector<std::string> loops;
        for (int i = 0; i < hatch->numLoops(); ++i) {
            Adesk::Int32 type = hatch->loopTypeAt(i);
            Fields loop{{"index", std::to_string(i)}, {"type", std::to_string(type)}};
            if ((type & AcDbHatch::kPolyline) != 0) {
                AcGePoint2dArray points; AcGeDoubleArray bulges;
                const auto status = hatch->getLoopAt(i, type, points, bulges);
                status_error(errors, "hatch polyline loop", status);
                loop.emplace_back("status", std::to_string(status));
                require(hatch_budget.try_consume(static_cast<std::size_t>((std::max)(points.length(), bulges.length()))), "hatch whole-leaf boundary budget exceeded");
                std::vector<std::string> vertices, values;
                if (status == Acad::eOk) {
                    for (int j = 0; j < points.length(); ++j) vertices.push_back(point(points[j]));
                    for (int j = 0; j < bulges.length(); ++j) values.push_back(number(bulges[j]));
                    if (!bce_capture::hatch_polyline_counts_valid(static_cast<std::size_t>(points.length()),
                        static_cast<std::size_t>(bulges.length()))) errors.push_back("hatch vertex/bulge count mismatch");
                }
                loop.emplace_back("vertices", array(vertices)); loop.emplace_back("bulges", array(values));
            } else {
                AcGeVoidPointerArray edges; AcGeIntArray types;
                const auto status = hatch->getLoopAt(i, type, edges, types);
                status_error(errors, "hatch edge loop", status);
                loop.emplace_back("status", std::to_string(status));
                require(hatch_budget.try_consume(static_cast<std::size_t>(edges.length())), "hatch whole-leaf boundary budget exceeded");
                std::vector<std::string> values;
                if (status == Acad::eOk) {
                    if (edges.length() != types.length()) errors.push_back("hatch edge/type count mismatch");
                    for (int j = 0; j < (std::min)(edges.length(), types.length()); ++j) {
                        Fields edge{{"type", std::to_string(types[j])}, {"borrowed", "true"}};
                        if (!edges[j]) errors.push_back("null hatch edge");
                        else if (types[j] == AcDbHatch::kLine) {
                            const auto* line = static_cast<const AcGeLineSeg2d*>(edges[j]);
                            edge.emplace_back("start", point(line->startPoint())); edge.emplace_back("end", point(line->endPoint()));
                        } else if (types[j] == AcDbHatch::kCirArc) {
                            const auto* arc = static_cast<const AcGeCircArc2d*>(edges[j]);
                            const auto center = arc->center(), start = arc->startPoint(), end = arc->endPoint();
                            const auto reference_vector = arc->refVec();
                            const bce_capture::CircularArc definition{{center.x, center.y}, arc->radius(), arc->startAng(), arc->endAng(),
                                arc->isClockWise() != 0, {reference_vector.x, reference_vector.y}, {start.x, start.y}, {end.x, end.y}};
                            require(bce_capture::circular_arc_endpoints_consistent(definition), "hatch circular-arc public endpoints/reference inconsistent");
                            const auto arc_fields = bce_capture::circular_arc_fields(definition, number);
                            edge.insert(edge.end(), arc_fields.begin(), arc_fields.end());
                        } else if (types[j] == AcDbHatch::kEllArc) {
                            const auto* arc = static_cast<const AcGeEllipArc2d*>(edges[j]);
                            edge.insert(edge.end(), {{"center", point(arc->center())},
                                {"majorAxis", vector(arc->majorAxis())}, {"minorAxis", vector(arc->minorAxis())},
                                {"majorRadius", number(arc->majorRadius())}, {"minorRadius", number(arc->minorRadius())},
                                {"startAngle", number(arc->startAng())}, {"endAngle", number(arc->endAng())},
                                {"clockwise", boolean(arc->isClockWise())}});
                        } else if (types[j] == AcDbHatch::kSpline) {
                            const auto* spline = static_cast<const AcGeNurbCurve2d*>(edges[j]);
                            require(hatch_budget.try_consume(static_cast<std::size_t>(spline->numControlPoints())) &&
                                hatch_budget.try_consume(static_cast<std::size_t>(spline->numKnots())) &&
                                hatch_budget.try_consume(static_cast<std::size_t>(spline->numWeights())), "hatch whole-leaf spline budget exceeded");
                            std::vector<std::string> controls, knots, weights;
                            for (int k = 0; k < spline->numControlPoints(); ++k) controls.push_back(point(spline->controlPointAt(k)));
                            for (int k = 0; k < spline->numKnots(); ++k) knots.push_back(number(spline->knotAt(k)));
                            for (int k = 0; k < spline->numWeights(); ++k) weights.push_back(number(spline->weightAt(k)));
                            double period{}; const bool periodic = spline->isPeriodic(period) != 0;
                            edge.insert(edge.end(), {{"degree", std::to_string(spline->degree())}, {"rational", boolean(spline->isRational())},
                                {"closed", boolean(spline->isClosed())}, {"periodic", boolean(periodic)},
                                {"period", periodic ? number(period) : "null"},
                                {"controlPoints", array(controls)}, {"knots", array(knots)}, {"weights", array(weights)}});
                        } else { errors.push_back("unknown hatch edge boundary type"); edge.emplace_back("decoded", "false"); }
                        values.push_back(object(edge));
                    }
                }
                loop.emplace_back("edges", array(values));
                // The SDK ARXDBG hatch reader clears these arrays without deleting their curves.
                // They are borrowed boundary geometry, not explode-created database entities.
            }
            loops.push_back(object(loop));
        }
        return {{"kind", "\"hatch\""}, {"pattern", quote(hatch->patternName())},
            {"patternType", std::to_string(static_cast<int>(hatch->patternType()))},
            {"scale", number(hatch->patternScale())}, {"angle", number(hatch->patternAngle())},
            {"patternSpace", number(hatch->patternSpace())}, {"patternDouble", boolean(hatch->patternDouble())},
            {"patternDefinitionCount", std::to_string(pattern_definition_count)}, {"patternDefinitions", array(pattern_definitions)},
            {"captureItemCount", std::to_string(hatch_budget.used())},
            {"associative", boolean(hatch->associative())}, {"loopCount", std::to_string(hatch->numLoops())},
            {"normal", vector(hatch->normal())}, {"elevation", number(hatch->elevation())},
            {"boundaryCoordinateSystem", "\"Hatch plane 2D; normal/elevation supplied separately\""}, {"loops", array(loops)}};
    }
    known = false;
    return {{"kind", "\"unknown\""}};
}

struct ReadContext {
    std::vector<std::unique_ptr<AcDbEntity>> owned;
    std::set<const void*> seen;
    std::set<AcDbObjectId> block_path;
    std::vector<std::string> nodes;
    Errors errors;
    std::size_t returned{}, deleted{}, borrowed{}, duplicates{}, unresolved{}, text_count{}, visits{}, dimensions{}, dimension_glyphs{};
    bool ownership_safe{true};
    void release() noexcept {
        for (auto& value : owned) { if (value) { value.reset(); ++deleted; } }
        owned.clear();
    }
    ~ReadContext() noexcept { release(); }
};
bool detached(const AcDbEntity& entity) {
    return entity.objectId().isNull() && entity.ownerId().isNull() && entity.database() == nullptr;
}
// Takes ownership only after all three public detached checks and pointer-identity checks.
// Raw unknown or borrowed returns remain borrowed; never close/delete someone else's object.
void take_returns(ReadContext& context, AcDbVoidPtrArray& values, const AcDbEntity* source,
                  std::vector<AcDbEntity*>& accepted) {
    for (int i = 0; i < values.length(); ++i) {
        auto* raw = values[i];
        if (!raw) { ++context.unresolved; context.ownership_safe = false; continue; }
        if (raw == source || context.seen.count(raw)) {
            ++context.duplicates; context.ownership_safe = false; continue;
        }
        context.seen.insert(raw);
        auto* entity = AcDbEntity::cast(static_cast<AcRxObject*>(raw));
        if (!entity) { ++context.unresolved; context.ownership_safe = false; continue; }
        if (!detached(*entity)) { ++context.borrowed; context.ownership_safe = false; continue; }
        std::unique_ptr<AcDbEntity> owned(entity);
        ++context.returned;
        if (context.owned.size() >= maximum_nodes) {
            owned.reset(); ++context.deleted;
            context.errors.push_back("owned transient budget truncated; extra proven detached entity deleted without traversal");
            continue;
        }
        context.owned.push_back(std::move(owned));
        accepted.push_back(entity);
    }
}
void read_node(AcDbEntity& entity, int parent, int depth, const char* origin, ReadContext& context);
bool read_dimension_block(AcDbDimension& dimension, int parent, int depth, ReadContext& context, Fields& fields) {
    const auto id = dimension.dimBlockId();
    fields.emplace_back("dimBlockId", handle(id));
    fields.emplace_back("dimBlockCoordinates", "\"Stored dimension block local coordinates; not asserted as transformed world glyphs\"");
    if (id.isNull()) { fields.emplace_back("dimBlockReadStatus", "\"null reference\""); return false; }
    if (context.block_path.count(id)) { context.errors.push_back("dimension block recursion cycle"); return false; }
    try {
        auto block = open_read<AcDbBlockTableRecord>(id);
        AcDbBlockTableRecordIterator* raw{}; check(block->newIterator(raw), "dimension block iterator");
        std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw);
        context.block_path.insert(id);
        struct PathScope { std::set<AcDbObjectId>& path; AcDbObjectId id; ~PathScope() { path.erase(id); } } path_scope{context.block_path, id};
        for (; !iterator->done(); iterator->step()) {
            if (context.visits >= maximum_nodes) { context.errors.push_back("dimension block node budget truncated"); break; }
            AcDbObjectId child_id; check(iterator->getEntityId(child_id), "dimension block entity id");
            auto child = open_read<AcDbEntity>(child_id);
            read_node(*child, parent, depth + 1, "dimBlock-borrowed-local", context);
        }
        fields.emplace_back("dimBlockReadStatus", "0");
        return true;
    } catch (const std::exception& failure) {
        context.errors.push_back(failure.what()); fields.emplace_back("dimBlockReadError", quoted_bytes(failure.what())); return false;
    }
}
void read_node(AcDbEntity& entity, int parent, int depth, const char* origin, ReadContext& context) {
    if (context.visits >= maximum_nodes) { context.errors.push_back("node budget truncated"); return; }
    const int node_id = static_cast<int>(context.visits++);
    const auto errors_before = context.errors.size();
    Fields fields{{"nodeId", std::to_string(node_id)}, {"parentNodeId", std::to_string(parent)},
        {"depth", std::to_string(depth)}, {"origin", quoted_bytes(origin)},
        {"transient", boolean(detached(entity))}, {"objectIdNull", boolean(entity.objectId().isNull())},
        {"ownerIdNull", boolean(entity.ownerId().isNull())}, {"databaseNull", boolean(entity.database() == nullptr)}};
    try {
        fields.emplace_back("public", common(entity, context.errors));
        bool known{};
        auto details = leaf_fields(entity, context.errors, known);
        fields.insert(fields.end(), details.begin(), details.end());
        auto* dimension = AcDbDimension::cast(&entity);
        const bool text = AcDbText::cast(&entity) || AcDbMText::cast(&entity);
        if (text) {
            const auto content_status = std::find_if(details.begin(), details.end(), [](const auto& field) {
                return field.first == "textStatus" || field.first == "plainStatus";
            });
            if (content_status != details.end() && content_status->second == "0") ++context.text_count;
        }
        if (dimension) {
            ++context.dimensions;
            double measurement{};
            const auto status = dimension->measurement(measurement); // 2020 public API is intentionally non-const.
            status_error(context.errors, "measurement", status);
            fields.emplace_back("measurement", object({{"status", std::to_string(status)},
                {"value", status == Acad::eOk ? number(measurement) : "null"}}));
            AcString replacement, formatted;
            const auto override_status = dimension->dimensionText(replacement);
            const bool format_attempted = status == Acad::eOk && override_status == Acad::eOk;
            Acad::ErrorStatus formatted_status = Acad::eNotApplicable;
            if (format_attempted)
                formatted_status = dimension->formatMeasurement(formatted, measurement, replacement.kACharPtr());
            if (format_attempted) status_error(context.errors, "formatMeasurement", formatted_status);
            else context.errors.push_back("formatMeasurement not attempted because measurement/override getter failed");
            fields.emplace_back("formatted", object({{"attempted", boolean(format_attempted)},
                {"status", format_attempted ? std::to_string(formatted_status) : "null"},
                {"text", format_attempted && formatted_status == Acad::eOk ? quote(formatted.kACharPtr()) : "null"},
                {"isActualGlyphRead", "false"}}));
        }
        if (auto* block = AcDbBlockReference::cast(&entity)) {
            // Attributes are database-owned instance entities and are not assumed to be included by block explode.
            std::unique_ptr<AcDbObjectIterator> attributes(block->attributeIterator());
            require(attributes != nullptr, "block attribute iterator unavailable");
            for (; !attributes->done(); attributes->step()) {
                if (depth >= maximum_depth || context.visits >= maximum_nodes) {
                    context.errors.push_back("block attribute traversal truncated"); break;
                }
                auto attribute = open_read<AcDbEntity>(attributes->objectId());
                read_node(*attribute, node_id, depth + 1, "block-attribute-borrowed-instance", context);
            }
        }
        const bool expandable = dimension || AcDbBlockReference::cast(&entity) || !known;
        if (expandable) {
            const auto text_before = context.text_count;
            if (depth >= maximum_depth) context.errors.push_back("explode depth truncated");
            else {
                AcDbVoidPtrArray values;
                // Even when a third-party implementation throws, any returned proven detached entities are reclaimed.
                Acad::ErrorStatus status = Acad::eNotApplicable;
                std::vector<AcDbEntity*> accepted;
                try { status = entity.explode(values); }
                catch (...) { take_returns(context, values, &entity, accepted); throw; }
                take_returns(context, values, &entity, accepted);
                fields.emplace_back("explode", object({{"status", std::to_string(status)},
                    {"returnedCount", std::to_string(values.length())}, {"acceptedDetachedCount", std::to_string(accepted.size())}}));
                if (values.length() > static_cast<int>(maximum_nodes)) context.errors.push_back("explode return count exceeds traversal budget");
                // A failed explode is retained as a read limitation even if a dimBlock fallback yields text.
                status_error(context.errors, "explode", status);
                for (auto* child : accepted) {
                    if (context.visits >= maximum_nodes) { context.errors.push_back("explode node budget truncated"); break; }
                    read_node(*child, node_id, depth + 1, "explode-transformed", context);
                }
                if (!known && accepted.empty()) context.errors.push_back("unknown leaf or empty public compound representation");
            }
            if (dimension) {
                if (context.text_count == text_before && depth < maximum_depth)
                    read_dimension_block(*dimension, node_id, depth, context, fields);
                const bool glyph_read = context.text_count > text_before;
                if (glyph_read) ++context.dimension_glyphs;
                fields.emplace_back("glyphRead", boolean(glyph_read));
                fields.emplace_back("glyphCount", std::to_string(context.text_count - text_before));
                if (!glyph_read) context.errors.push_back("dimension actual Text/MText glyphs unavailable through public read paths");
            }
        }
    } catch (const std::exception& failure) { context.errors.push_back(failure.what()); }
    catch (...) { context.errors.push_back("unknown C++ exception reading node"); }
    Errors node_errors(context.errors.begin() + static_cast<std::ptrdiff_t>(errors_before), context.errors.end());
    fields.emplace_back("errors", errors_json(node_errors));
    fields.emplace_back("complete", boolean(node_errors.empty()));
    context.nodes.push_back(object(fields));
}
std::string observation(AcDbEntity& root, bool& complete) {
    ReadContext context;
    context.seen.insert(&root);
    Errors common_errors;
    const auto public_root = common(root, common_errors);
    context.errors = common_errors;
    read_node(root, -1, 0, "model-root-borrowed", context);
    context.release();
    const bool ownership = context.ownership_safe && context.returned == context.deleted;
    if (!ownership) context.errors.push_back("explode return ownership was not fully safe");
    complete = context.errors.empty() && ownership;
    // Root common fields are promoted to observation level for simple consumers.
    std::string result = public_root;
    result.pop_back();
    return result + ",\"nodes\":" + array(context.nodes) + ",\"errors\":" + errors_json(context.errors)
        + ",\"complete\":" + boolean(complete) + ",\"ownershipSafe\":" + boolean(ownership)
        + ",\"glyphRead\":" + boolean(context.dimensions > 0 ? context.dimensions == context.dimension_glyphs : context.text_count > 0)
        + ",\"dimensionCount\":" + std::to_string(context.dimensions) + ",\"dimensionGlyphReadCount\":" + std::to_string(context.dimension_glyphs)
        + ",\"ownership\":" + object({{"returnedDetached", std::to_string(context.returned)},
            {"deletedDetached", std::to_string(context.deleted)}, {"rejectedBorrowed", std::to_string(context.borrowed)},
            {"duplicatePointers", std::to_string(context.duplicates)}, {"unresolvedPointers", std::to_string(context.unresolved)}}) + '}';
}
std::vector<AcDbObjectId> model_ids(AcDbDatabase& database) {
    AcDbBlockTable* raw_table{}; check(database.getBlockTable(raw_table, AcDb::kForRead), "read block table");
    Opened<AcDbBlockTable> table(raw_table);
    AcDbObjectId model_id; check(table->getAt(ACDB_MODEL_SPACE, model_id), "model space id");
    auto model = open_read<AcDbBlockTableRecord>(model_id);
    AcDbBlockTableRecordIterator* raw_iterator{}; check(model->newIterator(raw_iterator), "model iterator");
    std::unique_ptr<AcDbBlockTableRecordIterator> iterator(raw_iterator);
    std::vector<AcDbObjectId> ids;
    for (; !iterator->done(); iterator->step()) {
        require(ids.size() < maximum_roots, "model root budget exceeded");
        AcDbObjectId id; check(iterator->getEntityId(id), "model entity identity"); ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end(), [](AcDbObjectId a, AcDbObjectId b) { return handle(a) < handle(b); });
    return ids;
}
std::string sha256(const std::string& bytes) {
    HCRYPTPROV provider{};
    require(CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) != FALSE, "SHA256 provider unavailable");
    struct ProviderScope { HCRYPTPROV value; ~ProviderScope() { CryptReleaseContext(value, 0); } } provider_scope{provider};
    HCRYPTHASH hash{};
    require(CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) != FALSE, "SHA256 creation failed");
    struct HashScope { HCRYPTHASH value; ~HashScope() { CryptDestroyHash(value); } } hash_scope{hash};
    require(bytes.size() <= MAXDWORD, "fingerprint byte budget exceeded");
    require(CryptHashData(hash, reinterpret_cast<const BYTE*>(bytes.data()), static_cast<DWORD>(bytes.size()), 0) != FALSE, "SHA256 input failed");
    std::array<BYTE, 32> digest{}; DWORD length = static_cast<DWORD>(digest.size());
    require(CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &length, 0) != FALSE && length == digest.size(), "SHA256 output failed");
    std::ostringstream out; for (const auto byte : digest) out << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
    return quoted_bytes(out.str());
}
struct Fingerprint { std::string json{"null"}; std::string digest; bool complete{}; };
Fingerprint fingerprint(AcDbDatabase& database) {
    const auto ids = model_ids(database);
    std::vector<std::string> records;
    Errors errors;
    for (const auto id : ids) {
        auto entity = open_read<AcDbEntity>(id);
        Errors local;
        bool known{};
        const auto fields = leaf_fields(*entity, local, known);
        records.push_back(object({{"public", common(*entity, local)}, {"detail", object(fields)},
            {"declaredGeometryDecoded", boolean(known)}, {"errors", errors_json(local)}}));
        errors.insert(errors.end(), local.begin(), local.end());
    }
    const auto digest = sha256(array(records));
    return {object({{"algorithm", "\"SHA256\""}, {"scope", "\"Sorted model root public identity/appearance/bounds and supported direct getters; no private data or complete DWG equivalence\""},
        {"count", std::to_string(ids.size())}, {"sha256", digest}, {"complete", boolean(errors.empty())}, {"errors", errors_json(errors)}}), digest, errors.empty()};
}
struct SourceResult { std::string json; bool complete{}; };
SourceResult inspect_source(const wchar_t* role, const std::wstring& path) {
    AcDbDatabase database(false, true);
    const auto read_status = database.readDwgFile(path.c_str(), AcDbDatabase::kForReadAndAllShare, false);
    std::string close_status = "null";
    Errors errors;
    std::vector<std::string> observations;
    std::map<std::wstring, std::size_t> types;
    std::size_t count{}, proxies{};
    Fingerprint before, after;
    try {
        check(read_status, "read SideDB");
        const auto close = database.closeInput(true); close_status = std::to_string(close); check(close, "close SideDB input");
        WorkingDatabaseScope working(database);
        before = fingerprint(database);
        try {
            const auto ids = model_ids(database); count = ids.size();
            for (const auto id : ids) {
                auto entity = open_read<AcDbEntity>(id);
                const std::wstring type = entity->isA()->name();
                ++types[type]; if (entity->isAProxy()) ++proxies;
                const bool interesting = AcDbDimension::cast(entity.get()) || AcDbText::cast(entity.get()) ||
                    AcDbMText::cast(entity.get()) || AcDbBlockReference::cast(entity.get()) || AcDbHatch::cast(entity.get()) ||
                    type.compare(0, 3, L"TDb") == 0;
                if (interesting) {
                    bool complete{}; observations.push_back(observation(*entity, complete));
                    if (!complete) errors.push_back("incomplete observation handle=" + handle(id));
                }
            }
        } catch (const std::exception& failure) { errors.push_back(failure.what()); }
        catch (...) { errors.push_back("unknown C++ exception enumerating source"); }
        after = fingerprint(database);
        if (!before.complete || !after.complete) errors.push_back("public fingerprint contains getter limitations");
        if (before.digest != after.digest) errors.push_back("public model fingerprint changed during read");
    } catch (const std::exception& failure) { errors.push_back(failure.what()); }
    catch (...) { errors.push_back("unknown C++ exception opening/reading source"); }
    Fields type_fields;
    for (const auto& entry : types) type_fields.emplace_back(utf8(entry.first), std::to_string(entry.second));
    return {object({{"role", quote(role)}, {"path", quote(path)}, {"readStatus", std::to_string(read_status)},
        {"closeInputStatus", close_status}, {"modelCount", std::to_string(count)}, {"proxyCount", std::to_string(proxies)},
        {"typeCounts", object(type_fields)}, {"observations", array(observations)},
        {"before", before.json}, {"after", after.json}, {"publicFingerprintUnchanged", boolean(!before.digest.empty() && before.digest == after.digest)},
        {"error", errors.empty() ? "null" : errors_json(errors)}, {"complete", boolean(errors.empty())},
        {"sourceDrawingWritten", "false"}, {"businessWriteAttempted", "false"}}), errors.empty()};
}

struct HandleCloser { void operator()(void* value) const noexcept { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); } };
using WinHandle = std::unique_ptr<void, HandleCloser>;
std::wstring full_path(const std::wstring& value) {
    std::array<wchar_t, 32768> buffer{};
    const auto size = GetFullPathNameW(value.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    require(size > 0 && size < buffer.size(), "invalid full path");
    std::wstring path(buffer.data(), size);
    while (path.size() > 3 && path.back() == L'\\') path.pop_back();
    require(path.size() > 3 && path[1] == L':' && path[2] == L'\\', "only absolute local drive paths are accepted");
    require(path.find(L':', 2) == std::wstring::npos, "alternate stream path rejected");
    return path;
}
void reject_reparse(const std::wstring& path) {
    for (std::size_t i = 3; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != L'\\') continue;
        const auto part = path.substr(0, i);
        const auto attributes = GetFileAttributesW(part.c_str());
        require(attributes != INVALID_FILE_ATTRIBUTES, "configured path/ancestor does not exist");
        require((attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0, "reparse path rejected");
    }
}
bool below(const std::wstring& path, const std::wstring& root) {
    const auto prefix = root + L'\\';
    return path.size() > prefix.size() && _wcsnicmp(path.c_str(), prefix.c_str(), prefix.size()) == 0;
}
std::wstring desktop_name(HDESK desktop) {
    require(desktop != nullptr, "desktop handle unavailable");
    DWORD bytes{}; GetUserObjectInformationW(desktop, UOI_NAME, nullptr, 0, &bytes);
    require(bytes > sizeof(wchar_t) && bytes < 65536, "desktop name size unavailable");
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1);
    require(GetUserObjectInformationW(desktop, UOI_NAME, buffer.data(), static_cast<DWORD>(buffer.size() * sizeof(wchar_t)), &bytes) != FALSE, "desktop name unreadable");
    require(buffer[0] != 0, "desktop name is empty");
    return buffer.data();
}
struct Config { std::wstring nonce, session, scheme, reference, output; };
Config config() {
    std::array<wchar_t, 32768> module{};
    const auto length = GetModuleFileNameW(module_handle, module.data(), static_cast<DWORD>(module.size()));
    require(length > 0 && length < module.size(), "module path unavailable");
    std::wstring ini(module.data(), length);
    const auto extension = ini.find_last_of(L'.'); require(extension != std::wstring::npos, "module extension absent");
    ini.replace(extension, std::wstring::npos, L".ini");
    WinHandle file(CreateFileW(ini.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    require(file.get() != INVALID_HANDLE_VALUE, "module INI absent");
    std::array<BYTE, 2> bom{}; DWORD read{};
    require(ReadFile(file.get(), bom.data(), static_cast<DWORD>(bom.size()), &read, nullptr) != FALSE && read == 2 && bom[0] == 0xff && bom[1] == 0xfe, "INI must be UTF-16LE with BOM");
    const auto get = [&ini](const wchar_t* key) {
        std::array<wchar_t, 32768> value{};
        const auto size = GetPrivateProfileStringW(L"inspect", key, L"", value.data(), static_cast<DWORD>(value.size()), ini.c_str());
        require(size > 0 && size < value.size() - 1, "INI key absent or truncated");
        return std::wstring(value.data(), size);
    };
    Config result{get(L"nonce"), full_path(get(L"session")), full_path(get(L"scheme")), full_path(get(L"reference")), full_path(get(L"output"))};
    require(result.nonce.size() == 32 && std::all_of(result.nonce.begin(), result.nonce.end(), [](wchar_t c) { return (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'); }), "nonce must be 32 lowercase hex characters");
    require(below(result.scheme, result.session) && below(result.reference, result.session) && below(result.output, result.session), "inputs/output must be under session");
    require(_wcsicmp(result.scheme.c_str(), result.reference.c_str()) != 0, "inputs must be distinct files");
    reject_reparse(result.session); reject_reparse(result.scheme); reject_reparse(result.reference); reject_reparse(result.output);
    require((GetFileAttributesW(result.output.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0, "output must be an existing directory");
    require((GetFileAttributesW(result.scheme.c_str()) & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
        (GetFileAttributesW(result.reference.c_str()) & FILE_ATTRIBUTE_DIRECTORY) == 0, "input must be a file");
    return result;
}
void write_report(HANDLE file, const std::string& report) {
    require(report.size() <= MAXDWORD, "report exceeds write limit");
    DWORD written{};
    require(WriteFile(file, report.data(), static_cast<DWORD>(report.size()), &written, nullptr) != FALSE && written == report.size(), "exclusive report write failed");
    require(FlushFileBuffers(file) != FALSE, "report flush failed");
}
void capture() noexcept {
    Config settings;
    std::wstring desktop;
    std::vector<std::string> sources;
    std::string error = "null";
    WinHandle report;
    try {
        settings = config();
        const auto path = settings.output + L"\\reference-inspection.json";
        report.reset(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        require(report.get() != INVALID_HANDLE_VALUE, "report must be a new exclusive file");
        desktop = desktop_name(GetThreadDesktop(GetCurrentThreadId()));
        require(desktop == L"CodexS1_" + settings.nonce, "thread desktop does not match session nonce");
        HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
        require(input != nullptr, "input desktop unreadable");
        struct DesktopScope { HDESK value; ~DesktopScope() { CloseDesktop(value); } } input_scope{input};
        const auto input_name = desktop_name(input);
        require(_wcsicmp(desktop.c_str(), input_name.c_str()) != 0, "worker desktop is the input desktop");
        const auto scheme = inspect_source(L"scheme", settings.scheme);
        sources.push_back(scheme.json);
        const auto reference = inspect_source(L"reference", settings.reference);
        sources.push_back(reference.json);
        if (!scheme.complete || !reference.complete) error = "\"One or more sources contain explicit read limitations; inspect source errors\"";
    } catch (const std::exception& failure) { try { error = quoted_bytes(failure.what()); } catch (...) { error = "\"C++ exception while formatting failure\""; } }
    catch (...) { error = "\"Unknown C++ exception at capture boundary\""; }
    try {
        if (report && report.get() != INVALID_HANDLE_VALUE) write_report(report.get(), object({
            {"contract", "\"bce.reference-inspection.v1\""}, {"nonce", quote(settings.nonce)},
            {"pid", std::to_string(GetCurrentProcessId())}, {"desktop", quote(desktop)},
            {"sources", array(sources)}, {"error", error}, {"sourceDrawingWritten", "false"},
            {"businessWriteAttempted", "false"}, {"glyphReadMeaning", "\"Public Text/MText content and geometry were read; raster font rendering and visual equivalence are not verified\""},
            {"limits", object({{"maxDepth", std::to_string(maximum_depth)}, {"maxNodesPerObservation", std::to_string(maximum_nodes)},
                {"maxModelRoots", std::to_string(maximum_roots)}, {"maxVerticesPerLeaf", std::to_string(maximum_vertices)}})}}));
        acutPrintf(error == "null" ? L"\nBCE1_CAPTURE_REPORT_WRITTEN\n" : L"\nBCE1_CAPTURE_INCOMPLETE; inspect new JSON report if created\n");
    } catch (...) { acutPrintf(L"\nBCE1_CAPTURE_REPORT_WRITE_FAILED\n"); }
}
} // namespace

BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { module_handle = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
extern "C" AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void* id) {
    try {
        if (message == AcRx::kInitAppMsg) {
            acrxDynamicLinker->unlockApplication(id);
            acrxRegisterAppMDIAware(id);
            return acedRegCmds->addCommand(L"BCE1_REFERENCE_READONLY", L"BCE1_CAPTURE", L"BCE1_CAPTURE",
                ACRX_CMD_MODAL, capture) == Acad::eOk ? AcRx::kRetOK : AcRx::kRetError;
        }
        if (message == AcRx::kUnloadAppMsg) acedRegCmds->removeGroup(L"BCE1_REFERENCE_READONLY");
        return AcRx::kRetOK;
    } catch (...) { return AcRx::kRetError; }
}
