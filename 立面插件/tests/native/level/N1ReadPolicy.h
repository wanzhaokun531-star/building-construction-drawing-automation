#ifndef BCE_REFERENCE_CAPTURE_POLICY_H
#define BCE_REFERENCE_CAPTURE_POLICY_H

#include <cstddef>
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace bce_capture {
inline bool hatch_polyline_counts_valid(std::size_t vertex_count, std::size_t bulge_count) noexcept {
    // AcDbHatch::getLoopAt may return no bulges when every segment is straight.
    return bulge_count == 0 || vertex_count == bulge_count;
}

struct Point2 { double x{}, y{}; };
struct CircularArc {
    Point2 center;
    double radius{}, start_angle{}, end_angle{};
    bool clockwise{};
    Point2 reference_vector;
    Point2 start_point, end_point;
};

inline bool circular_arc_endpoints_consistent(const CircularArc& arc) noexcept {
    const auto finite_point = [](Point2 point) { return std::isfinite(point.x) && std::isfinite(point.y); };
    if (!finite_point(arc.center) || !finite_point(arc.reference_vector) || !finite_point(arc.start_point) ||
        !finite_point(arc.end_point) || !std::isfinite(arc.radius) || arc.radius <= 0 ||
        !std::isfinite(arc.start_angle) || !std::isfinite(arc.end_angle)) return false;
    const double reference_length = std::hypot(arc.reference_vector.x, arc.reference_vector.y);
    if (!std::isfinite(reference_length) || std::abs(reference_length - 1.0) > 1e-9) return false;
    const double coordinate_scale = (std::max)({1.0, arc.radius, std::abs(arc.center.x), std::abs(arc.center.y)});
    const double tolerance = 1e-9 * coordinate_scale;
    const auto on_circle = [&arc, tolerance](Point2 point) {
        const double distance = std::hypot(point.x - arc.center.x, point.y - arc.center.y);
        return std::isfinite(distance) && std::abs(distance - arc.radius) <= tolerance;
    };
    return on_circle(arc.start_point) && on_circle(arc.end_point);
}

// The reader and the native-free tests use the same boundary projection.
template<class EncodeNumber>
std::vector<std::pair<std::string, std::string>> circular_arc_fields(
    const CircularArc& arc, const EncodeNumber& encode_number) {
    const auto point = [&encode_number](Point2 value) {
        return '[' + encode_number(value.x) + ',' + encode_number(value.y) + ']';
    };
    return {{"center", point(arc.center)}, {"radius", encode_number(arc.radius)},
        {"startAngle", encode_number(arc.start_angle)}, {"endAngle", encode_number(arc.end_angle)},
        {"clockwise", arc.clockwise ? "true" : "false"},
        {"referenceVector", point(arc.reference_vector)}, {"startPoint", point(arc.start_point)}, {"endPoint", point(arc.end_point)},
        // Explicit absolute plane angles are derived from the authoritative SDK endpoints.
        // This does not assume the SDK's clockwise-relative parameter convention.
        {"planeStartAngle", encode_number(std::atan2(arc.start_point.y - arc.center.y, arc.start_point.x - arc.center.x))},
        {"planeEndAngle", encode_number(std::atan2(arc.end_point.y - arc.center.y, arc.end_point.x - arc.center.x))},
        {"endpointCheck", circular_arc_endpoints_consistent(arc) ? "true" : "false"}};
}

class LeafBudget {
public:
    explicit LeafBudget(std::size_t limit) noexcept : limit_(limit) {}
    bool try_consume(std::size_t amount) noexcept {
        if (amount > limit_ - used_) return false;
        used_ += amount;
        return true;
    }
    std::size_t used() const noexcept { return used_; }
private:
    std::size_t limit_{};
    std::size_t used_{};
};
} // namespace bce_capture
#endif
