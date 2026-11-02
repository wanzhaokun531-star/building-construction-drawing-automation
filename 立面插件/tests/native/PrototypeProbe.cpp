// N1 extracts only the bounded public reader from the read-only stage-one tool.
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
#include "N1ReadPolicy.h"
#pragma comment(lib, "Advapi32.lib")

#include "PrototypeProbe.h"
#include "N1ProbePolicy.h"
#include <dbidmap.h>
#include <dbmleader.h>
#include <rxmember.h>
#include <rxprop.h>
#include <rxvalue.h>
#include <AcDbAssocValueProviderPE.h>
namespace bce::native::reader {
Evidence* checkpointSink{}; // Command-scoped, reset before returning to host.
void danger(const std::string& name) { if (checkpointSink) checkpointSink->checkpoint(name); }
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
    if (const auto* d1 = AcDbAlignedDimension::cast(&dim)) fields.insert(fields.end(), {
        {"xLine1Point", point(d1->xLine1Point())}, {"xLine2Point", point(d1->xLine2Point())},
        {"dimLinePoint", point(d1->dimLinePoint())}, {"oblique", number(d1->oblique())}});
    else if (const auto* d2 = AcDbRotatedDimension::cast(&dim)) fields.insert(fields.end(), {
        {"xLine1Point", point(d2->xLine1Point())}, {"xLine2Point", point(d2->xLine2Point())},
        {"dimLinePoint", point(d2->dimLinePoint())}, {"rotation", number(d2->rotation())}, {"oblique", number(d2->oblique())}});
    else if (const auto* d3 = AcDb3PointAngularDimension::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d3->centerPoint())}, {"xLine1Point", point(d3->xLine1Point())},
        {"xLine2Point", point(d3->xLine2Point())}, {"arcPoint", point(d3->arcPoint())}});
    else if (const auto* d4 = AcDb2LineAngularDimension::cast(&dim)) fields.insert(fields.end(), {
        {"xLine1Start", point(d4->xLine1Start())}, {"xLine1End", point(d4->xLine1End())},
        {"xLine2Start", point(d4->xLine2Start())}, {"xLine2End", point(d4->xLine2End())}, {"arcPoint", point(d4->arcPoint())}});
    else if (const auto* d5 = AcDbRadialDimension::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d5->center())}, {"chordPoint", point(d5->chordPoint())}, {"leaderLength", number(d5->leaderLength())}});
    else if (const auto* d6 = AcDbDiametricDimension::cast(&dim)) fields.insert(fields.end(), {
        {"chordPoint", point(d6->chordPoint())}, {"farChordPoint", point(d6->farChordPoint())}, {"leaderLength", number(d6->leaderLength())}});
    else if (const auto* d7 = AcDbOrdinateDimension::cast(&dim)) fields.insert(fields.end(), {
        {"origin", point(d7->origin())}, {"definingPoint", point(d7->definingPoint())},
        {"leaderEndPoint", point(d7->leaderEndPoint())}, {"usingXAxis", boolean(d7->isUsingXAxis())}});
    else if (const auto* d8 = AcDbArcDimension::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d8->centerPoint())}, {"xLine1Point", point(d8->xLine1Point())},
        {"xLine2Point", point(d8->xLine2Point())}, {"arcPoint", point(d8->arcPoint())}});
    else if (const auto* d9 = AcDbRadialDimensionLarge::cast(&dim)) fields.insert(fields.end(), {
        {"center", point(d9->center())}, {"chordPoint", point(d9->chordPoint())},
        {"overrideCenter", point(d9->overrideCenter())}, {"jogPoint", point(d9->jogPoint())}, {"jogAngle", number(d9->jogAngle())}});
    else errors.push_back("dimension subclass defining points unsupported");
    return fields;
}
std::string actual_dimstyle(AcDbObjectId id, Errors& errors) {
    try {
        auto record = open_read<AcDbDimStyleTableRecord>(id);
        return object({{"dimscale", number(record->dimscale())}, {"dimtxt", number(record->dimtxt())},
            {"dimexo", number(record->dimexo())}, {"dimexe", number(record->dimexe())}, {"dimgap", number(record->dimgap())},
            {"dimlfac", number(record->dimlfac())}, {"dimdec", std::to_string(record->dimdec())},
            {"dimlunit", std::to_string(record->dimlunit())}, {"dimzin", std::to_string(record->dimzin())},
            {"dimrnd", number(record->dimrnd())}, {"dimasz", number(record->dimasz())}, {"dimtsz", number(record->dimtsz())},
            {"dimpost", quote(record->dimpost())}});
    } catch (const std::exception& failure) { errors.push_back(failure.what()); return "null"; }
}
std::string dimension_style(const AcDbDimension& dim, Errors& errors) {
    return object({{"reference", symbol_reference(dim.dimensionStyle(), errors)},
        {"actualDefinition", actual_dimstyle(dim.dimensionStyle(), errors)},
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
        for (int i = 0; i < 4; ++i) { AcGePoint3d p; check(solid->getPointAt(static_cast<Adesk::UInt16>(i), p), "solid vertex"); vertices.push_back(point(p)); }
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

struct SemanticNode {
    std::wstring type, text;
    std::vector<AcGePoint3d> points;
    std::vector<double> values;
    std::string style;
    bool localCoordinates{};
    bool geometryObservable{true};
    bool zeroBounds{};
    int boundsStatus{};
};
std::string style_definition(AcDbObjectId id) {
    if (id.isNull()) return "null";
    Errors errors;
    auto fields = text_style(id, errors);
    const auto start = fields.find(",\"fontRenderingVerified\"");
    require(start != std::string::npos, "style definition projection unavailable");
    require(errors.empty(), "style actual definition read failed");
    return '{' + fields.substr(start + 1);
}
std::string appearance_definition(const AcDbEntity& entity) {
    auto layer = open_read<AcDbLayerTableRecord>(entity.layerId());
    auto line = open_read<AcDbLinetypeTableRecord>(entity.linetypeId());
    auto layerLine = open_read<AcDbLinetypeTableRecord>(layer->linetypeObjectId());
    const auto definition = [](const AcDbLinetypeTableRecord& record) {
        require(record.numDashes() >= 0 && record.numDashes() <= maximum_vertices, "linetype dash budget");
        std::vector<std::string> dashes;
        for (int i = 0; i < record.numDashes(); ++i) dashes.push_back(number(record.dashLengthAt(i)));
        return object({{"patternLength", number(record.patternLength())}, {"dashes", array(dashes)}, {"scaledToFit", boolean(record.isScaledToFit())}});
    };
    return object({{"entityColor", std::to_string(entity.color().color())}, {"entityLineWeight", std::to_string(entity.lineWeight())},
        {"entityVisibility", std::to_string(entity.visibility())}, {"entityLinetypeScale", number(entity.linetypeScale())},
        {"layerColor", std::to_string(layer->color().color())}, {"layerLineWeight", std::to_string(layer->lineWeight())},
        {"layerOff", boolean(layer->isOff())}, {"layerFrozen", boolean(layer->isFrozen())}, {"layerLocked", boolean(layer->isLocked())},
        {"entityLinetype", definition(*line)}, {"layerLinetype", definition(*layerLine)}});
}
SemanticNode semantic(AcDbEntity& entity, const char* origin) {
    SemanticNode result;
    result.type = entity.isA()->name();
    result.localCoordinates = std::string(origin) == "dimBlock-borrowed-local";
    AcDbExtents bounds;
    result.boundsStatus=static_cast<int>(entity.getGeomExtents(bounds));
    if (result.boundsStatus == Acad::eOk) {
        result.zeroBounds=bounds.minPoint().x==0 && bounds.minPoint().y==0 && bounds.minPoint().z==0 &&
            bounds.maxPoint().x==0 && bounds.maxPoint().y==0 && bounds.maxPoint().z==0;
        result.points.push_back(bounds.minPoint()); result.points.push_back(bounds.maxPoint());
    }
    if (const auto* text = AcDbText::cast(&entity)) {
        AcString content; check(text->textString(content), "semantic Text content"); result.text = content.kACharPtr();
        result.points.push_back(text->position());
        if (text->horizontalMode() != AcDb::kTextLeft || text->verticalMode() != AcDb::kTextBase)
            result.points.push_back(text->alignmentPoint());
        result.values = {text->height(), text->widthFactor(), text->rotation(), text->oblique(),
            double(text->horizontalMode()), double(text->verticalMode())};
        result.style = style_definition(text->textStyle());
    } else if (const auto* mtext = AcDbMText::cast(&entity)) {
        AcString raw, plain; check(mtext->contents(raw), "semantic MText contents"); check(mtext->text(plain), "semantic MText plain");
        result.text = std::wstring(raw.kACharPtr()) + L"\n" + plain.kACharPtr(); result.points.push_back(mtext->location());
        result.values = {mtext->textHeight(), mtext->width(), mtext->actualWidth(), mtext->actualHeight(), mtext->rotation(), double(mtext->attachment())};
        result.style = style_definition(mtext->textStyle());
    } else if (auto* dim = AcDbDimension::cast(&entity)) {
        double measurement{}; danger("before-public-dimension-measurement"); check(dim->measurement(measurement), "semantic dimension measurement");
        AcString content, formatted; check(dim->dimensionText(content), "semantic dimension override");
        check(dim->formatMeasurement(formatted, measurement, content.kACharPtr()), "semantic dimension format");
        result.text = std::wstring(content.kACharPtr()) + L"\n" + formatted.kACharPtr();
        result.values = {measurement, dim->dimscale(), dim->dimtxt(), dim->dimexo(), dim->dimexe(), dim->dimgap(),
            dim->dimlfac(), double(dim->dimdec()), double(dim->dimlunit()), double(dim->dimzin()), dim->dimrnd(), dim->dimasz(), dim->dimtsz(),
            double(dim->dimtad()), double(dim->dimjust()), double(dim->dimtmove()), dim->dimaltf(), dim->dimtp(), dim->dimtm()};
        result.values.push_back(dim->isUsingDefaultTextPosition() ? 1.0 : 0.0);
        if (!dim->isUsingDefaultTextPosition()) result.points.push_back(dim->textPosition());
        if (const auto* aligned = AcDbAlignedDimension::cast(dim)) {
            result.points.push_back(aligned->xLine1Point()); result.points.push_back(aligned->xLine2Point()); result.points.push_back(aligned->dimLinePoint());
        } else if (const auto* rotated = AcDbRotatedDimension::cast(dim)) {
            result.points.push_back(rotated->xLine1Point()); result.points.push_back(rotated->xLine2Point()); result.points.push_back(rotated->dimLinePoint());
        }
        Errors styleErrors;
        result.style = object({{"textStyle", style_definition(dim->dimtxsty())}, {"dimensionStyle", actual_dimstyle(dim->dimensionStyle(), styleErrors)}});
        require(styleErrors.empty(), "actual dimension style record unavailable");
    } else if (const auto* line = AcDbLine::cast(&entity)) {
        result.points.push_back(line->startPoint()); result.points.push_back(line->endPoint());
    } else if (const auto* arc = AcDbArc::cast(&entity)) {
        result.points.push_back(arc->center()); result.values = {arc->radius(), arc->startAngle(), arc->endAngle()};
    } else if (const auto* circle = AcDbCircle::cast(&entity)) {
        result.points.push_back(circle->center()); result.values = {circle->radius()};
    } else if (const auto* polyline = AcDbPolyline::cast(&entity)) {
        require(polyline->numVerts() <= maximum_vertices, "semantic polyline budget");
        for (unsigned i = 0; i < polyline->numVerts(); ++i) { AcGePoint3d p; check(polyline->getPointAt(i, p), "semantic vertex"); result.points.push_back(p); }
    } else if (const auto* hatch = AcDbHatch::cast(&entity)) {
        result.text = hatch->patternName(); result.values = {hatch->patternScale(), hatch->patternAngle(), double(hatch->numLoops()), double(hatch->patternType())};
        require((hatch->normal() - AcGeVector3d::kZAxis).length() <= 1e-9, "hatch semantic comparison only supports observed XY-plane sample");
        std::size_t vertices = 0;
        for (int i = 0; i < hatch->numLoops(); ++i) {
            Adesk::Int32 type = hatch->loopTypeAt(i); require((type & AcDbHatch::kPolyline) != 0, "non-polyline hatch loop comparison is unproven");
            AcGePoint2dArray points; AcGeDoubleArray bulges; check(hatch->getLoopAt(i, type, points, bulges), "semantic hatch loop");
            vertices += static_cast<std::size_t>(points.length()); require(vertices <= maximum_vertices, "semantic hatch vertex budget");
            result.values.push_back(double(type)); result.values.push_back(double(points.length())); result.values.push_back(double(bulges.length()));
            for (int j = 0; j < points.length(); ++j) result.points.emplace_back(points[j].x, points[j].y, hatch->elevation());
            for (int j = 0; j < bulges.length(); ++j) result.values.push_back(bulges[j]);
        }
    } else if (const auto* block = AcDbBlockReference::cast(&entity)) result.points.push_back(block->position());
    result.style = object({{"specificStyle", result.style.empty() ? "null" : result.style}, {"actualAppearanceDefinitions", appearance_definition(entity)}});
    return result;
}
struct ReadContext {
    std::vector<std::unique_ptr<AcDbEntity>> owned;
    std::set<const void*> seen;
    std::set<AcDbObjectId> block_path;
    std::vector<std::string> nodes;
    std::vector<SemanticNode> semantics;
    Errors errors;
    std::size_t returned{}, deleted{}, borrowed{}, duplicates{}, unresolved{}, text_count{}, visits{}, dimensions{}, dimension_glyphs{};
    bool ownership_safe{true};
    bool unexpected_exception{};
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
        context.unexpected_exception=true;
        context.errors.push_back(std::string("unexpected dimBlock getter exception: ")+failure.what()); fields.emplace_back("dimBlockReadError", quoted_bytes(failure.what())); return false;
    }
}
void read_node(AcDbEntity& entity, int parent, int depth, const char* origin, ReadContext& context) {
    if (context.visits >= maximum_nodes) { context.errors.push_back("node budget truncated"); return; }
    const int node_id = static_cast<int>(context.visits++);
    const auto errors_before = context.errors.size();
    const auto semantic_index=context.semantics.size();
    Fields fields{{"nodeId", std::to_string(node_id)}, {"parentNodeId", std::to_string(parent)},
        {"depth", std::to_string(depth)}, {"origin", quoted_bytes(origin)},
        {"transient", boolean(detached(entity))}, {"objectIdNull", boolean(entity.objectId().isNull())},
        {"ownerIdNull", boolean(entity.ownerId().isNull())}, {"databaseNull", boolean(entity.database() == nullptr)}};
    try {
        context.semantics.push_back(semantic(entity, origin));
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
                try { danger("before-public-explode"); status = entity.explode(values); }
                catch (...) { take_returns(context, values, &entity, accepted); throw; }
                take_returns(context, values, &entity, accepted);
                fields.emplace_back("explode", object({{"status", std::to_string(status)},
                    {"returnedCount", std::to_string(values.length())}, {"acceptedDetachedCount", std::to_string(accepted.size())}}));
                if(semantic_index<context.semantics.size()) {
                    auto& captured=context.semantics[semantic_index];
                    if(policy::empty_public_placeholder(captured.type, AcDbText::cast(&entity)!=nullptr || AcDbMText::cast(&entity)!=nullptr,
                        captured.boundsStatus,captured.zeroBounds,static_cast<int>(status),static_cast<std::size_t>(values.length()))) {
                        captured.geometryObservable=false;captured.points.clear();
                        fields.emplace_back("publicGeometryObservation", "\"known-empty-placeholder; actual geometry unavailable\"");
                        context.errors.push_back("known empty public TDbMText placeholder; actual geometry unavailable");
                    }
                }
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
    } catch (const std::exception& failure) { context.unexpected_exception=true;context.errors.push_back(std::string("unexpected getter/C++ exception: ")+failure.what()); }
    catch (...) { context.unexpected_exception=true;context.errors.push_back("unknown C++ exception reading node"); }
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

} // namespace bce::native::reader

namespace bce::native {
namespace {
constexpr double tolerance = 1e-6;
void check(Acad::ErrorStatus status, const char* operation) { reader::check(status, operation); }
std::string wide(const wchar_t* text) { return text ? quote(std::wstring(text)) : "null"; }
std::string truth(bool value) { return value ? "true" : "false"; }
template<class T> reader::Opened<T> opened(AcDbObjectId id, AcDb::OpenMode mode = AcDb::kForRead) {
    T* raw{}; const auto status = acdbOpenObject(raw, id, mode); reader::Opened<T> value(raw);
    check(status, "open database-owned object"); require(raw != nullptr, "open returned null"); return value;
}
AcDbObjectId model_id(AcDbDatabase& db) {
    AcDbBlockTable* raw{}; check(db.getBlockTable(raw, AcDb::kForRead), "model table");
    reader::Opened<AcDbBlockTable> table(raw); AcDbObjectId id; check(table->getAt(ACDB_MODEL_SPACE, id), "model owner id"); return id;
}
std::string raw_handle(AcDbObjectId id) {
    wchar_t value[32]{}; id.handle().getIntoAsciiBuffer(value); return utf8(value);
}
struct Snapshot {
    Acad::ErrorStatus gripStatus{Acad::eNotApplicable}, stretchStatus{Acad::eNotApplicable};
    std::vector<AcGePoint3d> grips, stretch;
    std::vector<reader::SemanticNode> nodes;
    std::string json;
    bool complete{};
    bool hardFailure{};
};
std::string points_json(const std::vector<AcGePoint3d>& points) {
    std::vector<std::string> values; for (const auto& point : points) values.push_back(reader::point(point)); return array(values);
}
Snapshot read_snapshot_unchecked(AcDbObjectId id) {
    auto entity = opened<AcDbEntity>(id); Snapshot result;
    AcGePoint3dArray grips, stretch; AcDbIntArray osnaps, geometry;
    reader::danger("before-public-root-grips"); result.gripStatus = entity->getGripPoints(grips, osnaps, geometry);
    reader::danger("before-public-root-stretch-points"); result.stretchStatus = entity->getStretchPoints(stretch);
    if (result.gripStatus == Acad::eOk) for (int i = 0; i < grips.length(); ++i) result.grips.push_back(grips[i]);
    if (result.stretchStatus == Acad::eOk) for (int i = 0; i < stretch.length(); ++i) result.stretch.push_back(stretch[i]);
    reader::ReadContext context; context.seen.insert(entity.get());
    reader::read_node(*entity, -1, 0, "model-root-borrowed", context);
    context.release(); result.nodes = std::move(context.semantics);
    const bool ownership = context.ownership_safe && context.returned == context.deleted;
    result.complete = context.errors.empty() && ownership;
    result.hardFailure=!policy::observation_admissible(ownership,context.unexpected_exception,context.errors);
    result.json = object({{"rootType", wide(entity->isA()->name())}, {"handle", reader::handle(id)},
        {"owner", reader::handle(entity->ownerId())}, {"databaseMatchesId", truth(entity->database() == id.database())},
        {"proxy", truth(entity->isAProxy())}, {"gripStatus", std::to_string(result.gripStatus)}, {"grips", points_json(result.grips)},
        {"stretchStatus", std::to_string(result.stretchStatus)}, {"stretchPoints", points_json(result.stretch)},
        {"nodes", array(context.nodes)}, {"complete", truth(result.complete)}, {"hardObservationFailure",truth(result.hardFailure)},
        {"unexpectedException",truth(context.unexpected_exception)}, {"errors", reader::errors_json(context.errors)},
        {"ownership", object({{"safe", truth(ownership)}, {"returnedDetached", std::to_string(context.returned)},
            {"deletedDetached", std::to_string(context.deleted)}, {"rejectedBorrowed", std::to_string(context.borrowed)},
            {"duplicates", std::to_string(context.duplicates)}, {"unknown", std::to_string(context.unresolved)}})}});
    return result;
}
Snapshot snapshot(AcDbObjectId id) {
    return policy::checked_read([&] {
        auto captured=read_snapshot_unchecked(id);
        if(captured.hardFailure) reader::danger("hard-snapshot-evidence:"+captured.json);
        return captured;
    });
}
bool close_number(double first, double second) { return std::isfinite(first) && std::isfinite(second) && std::abs(first - second) <= tolerance; }
bool same_values(const std::vector<double>& first, const std::vector<double>& second) {
    if (first.size() != second.size()) return false;
    for (std::size_t i = 0; i < first.size(); ++i) if (!close_number(first[i], second[i])) return false;
    return true;
}
bool same_points(const std::vector<AcGePoint3d>& first, const std::vector<AcGePoint3d>& second, const AcGeVector3d& offset) {
    if (first.size() != second.size()) return false;
    for (std::size_t i = 0; i < first.size(); ++i) if ((second[i] - first[i] - offset).length() > tolerance) return false;
    return true;
}
struct Comparison { bool geometry{true}, content{true}, styles{true}; std::vector<std::string> errors; };
Comparison compare(const Snapshot& before, const Snapshot& after, const AcGeVector3d& offset) {
    Comparison result;
    if(before.hardFailure || after.hardFailure) {
        result.geometry=result.content=result.styles=false;
        result.errors.push_back("hard observation failure: ownership violation, unexpected getter status, or caught C++ exception");return result;
    }
    result.geometry = before.gripStatus == after.gripStatus && before.stretchStatus == after.stretchStatus &&
        same_points(before.grips, after.grips, offset) && same_points(before.stretch, after.stretch, offset);
    if (!result.geometry) result.errors.push_back("root grip/stretch geometry mismatch");
    if (before.nodes.size() != after.nodes.size() || before.nodes.empty()) {
        result.geometry = result.content = result.styles = false; result.errors.push_back("public semantic node count mismatch/empty"); return result;
    }
    for (std::size_t i = 0; i < before.nodes.size(); ++i) {
        const auto& a = before.nodes[i]; const auto& b = after.nodes[i];
        const bool geometry = a.type == b.type && a.geometryObservable==b.geometryObservable && a.localCoordinates == b.localCoordinates &&
            same_points(a.points, b.points, a.localCoordinates ? AcGeVector3d::kIdentity : offset);
        const bool content = a.type == b.type && a.text == b.text && same_values(a.values, b.values);
        const bool styles = a.style == b.style;
        result.geometry = result.geometry && geometry; result.content = result.content && content; result.styles = result.styles && styles;
        if (!geometry || !content || !styles) result.errors.push_back("semantic node " + std::to_string(i) + " mismatch: geometry=" + truth(geometry) + " content=" + truth(content) + " styles=" + truth(styles));
    }
    return result;
}
std::string comparison_json(const Comparison& comparison) {
    std::vector<std::string> errors; for (const auto& message : comparison.errors) errors.push_back(quote(message));
    return object({{"geometryEqualWithin1e-6", truth(comparison.geometry)}, {"contentAndMeasurementEqual", truth(comparison.content)},
        {"styleDefinitionsEqual", truth(comparison.styles)}, {"errors", array(errors)}});
}
struct Segment { AcGePoint3d first, second, baseline; double measurement{}; };
std::vector<Segment> segments(AcDbObjectId id) {
    auto entity = opened<AcDbEntity>(id); reader::ReadContext context; context.seen.insert(entity.get());
    AcDbVoidPtrArray values; std::vector<AcDbEntity*> accepted; Acad::ErrorStatus status = Acad::eNotApplicable;
    try { reader::danger("before-dimension-segment-explode"); status = entity->explode(values); }
    catch (...) { reader::take_returns(context, values, entity.get(), accepted); throw; }
    reader::take_returns(context, values, entity.get(), accepted); check(status, "dimension segment explode");
    require(context.ownership_safe, "dimension segment ownership rejected"); std::vector<Segment> result;
    for (auto* value : accepted) {
        auto* dim = AcDbAlignedDimension::cast(value); require(dim != nullptr, "dimension display segment is not aligned dimension");
        double measurement{}; reader::danger("before-segment-measurement"); check(dim->measurement(measurement), "segment measurement");
        result.push_back({dim->xLine1Point(), dim->xLine2Point(), dim->dimLinePoint(), measurement});
    }
    return result;
}
std::vector<double> measurements(const std::vector<Segment>& items) {
    std::vector<double> result; for (const auto& item : items) result.push_back(item.measurement); return result;
}
std::string segments_json(const std::vector<Segment>& items) {
    std::vector<std::string> result;
    for (const auto& item : items) result.push_back(object({{"first", reader::point(item.first)}, {"second", reader::point(item.second)},
        {"baseline", reader::point(item.baseline)}, {"measurement", reader::number(item.measurement)}}));
    return array(result);
}
void move_grip(AcDbObjectId id, int index, const AcGeVector3d& offset) {
    auto entity = opened<AcDbEntity>(id, AcDb::kForWrite); AcDbIntArray indices; indices.append(index);
    reader::danger("before-public-moveGripPointsAt-index-" + std::to_string(index));
    check(entity->moveGripPointsAt(indices, offset), "move public dimension grip");
}
std::string rx_value(const AcRxValue& value, bool& complete) {
    for (std::size_t capacity = 128; capacity <= 1048576; capacity *= 2) {
        std::vector<wchar_t> buffer(capacity, L'\xffff'); reader::danger("before-rx-value-dynamic-format");
        const auto written = value.toString(buffer.data(), capacity);
        const auto end = std::find(buffer.begin(), buffer.end(), L'\0');
        if (written >= 0 && static_cast<std::size_t>(written) < capacity - 1 && end != buffer.end())
            return object({{"formatStatus", std::to_string(written)}, {"capacity", std::to_string(capacity)}, {"truncated", "false"},
                {"text", quote(std::wstring(buffer.begin(), end))}});
    }
    complete = false; return object({{"formatStatus", "null"}, {"truncated", "true"}, {"text", "null"}, {"reason", quote(std::string("dynamic format failed or exceeded 1M wchar budget"))}});
}
struct RxTraversal { std::set<const AcRxMember*> seen; std::vector<std::string> members; bool complete{true}; bool collectionsNotTraversed{}; };
void rx_member(const AcRxMember* member, AcDbEntity& entity, unsigned depth, RxTraversal& traversal) {
    if (!member) { traversal.complete = false; return; }
    if (traversal.members.size() >= 2048 || depth > 8) { traversal.complete = false; return; }
    if (!traversal.seen.insert(member).second) return;
    const auto* property = AcRxProperty::cast(member); const auto* base = AcRxPropertyBase::cast(member);
    const auto* collection = AcRxCollectionProperty::cast(member); const auto* ownerClass = AcRxClass::cast(member->owner());
    const auto* ownerMember = AcRxMember::cast(member->owner());
    Fields fields{{"name", wide(member->name())}, {"depth", std::to_string(depth)}, {"rxClass", wide(member->isA()->name())},
        {"owner", ownerClass ? wide(ownerClass->name()) : ownerMember ? wide(ownerMember->name()) : "null"},
        {"valueType", wide(member->type().name())}, {"readOnly", base ? truth(base->isReadOnly(&entity)) : "null"},
        {"collection", truth(collection != nullptr)}, {"collectionTraversed", "false"}};
    if (collection) { traversal.collectionsNotTraversed = true; traversal.complete = false; }
    if (property) {
        AcRxValue value; reader::danger("before-rx-getValue-" + utf8(member->name())); const auto status = property->getValue(&entity, value);
        fields.emplace_back("getStatus", std::to_string(status)); fields.emplace_back("value", status == Acad::eOk ? rx_value(value, traversal.complete) : "null");
        if (status != Acad::eOk) traversal.complete = false;
    } else { fields.emplace_back("getStatus", "null"); fields.emplace_back("value", "null"); }
    reader::danger("before-rx-children-" + utf8(member->name())); const auto* children = member->children();
    fields.emplace_back("childrenCount", children ? std::to_string(children->length()) : "0"); traversal.members.push_back(object(fields));
    if (children) for (int i = 0; i < children->length(); ++i) {
        if (traversal.members.size() >= 2048) { traversal.complete = false; break; } rx_member((*children)[i], entity, depth + 1, traversal);
    }
}
std::string rx_discovery(AcDbObjectId id) {
    auto entity = opened<AcDbEntity>(id); auto* engine = AcRxMemberQueryEngine::theEngine(); require(engine != nullptr, "RX engine unavailable");
    std::vector<std::string> contexts;
    for (const bool promoted : {false, true}) {
        const AcRxMemberQueryContext* context = promoted ? engine->promotingContext() : nullptr;
        reader::danger(promoted ? "before-rx-promoting-iterator" : "before-rx-default-iterator");
        std::unique_ptr<AcRxMemberIterator> iterator(engine->newMemberIterator(entity.get(), context)); RxTraversal traversal;
        if (!iterator) traversal.complete = false;
        if (iterator) for (; !iterator->done(); iterator->next()) {
            if (traversal.members.size() >= 2048) { traversal.complete = false; break; }
            rx_member(iterator->current(), *entity, 0, traversal);
        }
        contexts.push_back(object({{"context", quote(std::string(promoted ? "promoting" : "default"))},
            {"contextAvailable", truth(!promoted || context != nullptr)}, {"iteratorReturned", truth(iterator != nullptr)},
            {"complete", truth(traversal.complete && (!promoted || context != nullptr))},
            {"collectionsNotTraversed", truth(traversal.collectionsNotTraversed)}, {"members", array(traversal.members)}}));
    }
    return object({{"contexts", array(contexts)}, {"casts", object({{"AcDbText", truth(AcDbText::cast(entity.get()) != nullptr)},
        {"AcDbMText", truth(AcDbMText::cast(entity.get()) != nullptr)}, {"AcDbMLeader", truth(AcDbMLeader::cast(entity.get()) != nullptr)}})},
        {"AcDbAssocValueProviderPE", truth(entity->queryX(AcDbAssocValueProviderPE::desc()) != nullptr)},
        {"valueNamesGuessed", "false"}, {"providerOrOverruleRegistered", "false"}, {"setterAttempted", "false"}});
}
struct SourceGuard {
    AcDbDatabase& scheme; AcDbDatabase& reference; const Config& config;
    std::string state() {
        reader::WorkingDatabaseScope schemeScope(scheme); const auto a = reader::fingerprint(scheme);
        reader::WorkingDatabaseScope referenceScope(reference); const auto b = reader::fingerprint(reference);
        return object({{"scheme", a.json}, {"reference", b.json}, {"schemeFileSHA256", quote(sha256(config.scheme))},
            {"referenceFileSHA256", quote(sha256(config.reference))}});
    }
};

template<class Function> void guarded_step(const std::string& operation, SourceGuard& guard, Evidence& evidence,
    std::vector<std::string>& steps, Function&& function) {
    evidence.checkpoint("before-source-model-state-" + operation);
    const auto before = guard.state();
    evidence.checkpoint("before-" + operation);
    try {
        function();
        const auto after = guard.state();
        steps.push_back(object({{"operation", quote(operation)}, {"sourceBefore", before}, {"sourceAfter", after},
            {"sourcePublicAndFileSHAUnchanged", truth(before == after)}, {"status", quote(std::string("completed"))}}));
        require(before == after, "SOURCE_CHANGED: " + operation);
    } catch (const std::exception& error) {
        const auto after = guard.state();
        steps.push_back(object({{"operation", quote(operation)}, {"sourceBefore", before}, {"sourceAfter", after},
            {"sourcePublicAndFileSHAUnchanged", truth(before == after)}, {"status", quote(std::string("failed"))}, {"error", quote(std::string(error.what()))}}));
        throw;
    }
}
struct Selection { std::wstring type, handle; int count{-1}; AcDbObjectId id; };
std::vector<Selection> select_prototypes(AcDbDatabase& reference) {
    reader::WorkingDatabaseScope working(reference);
    const auto ids = reader::model_ids(reference);
    std::vector<Selection> wanted{{L"TDbDimension2", L"ACB0", 1, {}}, {L"TDbDimension2", L"10837", 2, {}},
        {L"TDbDimension2", L"AC83", 6, {}}, {L"TDbDimension2", L"CAF3", 12, {}},
        {L"TDbSymbElevation", L"", -1, {}}, {L"TDbAxisLabelSet", L"", -1, {}}, {L"TDbDrawingName", L"", -1, {}},
        {L"TDbSymbDrawingIndex", L"", -1, {}}, {L"TDbSymbIndexPointer", L"", -1, {}},
        {L"TDbSymbMultiLeader", L"", -1, {}}, {L"AcDbHatch", L"", -1, {}}};
    for (const auto id : ids) {
        auto entity = opened<AcDbEntity>(id);
        for (auto& choice : wanted) {
            if (!choice.id.isNull() || choice.type != entity->isA()->name()) continue;
            if (!choice.handle.empty() && utf8(choice.handle) != raw_handle(id)) continue;
            choice.id = id;
            const auto selectedHandle = raw_handle(id); choice.handle.assign(selectedHandle.begin(), selectedHandle.end());
        }
    }
    for (const auto& choice : wanted) {
        require(!choice.id.isNull(), "bound reference lacks required prototype " + utf8(choice.type));
        if (choice.count > 0) require(segments(choice.id).size() == static_cast<std::size_t>(choice.count), "selected dimension segment count mismatch");
    }
    return wanted;
}
std::string grip_probes(AcDbObjectId target, PrototypeAssertions& result, SourceGuard& guard, Evidence& evidence, std::vector<std::string>& stepRecords) {
    const auto original = snapshot(target); const auto definitions = segments(target);
    require(definitions.size() == static_cast<std::size_t>(result.dimensionSegments), "grip probe segment count");
    require(original.gripStatus == Acad::eOk && original.grips.size() >= 2, "dimension root grips unavailable");
    const auto delta = definitions.front().second - definitions.front().first;
    require(delta.length() > tolerance, "zero length first segment"); const auto along = delta.normal();
    const auto normal = AcGeVector3d(-along.y, along.x, 0).normal();
    std::vector<AcGePoint3d> measuredPoints;
    for (const auto& part : definitions) { measuredPoints.push_back(part.first); measuredPoints.push_back(part.second); }
    const auto isMeasured = [&](const AcGePoint3d& point) {
        return std::any_of(measuredPoints.begin(), measuredPoints.end(), [&](const auto& candidate) { return candidate.distanceTo(point) <= tolerance; });
    };
    // Derive baseline candidates from the actual exploded dimLinePoints. No fixed legacy index is assumed.
    const auto onBaseline = [&](const AcGePoint3d& point) {
        return std::abs((point - definitions.front().baseline).dotProduct(normal)) <= tolerance && std::abs(point.z - definitions.front().baseline.z) <= tolerance;
    };
    int baselineIndex = -1, otherBaselineIndex = -1;
    for (std::size_t i = 0; i < original.grips.size(); ++i) {
        if (!isMeasured(original.grips[i]) && onBaseline(original.grips[i])) {
            if (baselineIndex < 0) baselineIndex = static_cast<int>(i);
            else if (otherBaselineIndex < 0 && original.grips[i].distanceTo(original.grips[static_cast<std::size_t>(baselineIndex)]) > tolerance)
                otherBaselineIndex = static_cast<int>(i);
        }
    }
    require(baselineIndex >= 0 && otherBaselineIndex >= 0, "cannot match two baseline grips to actual segment definition");
    guarded_step("baseline-grip-normal120", guard, evidence, stepRecords, [&] { move_grip(target, baselineIndex, normal * 120); });
    const auto movedBaseline = snapshot(target); const auto movedDefinitions = segments(target);
    const auto base = static_cast<std::size_t>(baselineIndex), other = static_cast<std::size_t>(otherBaselineIndex);
    bool baselineVerified = movedBaseline.grips.size() == original.grips.size() &&
        (movedBaseline.grips[base] - original.grips[base] - normal * 120).length() <= tolerance &&
        (movedBaseline.grips[other] - original.grips[other] - normal * 120).length() <= tolerance &&
        same_values(measurements(definitions), measurements(movedDefinitions));
    if (movedDefinitions.size() == definitions.size()) for (std::size_t i = 0; i < definitions.size(); ++i) {
        baselineVerified = baselineVerified && definitions[i].first.distanceTo(movedDefinitions[i].first) <= tolerance &&
            definitions[i].second.distanceTo(movedDefinitions[i].second) <= tolerance &&
            (movedDefinitions[i].baseline - definitions[i].baseline - normal * 120).length() <= tolerance;
    } else baselineVerified = false;
    guarded_step("baseline-grip-restore", guard, evidence, stepRecords, [&] { move_grip(target, baselineIndex, normal * -120); });
    const auto baselineRestored = snapshot(target); const auto baselineCompare = compare(original, baselineRestored, AcGeVector3d::kIdentity);
    require(baselineVerified && baselineCompare.geometry && baselineCompare.content && baselineCompare.styles, "baseline120 or complete grip/measurement restoration failed");
    result.baselineGrip120 = true;
    Fields shared{{"attempted", truth(result.dimensionSegments > 1)}};
    if (result.dimensionSegments > 1) {
        std::vector<policy::Segment> actualDefinitions;std::vector<policy::Point> actualGrips;
        const auto projected=[](const AcGePoint3d& point){return policy::Point{point.x,point.y,point.z};};
        for(const auto& part:definitions)actualDefinitions.push_back({{projected(part.first),projected(part.second)}});
        for(const auto& grip:original.grips)actualGrips.push_back(projected(grip));
        const auto match=policy::find_shared(actualDefinitions,actualGrips);
        require(match.found, "no actual shared measurement grip matched to two segment definitions");
        const auto sharedIndex=static_cast<int>(match.grip);const auto left=match.first,right=match.second;
        const AcGeVector3d sharedAlong(match.direction.x,match.direction.y,match.direction.z);
        guarded_step("shared-measurement-grip40", guard, evidence, stepRecords, [&] { move_grip(target, sharedIndex, sharedAlong * 40); });
        const auto changed = segments(target); bool linked = changed.size() == definitions.size();
        if (linked) for (std::size_t i = 0; i < definitions.size(); ++i) {
            const double expected = i == left ? 40 : i == right ? -40 : 0;
            linked = linked && close_number(changed[i].measurement - definitions[i].measurement, expected);
        }
        guarded_step("shared-measurement-grip-restore", guard, evidence, stepRecords, [&] { move_grip(target, sharedIndex, sharedAlong * -40); });
        const auto restored = snapshot(target); const auto restoration = compare(original, restored, AcGeVector3d::kIdentity);
        require(linked && restoration.geometry && restoration.content && restoration.styles &&
            same_values(measurements(definitions), measurements(segments(target))), "shared40 +40/-40 or all-grip restoration failed");
        result.sharedGrip40 = true;
        shared.insert(shared.end(), {{"indexMatchedFromDefinition", std::to_string(sharedIndex)}, {"leftSegment", std::to_string(left)},
            {"rightSegment", std::to_string(right)}, {"leftSharedEndpoint",std::to_string(match.firstEndpoint)},
            {"rightSharedEndpoint",std::to_string(match.secondEndpoint)}, {"outward40Direction",reader::vector(sharedAlong)},
            {"changedSegments", segments_json(changed)}, {"restored", "true"}});
    }
    return object({{"baselineIndexMatchedFromDefinition", std::to_string(baselineIndex)},
        {"otherBaselineIndex", std::to_string(otherBaselineIndex)}, {"normalOffset", reader::vector(normal * 120)},
        {"baselineBefore", original.json}, {"baselineMoved", movedBaseline.json}, {"baselineRestored", baselineRestored.json},
        {"segmentsBefore", segments_json(definitions)}, {"segmentsBaselineMoved", segments_json(movedDefinitions)}, {"shared40", object(shared)}});
}
PrototypeAssertions probe_one(const Selection& selected, unsigned index, SourceGuard& guard, Evidence& evidence) {
    PrototypeAssertions result; result.type = utf8(selected.type); result.dimensionSegments = selected.count; result.implemented = true;
    std::vector<std::string> stepRecords, failures;
    Fields details{{"sourceHandleBoundToReferenceSHA", quote(selected.handle)}, {"requestedType", quote(selected.type)},
        {"requestedSegments", std::to_string(selected.count)}, {"duplicateRecordPolicy", quote(std::string("kDrcMangleName: probe only"))}};
    const auto sourceBefore = guard.state(); Snapshot sourceSnapshot;
    try {
        policy::ExecutionGuard execution(result.executionFailed);
        guarded_step("source-public-observation", guard, evidence, stepRecords, [&] {
            reader::WorkingDatabaseScope working(guard.reference); sourceSnapshot = snapshot(selected.id);
            result.hardObservationFailure=result.hardObservationFailure || sourceSnapshot.hardFailure;
            details.emplace_back("sourceObservationBefore", sourceSnapshot.json);
        });
        AcDbDatabase target(true, true); reader::WorkingDatabaseScope targetWorking(target); AcDbObjectId clone;
        guarded_step("wblockCloneObjects", guard, evidence, stepRecords, [&] {
            auto sourceRead = opened<AcDbEntity>(selected.id); require(!sourceRead->isWriteEnabled(), "source object unexpectedly write-open");
            const auto owner = model_id(target); AcDbObjectIdArray ids; ids.append(selected.id); AcDbIdMapping mapping;
            check(guard.reference.wblockCloneObjects(ids, owner, mapping, AcDb::kDrcMangleName), "clone prototype into independent target");
            AcDbIdPair pair; pair.setKey(selected.id); require(mapping.compute(pair) && pair.isCloned() && !pair.value().isNull(), "clone primary mapping missing");
            clone = pair.value(); auto entity = opened<AcDbEntity>(clone);
            require(entity->ownerId() == owner && entity->database() == &target && !entity->isAProxy() && selected.type == entity->isA()->name(), "clone class/owner/database/proxy mismatch");
            AcDbDatabase* destination{}; check(mapping.destDb(destination), "mapping destination db"); require(destination == &target, "mapping destination database mismatch");
            std::vector<std::string> mappings; AcDbIdMappingIter iterator(mapping);
            for (; !iterator.done(); iterator.next()) { AcDbIdPair item; require(iterator.getMap(item), "clone map iterator failure");
                require(mappings.size() < 50000, "clone mapping node budget exceeded");
                mappings.push_back(object({{"source", reader::handle(item.key())}, {"target", reader::handle(item.value())},
                    {"cloned", truth(item.isCloned())}, {"primary", truth(item.isPrimary())}, {"ownerTranslated", truth(item.isOwnerXlated())}})); }
            details.emplace_back("mapping", array(mappings)); result.cloneMapped = result.nativeClassAndOwner = true;
        });
        Snapshot original;
        guarded_step("clone-public-baseline", guard, evidence, stepRecords, [&] {
            original = snapshot(clone); details.emplace_back("cloneBefore", original.json);
            result.hardObservationFailure=result.hardObservationFailure || original.hardFailure;
            const auto comparison = compare(sourceSnapshot, original, AcGeVector3d::kIdentity); details.emplace_back("sourceToCloneComparison", comparison_json(comparison));
            require(comparison.geometry && comparison.content && comparison.styles, "clone public geometry/content/style mismatch");
        });
        guarded_step("rigid-X-translation-1234", guard, evidence, stepRecords, [&] {
            { auto entity = opened<AcDbEntity>(clone, AcDb::kForWrite); check(entity->transformBy(AcGeMatrix3d::translation(AcGeVector3d(1234, 0, 0))), "translate cloned prototype"); }
            const auto moved = snapshot(clone); const auto comparison = compare(original, moved, AcGeVector3d(1234, 0, 0));
            result.hardObservationFailure=result.hardObservationFailure || moved.hardFailure;
            details.emplace_back("translated", moved.json); details.emplace_back("translationComparison", comparison_json(comparison));
            require(comparison.geometry && comparison.content && comparison.styles, "rigid translation public geometry/content/style mismatch"); result.rigidTranslation1234 = true;
        });
        guarded_step("restore-X-translation", guard, evidence, stepRecords, [&] {
            { auto entity = opened<AcDbEntity>(clone, AcDb::kForWrite); check(entity->transformBy(AcGeMatrix3d::translation(AcGeVector3d(-1234, 0, 0))), "restore clone translation"); }
            const auto restored = snapshot(clone); const auto comparison = compare(original, restored, AcGeVector3d::kIdentity);
            result.hardObservationFailure=result.hardObservationFailure || restored.hardFailure;
            details.emplace_back("rigidRestoreComparison", comparison_json(comparison)); require(comparison.geometry && comparison.content && comparison.styles, "rigid restore mismatch"); result.restoreBaseline = true;
        });
        if (selected.count > 0) guarded_step("public-dimension-grip-tests", guard, evidence, stepRecords, [&] { details.emplace_back("dimensionGripProbe", grip_probes(clone, result, guard, evidence, stepRecords)); });
        guarded_step("public-RX-default-promoting-children", guard, evidence, stepRecords, [&] { details.emplace_back("rx", rx_discovery(clone)); });
        guarded_step("save-and-independent-reopen", guard, evidence, stepRecords, [&] {
            const auto output = guard.config.output + L"\\prototype-" + std::to_wstring(index) + L"-roundtrip.dwg";
            require(GetFileAttributesW(output.c_str()) == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND, "new DWG output already exists or is inaccessible");
            const auto savedHandle = clone.handle(); check(target.saveAs(output.c_str(), false, AcDb::kDHL_CURRENT), "save independent cloned prototype");
            evidence.checkpoint("before-independent-saved-DWG-open"); AcDbDatabase reopened(false, true);
            check(reopened.readDwgFile(output.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read independently saved prototype");
            check(reopened.closeInput(true), "close saved input stream"); reader::WorkingDatabaseScope reopenWorking(reopened);
            AcDbObjectId reopenedId; check(reopened.getAcDbObjectId(reopenedId, false, savedHandle), "new file target handle lookup");
            { auto entity = opened<AcDbEntity>(reopenedId); require(entity->ownerId() == model_id(reopened) && entity->database() == &reopened &&
                !entity->isAProxy() && selected.type == entity->isA()->name(), "reopened class/owner/database/proxy mismatch"); }
            const auto readback = snapshot(reopenedId); const auto comparison = compare(original, readback, AcGeVector3d::kIdentity);
            result.hardObservationFailure=result.hardObservationFailure || readback.hardFailure;
            details.emplace_back("savedFile", object({{"path", quote(output)}, {"sha256", quote(sha256(output))}, {"savedTargetHandle", reader::handle(clone)},
                {"reopenedTargetHandle", reader::handle(reopenedId)}, {"rootCount", std::to_string(reader::model_ids(reopened).size())}}));
            details.emplace_back("reopenedObservation", readback.json); details.emplace_back("reopenComparison", comparison_json(comparison));
            require(comparison.geometry && comparison.content && comparison.styles, "saved independent readback mismatch");
            result.saveReopen = true; result.textAndMeasurementPreserved = comparison.content; result.stylesReadBack = comparison.styles;
            result.observationComplete = sourceSnapshot.complete && original.complete && readback.complete;
        });
        execution.commit();
    } catch (const std::exception& failure) { result.executionFailed=true;failures.push_back(quote(std::string(failure.what()))); }
    catch (...) { result.executionFailed=true;failures.push_back(quote(std::string("unknown C++ prototype exception"))); }
    try {
        policy::ExecutionGuard execution(result.executionFailed);
        reader::WorkingDatabaseScope working(guard.reference); const auto sourceAfter = snapshot(selected.id);
        result.hardObservationFailure=result.hardObservationFailure || sourceAfter.hardFailure;
        const auto comparison = compare(sourceSnapshot, sourceAfter, AcGeVector3d::kIdentity);
        result.sourceUnchanged = sourceBefore == guard.state() && comparison.geometry && comparison.content && comparison.styles;
        details.emplace_back("sourceObservationAfter", sourceAfter.json); details.emplace_back("sourceBeforeAfterComparison", comparison_json(comparison));
        execution.commit();
    } catch (const std::exception& failure) { result.executionFailed=true;failures.push_back(quote(std::string(failure.what()))); }
    catch (...) { result.executionFailed=true;failures.push_back(quote(std::string("unknown final source observation exception"))); }
    details.emplace_back("sourcePublicAndSHAUnchanged", truth(result.sourceUnchanged));
    details.emplace_back("steps", array(stepRecords)); details.emplace_back("errors", array(failures));
    details.emplace_back("publicSnapshotComplete", truth(result.observationComplete));
    details.emplace_back("executionFailed",truth(result.executionFailed));details.emplace_back("hardObservationFailure",truth(result.hardObservationFailure));
    result.details = object(details);
    const auto path = guard.config.output + L"\\prototype-" + std::to_wstring(index) + L".json";
    auto file = create_new(path); write_flushed(file.get(), result.details);
    if (!failures.empty()) evidence.error(result.type + ": " + array(failures));
    return result;
}
} // namespace

std::vector<PrototypeAssertions> probe_prototypes(const Config& config, Evidence& evidence) {
    reader::checkpointSink = &evidence;
    struct CheckpointScope { ~CheckpointScope() noexcept { reader::checkpointSink = nullptr; } } checkpointScope;
    AcDbDatabase scheme(false, true), reference(false, true);
    evidence.checkpoint("before-read-only-scheme-SideDB"); check(scheme.readDwgFile(config.scheme.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read scheme source"); check(scheme.closeInput(true), "close scheme source input");
    evidence.checkpoint("before-read-only-reference-SideDB"); check(reference.readDwgFile(config.reference.c_str(), AcDbDatabase::kForReadAndAllShare, false), "read reference source"); check(reference.closeInput(true), "close reference source input");
    SourceGuard guard{scheme, reference, config}; const auto before = guard.state();
    evidence.checkpoint("before-prototype-selection"); const auto selected = select_prototypes(reference);
    std::vector<PrototypeAssertions> results;
    for (std::size_t i = 0; i < selected.size(); ++i) results.push_back(probe_one(selected[i], static_cast<unsigned>(i), guard, evidence));
    require(before == guard.state(), "source states differ after all prototype probes");
    evidence.step("sourceModelAndFileSHA", "passed", "all observed source root state fingerprints and bound source SHA remain identical");
    return results;
}
bool assert_prototype_roundtrip(const Config& config, Evidence& evidence) {
    const auto results = probe_prototypes(config, evidence); bool all = results.size() == 11;
    for (const auto& item : results) {
        const bool core = item.cloneMapped && item.nativeClassAndOwner && item.rigidTranslation1234 && item.restoreBaseline && item.saveReopen &&
            item.textAndMeasurementPreserved && item.stylesReadBack && item.sourceUnchanged;
        const bool grips = item.dimensionSegments < 0 || (item.baselineGrip120 && (item.dimensionSegments == 1 || item.sharedGrip40));
        const bool assertions=policy::assertions_allowed(core,grips,item.executionFailed,item.hardObservationFailure);
        all = all && assertions;
        const auto state = [&](bool value) { return quote(std::string(value ? "passed-public-observation" : "failed-or-unproven")); };
        evidence.capability(object({{"type", quote(item.type)}, {"segments", std::to_string(item.dimensionSegments)},
            {"clone", state(item.cloneMapped && item.nativeClassAndOwner)}, {"rigidTranslation", state(item.rigidTranslation1234)},
            {"rigidRestoration", state(item.restoreBaseline)}, {"saveReopen", state(item.saveReopen)},
            {"observedTextAndMeasurementPreserved", state(item.textAndMeasurementPreserved)}, {"styleDefinitionReadback", state(item.stylesReadBack)},
            {"baselineGrip120", item.dimensionSegments > 0 ? state(item.baselineGrip120) : quote(std::string("notApplicable"))},
            {"sharedGrip40", item.dimensionSegments > 1 ? state(item.sharedGrip40) : quote(std::string("notApplicable"))},
            {"sourcePublicAndSHAUnchanged", truth(item.sourceUnchanged)}, {"publicSnapshotComplete", truth(item.observationComplete)},
            {"geometryVerificationScope",quote(std::string("observable-public-geometry-only; empty custom placeholders retain raw status/bounds and are not actual glyph geometry"))},
            {"fullPrivateGeometryVerified","false"},
            {"executionFailed",truth(item.executionFailed)}, {"hardObservationFailure",truth(item.hardObservationFailure)},
            {"businessValueModification", quote(std::string("unsupported-no-enumerated-whitelisted-setter"))},
            {"assertionsPassed", truth(assertions)}, {"detailFileIndex", std::to_string(&item - results.data())}}));
        if (!assertions) evidence.error("NATIVE_ASSERTION_UNPROVEN: " + item.type + ":" + std::to_string(item.dimensionSegments));
    }
    evidence.capability(object({{"type", quote(std::string("TDbDimension2"))}, {"segments", "13"},
        {"status", quote(std::string("missing-reference-prototype"))}, {"mergeExecuted", "false"}, {"connectionValidation", quote(std::string("pending"))}}));
    return all;
}
} // namespace bce::native

