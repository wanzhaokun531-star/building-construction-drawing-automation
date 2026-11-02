#pragma once

#include "PlanRules.h"
#include <cstddef>
#include <vector>

namespace bcp {

struct ApronInterruption { Point a, b; };
struct ApronPath { std::vector<Point> points; bool closed = false; };
struct ApronSegment { Point a, b; };
struct ApronOutline {
    // Clean, counterclockwise contours; no duplicated closing vertex.
    std::vector<Point> innerContour;
    std::vector<Point> offsetOuterContour;
    // No inner-wall strokes. Open paths include exact cut-box boundary caps,
    // which can turn at concave corners without moving the wall opening ends.
    std::vector<ApronPath> paths;
    std::vector<ApronSegment> convexCornerJoints;
    std::size_t interruptionComponentCount = 0;
};

// Millimetres. The contour is one simple orthogonal building perimeter, not an
// inferred courtyard/room boundary. Either winding is accepted. Every cut must
// be a nonzero segment contained in one actual straight contour edge. A platform
// spanning corners is expressed as touching cuts on the respective edges.
// Touching/overlapping cuts merge cyclically. Numerical epsilon is inherited
// from PlanRules; it is not a construction clearance. Invalid/nonfinite input,
// self-intersection, collapsed offsets, or ambiguous corner-only contacts throw
// std::invalid_argument. Cuts subtract their exact outward-normal width rectangles
// from the strip; near concave corners the resulting caps can therefore be L
// shaped. No topology repair or inferred widening of an opening is attempted.
ApronOutline makeApronOutline(const std::vector<Point>& contour, double width,
    const std::vector<ApronInterruption>& interruptions = {});

} // namespace bcp
