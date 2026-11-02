#include "ReferenceCapturePolicy.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::string encode_number(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(17) << value;
    return stream.str();
}
std::string field(const std::vector<std::pair<std::string, std::string>>& fields, const char* name) {
    for (const auto& value : fields) if (value.first == name) return value.second;
    throw std::runtime_error(std::string("missing serialized field: ") + name);
}
void test_hatch_empty_bulges_are_valid_straight_segments() {
    expect(bce_capture::hatch_polyline_counts_valid(4, 0), "all-straight ring may omit every bulge");
    expect(bce_capture::hatch_polyline_counts_valid(4, 4), "one bulge per vertex must remain valid");
    expect(!bce_capture::hatch_polyline_counts_valid(4, 3), "nonempty partial bulge data must remain invalid");
    expect(!bce_capture::hatch_polyline_counts_valid(0, 1), "bulges without vertices must remain invalid");
    expect(bce_capture::hatch_polyline_counts_valid(0, 0), "empty arrays have no count mismatch");
}
void test_rotated_arc_serialization_preserves_geometry() {
    // Explicit physical points: quarter circle centered (3,5), +Y reference,
    // from (3,7) to (1,5), counterclockwise. The old projection loses the +Y basis.
    const double half_pi = std::acos(-1.0) / 2.0;
    const bce_capture::CircularArc arc{{3, 5}, 2, 0, half_pi, false, {0, 1}, {3, 7}, {1, 5}};
    expect(bce_capture::circular_arc_endpoints_consistent(arc), "valid physical endpoints rejected");
    const auto fields = bce_capture::circular_arc_fields(arc, encode_number);
    expect(field(fields, "referenceVector") == "[0,1]", "nondefault reference vector lost");
    expect(field(fields, "startPoint") == "[3,7]", "physical start point changed");
    expect(field(fields, "endPoint") == "[1,5]", "physical end point changed");
    const auto start_angle = std::stod(field(fields, "planeStartAngle"));
    const auto end_angle = std::stod(field(fields, "planeEndAngle"));
    expect(std::abs(3 + 2 * std::cos(start_angle) - 3) < 1e-12 &&
        std::abs(5 + 2 * std::sin(start_angle) - 7) < 1e-12, "serialized start cannot reconstruct actual point");
    expect(std::abs(3 + 2 * std::cos(end_angle) - 1) < 1e-12 &&
        std::abs(5 + 2 * std::sin(end_angle) - 5) < 1e-12, "serialized end cannot reconstruct actual point");
    auto inconsistent = arc;
    inconsistent.end_point = {2, 5};
    expect(!bce_capture::circular_arc_endpoints_consistent(inconsistent), "endpoint not on circle must be rejected");
}
void test_two_large_loops_rejected_cumulatively() {
    bce_capture::LeafBudget budget(65536);
    expect(budget.try_consume(40000), "first loop should fit");
    expect(!budget.try_consume(40000), "40000+40000 must exceed whole-leaf limit");
    expect(budget.used() == 40000, "rejected allocation must not change consumed budget");
}
void test_exact_boundary_then_one_rejected() {
    bce_capture::LeafBudget budget(65536);
    expect(budget.try_consume(65536), "exact limit must fit");
    expect(!budget.try_consume(1), "one after exact limit must be rejected");
    expect(budget.used() == 65536, "exact limit consumed count changed");
}
void test_size_t_overflow_rejected() {
    const auto maximum = (std::numeric_limits<std::size_t>::max)();
    bce_capture::LeafBudget budget(maximum);
    expect(budget.try_consume(maximum - 2), "large first count should fit");
    expect(!budget.try_consume(4), "size_t wraparound must be rejected");
    expect(budget.used() == maximum - 2, "overflow rejection changed consumed count");
}
} // namespace
int main() {
    struct Test { const char* name; void (*run)(); };
    const Test tests[]{
        {"hatch_empty_bulges_are_valid_straight_segments", test_hatch_empty_bulges_are_valid_straight_segments},
        {"rotated_arc_serialization_preserves_geometry", test_rotated_arc_serialization_preserves_geometry},
        {"two_large_loops_rejected_cumulatively", test_two_large_loops_rejected_cumulatively},
        {"exact_boundary_then_one_rejected", test_exact_boundary_then_one_rejected},
        {"size_t_overflow_rejected", test_size_t_overflow_rejected}};
    int failures{};
    for (const auto& test : tests) {
        try { test.run(); std::cout << "PASS " << test.name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cout << "FAIL " << test.name << ": " << error.what() << '\n'; }
    }
    std::cout << "tests=" << (sizeof(tests) / sizeof(tests[0])) << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
