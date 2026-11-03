#include "TextOccupancy.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace bce {
namespace {
bool finite(Point2 p) { return std::isfinite(p.x) && std::isfinite(p.y); }
bool finite(const TextAffine2& m) {
    return std::isfinite(m.a) && std::isfinite(m.b) && std::isfinite(m.tx) &&
        std::isfinite(m.c) && std::isfinite(m.d) && std::isfinite(m.ty);
}
Point2 apply(const TextAffine2& m, Point2 p) {
    return {m.a*p.x+m.b*p.y+m.tx, m.c*p.x+m.d*p.y+m.ty};
}
bool invert(const TextAffine2& m, TextAffine2& inverse) {
    if (!finite(m)) return false;
    const double scale=std::max({std::abs(m.a),std::abs(m.b),std::abs(m.c),std::abs(m.d)});
    if (scale==0) return false;
    const double a=m.a/scale,b=m.b/scale,c=m.c/scale,d=m.d/scale;
    const double determinant=a*d-b*c;
    if (!std::isfinite(determinant) || determinant==0) return false;
    inverse={d/determinant/scale,-b/determinant/scale,0,
        -c/determinant/scale,a/determinant/scale,0};
    inverse.tx=-(inverse.a*m.tx+inverse.b*m.ty);
    inverse.ty=-(inverse.c*m.tx+inverse.d*m.ty);
    return finite(inverse);
}
template<class T> Result<T> reject(const char* code,const char* message) {
    return {std::nullopt,{{code,message,{},true}}};
}
bool boundsValid(const Bounds2& b) {
    return finite(b.minimum) && finite(b.maximum) &&
        b.minimum.x<b.maximum.x && b.minimum.y<b.maximum.y;
}
std::array<Point2,4> corners(const Bounds2& b) {
    return {{b.minimum,{b.maximum.x,b.minimum.y},b.maximum,{b.minimum.x,b.maximum.y}}};
}
bool finish(TextQuadrilateral& q) {
    q.bounds={q.corners[0],q.corners[0]};
    for(const auto p:q.corners) {
        if(!finite(p)) return false;
        q.bounds.minimum.x=std::min(q.bounds.minimum.x,p.x);
        q.bounds.minimum.y=std::min(q.bounds.minimum.y,p.y);
        q.bounds.maximum.x=std::max(q.bounds.maximum.x,p.x);
        q.bounds.maximum.y=std::max(q.bounds.maximum.y,p.y);
    }
    if(!boundsValid(q.bounds)) return false;
    // Validate the actual rounded corners, not just their AABB. Translate to
    // adjacent edge vectors before cross products so a large world origin does
    // not cancel the area. Normalize each edge to avoid product overflow; no
    // geometric tolerance or margin turns a collapsed polygon into a success.
    double firstTurn=0;
    for(std::size_t k=0;k<q.corners.size();++k) {
        const auto p=q.corners[k];
        const auto next=q.corners[(k+1)%q.corners.size()];
        const auto after=q.corners[(k+2)%q.corners.size()];
        const Point2 edge={next.x-p.x,next.y-p.y};
        const Point2 following={after.x-next.x,after.y-next.y};
        if(!finite(edge) || !finite(following)) return false;
        const double scale=std::max(std::abs(edge.x),std::abs(edge.y));
        const double followingScale=std::max(std::abs(following.x),std::abs(following.y));
        if(scale==0 || followingScale==0) return false;
        const double turn=(edge.x/scale)*(following.y/followingScale)-
            (edge.y/scale)*(following.x/followingScale);
        if(!std::isfinite(turn) || turn==0) return false;
        if(k==0) firstTurn=turn;
        else if((turn>0)!=(firstTurn>0)) return false;
    }
    return true;
}
}
Result<TextQuadrilateral> transformTextRange(const TextRangeInput& i) {
    if(i.coordinates!=TextRangeCoordinates::beforeDisplayBackward &&
       i.coordinates!=TextRangeCoordinates::afterDisplayBackward)
        return reject<TextQuadrilateral>("TEXT_RANGE_CONVENTION","Explicit range coordinate convention required.");
    if(!boundsValid(i.localRange) || !finite(i.position) || !finite(i.localX) || !finite(i.localY))
        return reject<TextQuadrilateral>("TEXT_RANGE_INPUT","Finite nondegenerate local range and basis required.");
    TextAffine2 unused;
    if(!invert({i.localX.x,i.localY.x,0,i.localX.y,i.localY.y,0},unused) ||
       !invert(i.modelToWorld,unused))
        return reject<TextQuadrilateral>("TEXT_RANGE_TRANSFORM","Finite invertible basis and net affine required.");
    TextQuadrilateral q;
    const auto local=corners(i.localRange);
    for(std::size_t k=0;k<local.size();++k) {
        const double x=(i.backward && i.coordinates==TextRangeCoordinates::beforeDisplayBackward)?-local[k].x:local[k].x;
        const Point2 model={i.position.x+i.localX.x*x+i.localY.x*local[k].y,
            i.position.y+i.localX.y*x+i.localY.y*local[k].y};
        q.corners[k]=apply(i.modelToWorld,model);
    }
    if(!finish(q)) return reject<TextQuadrilateral>("TEXT_RANGE_NUMERIC","Transformed corners are not finite and nondegenerate.");
    return {q,{}};
}

Result<ObservedTextFootprint> measureTextFootprint(const TextRgbBuffer& b,
    const TextAffine2& mapping,const TextRenderContext& context) {
    const auto limit=std::numeric_limits<std::size_t>::max();
    constexpr double exactIntegerLimit=9007199254740992.0;
    if(!b.bytes || b.width==0 || b.height==0 || b.width>limit/3 ||
       static_cast<double>(b.width)>=exactIntegerLimit || static_cast<double>(b.height)>=exactIntegerLimit)
        return reject<ObservedTextFootprint>("TEXT_RGB_LAYOUT","Invalid RGB dimensions or pointer.");
    const auto rowBytes=b.width*3;
    if(b.rowStride<rowBytes || (b.height-1)>(limit-rowBytes)/b.rowStride ||
       b.byteLength<(b.height-1)*b.rowStride+rowBytes)
        return reject<ObservedTextFootprint>("TEXT_RGB_LAYOUT","RGB stride, length or size overflow.");
    TextAffine2 inverse;
    if(!invert(mapping,inverse))
        return reject<ObservedTextFootprint>("TEXT_PIXEL_MAPPING","Finite invertible worldToPixel required.");
    if(context.deviceIdentity.empty() || context.frameIdentity.empty() || context.fontIdentity.empty() ||
       context.sourceIdentity.empty() || !std::isfinite(context.dpiX) || !std::isfinite(context.dpiY) ||
       context.dpiX<=0 || context.dpiY<=0 || !finite(context.pixelBoundaryPhase))
        return reject<ObservedTextFootprint>("TEXT_RENDER_CONTEXT","Device, frame, font, source, DPI and finite phase required.");
    std::size_t count=0,minX=b.width,minY=b.height,maxX=0,maxY=0;
    for(std::size_t y=0;y<b.height;++y) {
        const auto* row=b.bytes+y*b.rowStride;
        for(std::size_t x=0;x<b.width;++x) {
            const auto* pixel=row+x*3;
            if(pixel[0]<255 || pixel[1]<255 || pixel[2]<255) {
                ++count; minX=std::min(minX,x);minY=std::min(minY,y);
                maxX=std::max(maxX,x);maxY=std::max(maxY,y);
            }
        }
    }
    if(count==0) return reject<ObservedTextFootprint>("TEXT_RGB_BLANK","No observed ink.");
    if(minX==0 || minY==0 || maxX==b.width-1 || maxY==b.height-1)
        return reject<ObservedTextFootprint>("TEXT_RGB_EDGE","Ink touches buffer edge; uncropped observation unproven.");
    ObservedTextFootprint result;
    result.inkPixels=count; result.context=context; result.worldToPixel=mapping;
    result.deviceCellBounds={{static_cast<double>(minX),static_cast<double>(minY)},
        {static_cast<double>(maxX+1),static_cast<double>(maxY+1)}};
    const auto device=corners(result.deviceCellBounds);
    for(std::size_t k=0;k<device.size();++k) result.world.corners[k]=apply(inverse,device[k]);
    if(!finish(result.world))
        return reject<ObservedTextFootprint>("TEXT_PIXEL_NUMERIC","Inverse cell corners are not finite and nondegenerate.");
    return {result,{}};
}
}
