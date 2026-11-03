#ifndef BCE_TEXT_OCCUPANCY_H
#define BCE_TEXT_OCCUPANCY_H

#include "ElevationModel.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace bce {

// Row-major 2x3 affine: (a*x+b*y+tx, c*x+d*y+ty).
struct TextAffine2 {
    double a{1}; double b{0}; double tx{0};
    double c{0}; double d{1}; double ty{0};
};
enum class TextRangeCoordinates { unknown, beforeDisplayBackward, afterDisplayBackward };
struct TextRangeInput {
    Bounds2 localRange;
    TextRangeCoordinates coordinates{TextRangeCoordinates::unknown};
    bool backward{false};
    Point2 position;
    // Explicit 2D text plane basis, including normal handedness. Caller obtains
    // this from normal/direction. Do not apply height/xScale/tracking/oblique again.
    Point2 localX{1, 0};
    Point2 localY{0, 1};
    TextAffine2 modelToWorld; // The single NET model transform, applied once.
};
struct TextQuadrilateral {
    std::array<Point2, 4> corners;
    Bounds2 bounds;
};
Result<TextQuadrilateral> transformTextRange(const TextRangeInput& input);

struct TextRenderContext {
    std::string deviceIdentity;
    std::string frameIdentity;
    std::string fontIdentity;
    std::string sourceIdentity;
    double dpiX{0}; double dpiY{0};
    // Pixel boundary origin relative to the named device frame, in pixels.
    // worldToPixel already includes this phase; it is never added a second time.
    Point2 pixelBoundaryPhase;
};
struct TextRgbBuffer {
    const std::uint8_t* bytes{nullptr}; // Borrowed RGB, opaque white background.
    std::size_t byteLength{0};
    std::size_t width{0}; std::size_t height{0};
    std::size_t rowStride{0}; // Positive, top-to-bottom; padding is not image data.
};
struct ObservedTextFootprint {
    std::size_t inkPixels{0};
    // Complete cells [minX,maxX+1] x [minY,maxY+1], never pixel centers.
    Bounds2 deviceCellBounds;
    TextQuadrilateral world;
    TextAffine2 worldToPixel;
    TextRenderContext context;
};
// Any RGB channel <255 is ink (including AA). Pixel (x,y) occupies
// [x,x+1] x [y,y+1], top-left origin, y DOWN. Scan the complete image.
// Blank or edge-touching ink is rejected. Successful observation does not
// establish external scene completeness, glyph completeness or Layout eligibility.
Result<ObservedTextFootprint> measureTextFootprint(const TextRgbBuffer& buffer,
    const TextAffine2& worldToPixel, const TextRenderContext& context);

}
#endif
