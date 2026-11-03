#include "TestSupport.h"
#include "../src/ProjectFacts.h"
#include "../src/GenerationState.h"
#include <cmath>
#include <limits>

namespace bce::test {
namespace {
ProjectFactConfirmation confirmation() {
    ProjectFactConfirmation input;
    input.persistentProjectId = "actual-project";
    input.units = {25.4, "user:inches"};
    input.read = {"old-document", "old-database", 9};
    input.modelSpaceOwnerHandle = "1F";
    input.title = {"A1", "1F", "AcDbText", true, true, input.read, FactText{}};
    std::get<FactText>(input.title.definition).text = u8"东立面";
    auto levelName = input.title;
    levelName.rootHandle = "A2";
    std::get<FactText>(levelName.definition).text = u8"首层基准";
    auto levelValue = levelName; levelValue.rootHandle = "A3";
    std::get<FactText>(levelValue.definition).text = "-1.250";
    input.levelDeclarations = {{"floor-name", levelName}, {"level-value", levelValue}};
    input.datumSource = {"B3", "1F", "AcDbLine", true, true, input.read,
        FactLine{{100, 200, 0}, {300, 400, 0}}};
    input.pointSelector = DatumPointSelector::lineEnd;
    input.datumMeters = -1.25;
    input.datumConfirmationSource = "user:actual-level";
    input.heightDirection = {-3, 4, 0};
    input.window = {{20, 30, 0}, {0.6, 0.8, 0}, {-0.8, 0.6, 0}, 50, 20};
    input.selectionConfirmationSource = "user:selected-elevation-only";
    return input;
}
ProjectFacts facts(const ProjectFactConfirmation& input) {
    const auto result = confirmProjectFacts(input);
    require(result.value.has_value() && result.issues.empty(), "valid actual facts must be confirmable");
    return *result.value;
}
ProjectFactObservation observation(const ProjectFactConfirmation& input) {
    ProjectFactObservation current{true, input.persistentProjectId, input.units,
        {"reopened-document", "reopened-database", 1}, input.modelSpaceOwnerHandle,
        {input.title, input.levelDeclarations[0].source, input.datumSource, input.levelDeclarations[1].source}};
    for (auto& source : current.sources) { source.read = current.read; }
    return current;
}
bool near(double left, double right) { return std::abs(left - right) < 1e-10; }
void pointIs(FactPoint3 actual, FactPoint3 expected, const char* message) {
    require(near(actual.x, expected.x) && near(actual.y, expected.y) && near(actual.z, expected.z), message);
}
void located(const FactSubsetCheck& check, FactSubsetStatus status, const char* handle, const char* code) {
    require(check.status == status, "fact status must describe the changed dependency");
    require(!check.issues.empty() && check.issues.front().code == code, "diagnostic must identify change kind");
    const auto& source = check.issues.front().source;
    require(source.entity && source.entity->handle == handle && !check.issues.front().message.empty(),
        "dependency diagnostic must retain selected source handle");
}
}

void factSelectorsReadActualDefinitionPoints() {
    auto input = confirmation();
    input.pointSelector = DatumPointSelector::lineStart;
    pointIs(facts(input).datumPoint, {100, 200, 0}, "line start must come from typed start");
    input.pointSelector = DatumPointSelector::lineEnd;
    pointIs(facts(input).datumPoint, {300, 400, 0}, "line end must come from typed end");
    input.datumSource.exactClass = "AcDbAlignedDimension";
    input.datumSource.definition = FactAlignedDimension{{7, 8, 0}, {12, 13, 0}};
    input.pointSelector = DatumPointSelector::dimXLine1;
    pointIs(facts(input).datumPoint, {7, 8, 0}, "dimension xLine1 must not use displayed text or measurement");
    input.pointSelector = DatumPointSelector::dimXLine2;
    pointIs(facts(input).datumPoint, {12, 13, 0}, "dimension xLine2 must be explicit");
    input.pointSelector = DatumPointSelector::lineEnd;
    const auto wrong = confirmProjectFacts(input);
    require(hasCode(wrong, "FACT_SELECTOR_UNSUPPORTED") && !wrong.value, "selector must match exact source class");
    input.pointSelector = static_cast<DatumPointSelector>(99);
    require(!confirmProjectFacts(input).value, "unknown selector must not default to a point");
}
void factElevationUsesConfirmedUnitsDirectionAndDatum() {
    auto input = confirmation();
    const auto original = facts(input);
    const auto elevation = projectFactElevationMeters(original, {294, 408, 0});
    require(elevation.value && near(*elevation.value, -0.996), "10 input inches above nonzero datum in explicit direction");
    pointIs(original.unitHeightDirection, {-0.6, 0.8, 0}, "height direction must be normalized");
    auto& line = std::get<FactLine>(input.datumSource.definition);
    line.start.x += 10000; line.start.y -= 7000;
    line.end.x += 10000; line.end.y -= 7000;
    const auto moved = projectFactElevationMeters(facts(input), {10294, -6592, 0});
    require(moved.value && near(*moved.value, *elevation.value), "translation must retain actual elevation");
    require(!projectFactElevationMeters(original, {294, 408, 1}).value, "off-plane measurement is unsupported");
    input = confirmation();
    input.units.mmPerInputUnit = 1e-321;
    input.datumMeters = 0;
    input.heightDirection = {1, 0, 0};
    input.pointSelector = DatumPointSelector::lineStart;
    std::get<FactLine>(input.datumSource.definition).start = {};
    const auto tinyScale = projectFactElevationMeters(facts(input), {1e308, 0, 0});
    require(tinyScale.value && std::abs(*tinyScale.value / 9.98012604599318e-17 - 1.0) < 1e-12,
        "representable actual elevation must not become zero through intermediate unit underflow");
}
void factRotatedWindowRetainsFourCorners() {
    const auto confirmed = facts(confirmation());
    pointIs(confirmed.windowCorners[0], {20, 30, 0}, "window origin");
    pointIs(confirmed.windowCorners[1], {50, 70, 0}, "rotated U corner");
    pointIs(confirmed.windowCorners[2], {34, 82, 0}, "rotated opposite corner");
    pointIs(confirmed.windowCorners[3], {4, 42, 0}, "rotated V corner must not be WCS bounding box");
    auto input = confirmation();
    input.window.v = input.window.u;
    require(hasCode(confirmProjectFacts(input), "FACT_WINDOW_UNSUPPORTED"), "sheared/degenerate window rejected");
    input = confirmation();
    input.window.origin = {1e16, 0, 0};
    input.window.width = 1; input.window.height = 1;
    require(hasCode(confirmProjectFacts(input), "FACT_WINDOW_UNSUPPORTED"),
        "four distinct adjacent corners that round to a line cannot form the selected rectangle");
    input.window.width = 4; input.window.height = 4;
    require(hasCode(confirmProjectFacts(input), "FACT_WINDOW_UNSUPPORTED"),
        "noncollinear rounded corners must still retain the selected rectangle dimensions and directions");
    input.window.u = {1, 0, 0}; input.window.v = {0, 1, 0};
    input.window.width = 2; input.window.height = 2;
    pointIs(facts(input).windowCorners[2], {1e16 + 2, 2, 0}, "exactly representable large-origin rectangle remains supported");
}
void factChangesLocateOnlyRequiredSubset() {
    const auto input = confirmation();
    const auto history = facts(input);
    auto current = observation(input);
    std::get<FactLine>(current.sources[2].definition).start.x += 1;
    located(checkProjectFacts(history, current), FactSubsetStatus::stale, "B3", "FACT_DEFINITION_CHANGED");
    current = observation(input);
    std::get<FactText>(current.sources[0].definition).text += " revised";
    located(checkProjectFacts(history, current), FactSubsetStatus::stale, "A1", "FACT_DEFINITION_CHANGED");
    current = observation(input);
    std::get<FactText>(current.sources[1].definition).alignmentPoint.y += 1;
    located(checkProjectFacts(history, current), FactSubsetStatus::stale, "A2", "FACT_DEFINITION_CHANGED");
    current = observation(input);
    std::get<FactText>(current.sources[3].definition).text = "0.000";
    located(checkProjectFacts(history, current), FactSubsetStatus::stale, "A3", "FACT_DEFINITION_CHANGED");
    current = observation(input); current.sources[2].effectiveVisible = false;
    located(checkProjectFacts(history, current), FactSubsetStatus::unsupported, "B3", "FACT_SOURCE_UNSUPPORTED");
    current = observation(input); current.sources[2].ownerHandle = "2F";
    located(checkProjectFacts(history, current), FactSubsetStatus::stale, "B3", "FACT_OWNER_CHANGED");
    current = observation(input); current.units.mmPerInputUnit = 1;
    require(checkProjectFacts(history, current).status == FactSubsetStatus::stale, "changed unit scale stales facts");
    current = observation(input); current.units.confirmationSource = "different confirmation";
    require(checkProjectFacts(history, current).status == FactSubsetStatus::stale, "unit provenance change stales facts");
}
void factReopenRequiresFreshConsistentObservation() {
    const auto input = confirmation();
    const auto history = facts(input);
    auto current = observation(input);
    current.sources[2].rootHandle = "000b3";
    current.sources[2].ownerHandle = "001f";
    require(checkProjectFacts(history, current).status == FactSubsetStatus::currentSubsetMatches,
        "fresh same subset after reopen matches despite new runtime identity and revision");
    require(checkProjectFacts(history, std::nullopt).status == FactSubsetStatus::stale, "history alone is not a current read");
    current.readCompleted = false;
    require(checkProjectFacts(history, current).status == FactSubsetStatus::stale, "incomplete observation is not current");
    current = observation(input); current.sources[1].read.revision++;
    located(checkProjectFacts(history, current), FactSubsetStatus::unsupported, "A2", "FACT_READ_INVALID");
    current = observation(input); current.read.revision = 0;
    for (auto& source : current.sources) { source.read = current.read; }
    const auto zeroRevision = checkProjectFacts(history, current);
    require(zeroRevision.status == FactSubsetStatus::unsupported && !zeroRevision.issues.empty() &&
        zeroRevision.issues.front().code == "FACT_READ_INVALID", "even a consistent zero revision is not a valid current read");
    current = observation(input); current.sources.erase(current.sources.begin() + 2);
    located(checkProjectFacts(history, current), FactSubsetStatus::stale, "B3", "FACT_SOURCE_MISSING");
    current = observation(input); current.persistentProjectId = "another-project";
    require(checkProjectFacts(history, current).status == FactSubsetStatus::orphan, "project ID mismatch cannot rebind history");
    current.persistentProjectId.clear();
    require(checkProjectFacts(history, current).status == FactSubsetStatus::orphan, "removed project leaves orphan facts");
}
void factUnsupportedSourcesAndNonfiniteValuesAreRejected() {
    const auto baseline = confirmation();
    (void)facts(baseline);
    auto input = baseline;
    input.read.revision = 0; input.title.read = input.read; input.datumSource.read = input.read;
    for (auto& level : input.levelDeclarations) { level.source.read = input.read; }
    require(!confirmProjectFacts(input).value, "zero revision cannot establish confirmed read provenance");
    input = baseline;
    input.datumSource.exactClass = "TDbDimension";
    require(hasCode(confirmProjectFacts(input), "FACT_SOURCE_UNSUPPORTED"), "TCH/derived class is not exact supported source");
    input = baseline; input.datumSource.directRoot = false;
    require(!confirmProjectFacts(input).value, "nested source cannot act as direct definition point");
    input = baseline; std::get<FactText>(input.title.definition).hasFields = true;
    require(!confirmProjectFacts(input).value, "field-bearing title cannot silently match raw text");
    input = baseline; input.heightDirection = {};
    require(!confirmProjectFacts(input).value, "zero direction cannot normalize");
    input = baseline; input.datumMeters = std::numeric_limits<double>::infinity();
    require(!confirmProjectFacts(input).value, "nonfinite datum rejected");
    input = baseline; std::get<FactLine>(input.datumSource.definition).end.z = 1;
    require(!confirmProjectFacts(input).value, "nonplanar line rejected");
    input = baseline; input.window.width = std::numeric_limits<double>::max();
    input.window.origin.x = std::numeric_limits<double>::max();
    require(!confirmProjectFacts(input).value, "overflowing corners rejected");
    for (const auto* malformed : {"0", "0xB3", "-1", " B3", "10000000000000000"}) {
        input = baseline; input.datumSource.rootHandle = malformed;
        require(!confirmProjectFacts(input).value, "malformed/overflowing handle rejected");
    }
}
void factObservationRejectsAmbiguityAndUnsupportedChanges() {
    const auto input = confirmation();
    const auto history = facts(input);
    auto current = observation(input);
    current.sources.push_back(current.sources[2]); current.sources.back().rootHandle = "00b3";
    require(checkProjectFacts(history, current).status == FactSubsetStatus::unsupported, "duplicate canonical source is ambiguous");
    current = observation(input); current.sources[2].exactClass = "AcDbAlignedDimension";
    located(checkProjectFacts(history, current), FactSubsetStatus::unsupported, "B3", "FACT_SOURCE_UNSUPPORTED");
    current = observation(input); std::get<FactText>(current.sources[0].definition).rotation = std::numeric_limits<double>::quiet_NaN();
    located(checkProjectFacts(history, current), FactSubsetStatus::unsupported, "A1", "FACT_SOURCE_UNSUPPORTED");
    auto damaged = history; damaged.datumPoint.x++;
    require(checkProjectFacts(damaged, observation(input)).status == FactSubsetStatus::unsupported,
        "malformed historical derived point cannot be trusted");
}
void factSubsetMatchNeverAuthorizesGeneration() {
    const auto input = confirmation();
    const auto history = facts(input);
    auto current = observation(input);
    // An unrelated source is outside the three selected dependencies.
    auto other = current.sources[2]; other.rootHandle = "CAFE";
    std::get<FactLine>(other.definition).end.x = -12345;
    current.sources.push_back(other);
    require(checkProjectFacts(history, current).status == FactSubsetStatus::currentSubsetMatches,
        "unrelated building edit is not a claim of complete scene equality");
    ProjectConfig config;
    ObservedScene unqualified;
    GenerationRequest request;
    require(!planGenerationCommand(unqualified, config, request).value,
        "finite subset match cannot supply complete generation qualification");
    auto dimensionInput = input;
    dimensionInput.datumSource.exactClass = "AcDbAlignedDimension";
    dimensionInput.datumSource.definition = FactAlignedDimension{{1, 2, 0}, {3, 4, 0}};
    dimensionInput.pointSelector = DatumPointSelector::dimXLine1;
    require(checkProjectFacts(facts(dimensionInput), observation(dimensionInput)).status == FactSubsetStatus::currentSubsetMatches,
        "dimension subset does not require measurement, displayed text or style fingerprints");
}
} // namespace bce::test
