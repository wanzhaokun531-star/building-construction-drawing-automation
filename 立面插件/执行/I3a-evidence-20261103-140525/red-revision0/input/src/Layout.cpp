#include "Layout.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace bce {
namespace {
template<class T> Result<T> failure(const std::string& code, const std::string& view = {}) {
    return {std::nullopt, {{code,u8"排版输入、占位证据或搜索结果不满足约定，请按诊断核对。",{"",view,std::nullopt},true}}};
}
bool named(const std::string& s) { return !s.empty() && s.find('\0') == std::string::npos; }
bool finite(double v) { return std::isfinite(v); }
bool positive(double v) { return finite(v) && v > 0; }
bool validBounds(const Bounds2& b) {
    return finite(b.minimum.x) && finite(b.minimum.y) && finite(b.maximum.x) && finite(b.maximum.y) &&
        b.minimum.x <= b.maximum.x && b.minimum.y <= b.maximum.y &&
        finite(b.maximum.x-b.minimum.x) && finite(b.maximum.y-b.minimum.y);
}
bool validKind(OccupancyKind k) {
    return k == OccupancyKind::architecture || k == OccupancyKind::textGlyph || k == OccupancyKind::symbol ||
        k == OccupancyKind::dimensionGlyph || k == OccupancyKind::dimensionBaseline || k == OccupancyKind::materialTable;
}
bool glyphBearing(OccupancyKind k) {
    return k == OccupancyKind::textGlyph || k == OccupancyKind::dimensionGlyph ||
        k == OccupancyKind::symbol || k == OccupancyKind::materialTable;
}
bool validContext(const LayoutContext& c) {
    return positive(c.scaleDenominator) && positive(c.mmPerInputUnit) && named(c.geometrySignature) &&
        named(c.fontSignature) && named(c.glyphSignature);
}
bool sameContext(const LayoutContext& a, const LayoutContext& b) {
    return a.scaleDenominator == b.scaleDenominator && a.mmPerInputUnit == b.mmPerInputUnit &&
        a.geometrySignature == b.geometrySignature && a.fontSignature == b.fontSignature && a.glyphSignature == b.glyphSignature;
}
Bounds2 merged(const std::vector<VisibleOccupancy>& items) {
    auto b = items.front().bounds;
    for (const auto& item : items) {
        b.minimum.x = std::min(b.minimum.x,item.bounds.minimum.x); b.minimum.y = std::min(b.minimum.y,item.bounds.minimum.y);
        b.maximum.x = std::max(b.maximum.x,item.bounds.maximum.x); b.maximum.y = std::max(b.maximum.y,item.bounds.maximum.y);
    }
    return b;
}
std::string validateParts(const std::vector<OccupancyPart>& required, const std::vector<VisibleOccupancy>& items,
    bool complete, const std::string& source, const LayoutContext& context, bool& nativeReady) {
    if (!complete || !named(source) || required.empty() || required.size() != items.size()) { return "LAYOUT_OCCUPANCY_INCOMPLETE"; }
    if (!validContext(context)) { return "INVALID_LAYOUT_GEOMETRY"; }
    std::map<std::string,OccupancyKind> inventory;
    for (const auto& part : required) {
        if (!named(part.identity) || !validKind(part.kind) || !inventory.emplace(part.identity,part.kind).second) { return "LAYOUT_OCCUPANCY_INCOMPLETE"; }
    }
    for (const auto& item : items) {
        const auto found = inventory.find(item.part.identity);
        if (found == inventory.end() || found->second != item.part.kind || !named(item.source) ||
            (item.evidence != OccupancyEvidence::nativeVerified && item.evidence != OccupancyEvidence::syntheticVerified)) { return "LAYOUT_OCCUPANCY_INCOMPLETE"; }
        inventory.erase(found);
        if (!validBounds(item.bounds)) { return "INVALID_LAYOUT_GEOMETRY"; }
        if (glyphBearing(item.part.kind) && (item.fontSignature != context.fontSignature || item.glyphSignature != context.glyphSignature)) { return "LAYOUT_GLYPH_MISMATCH"; }
        if (item.evidence == OccupancyEvidence::syntheticVerified) { nativeReady = false; }
    }
    if (!inventory.empty()) { return "LAYOUT_OCCUPANCY_INCOMPLETE"; }
    return validBounds(merged(items)) ? "" : "INVALID_LAYOUT_GEOMETRY";
}
bool collectKeys(const std::vector<EntityKey>& keys, const LayoutRequest& request, std::vector<EntityKey>& seen) {
    if (keys.empty()) { return false; }
    for (const auto& key : keys) {
        if (!named(key.databaseIdentity) || key.databaseIdentity != request.databaseIdentity || key.revision != request.revision || !(key == key) ||
            std::find(seen.begin(),seen.end(),key) != seen.end()) { return false; }
        seen.push_back(key);
    }
    return true;
}
bool inside(const Bounds2& b, const Bounds2& area) {
    return b.minimum.x >= area.minimum.x && b.minimum.y >= area.minimum.y &&
        b.maximum.x <= area.maximum.x && b.maximum.y <= area.maximum.y;
}
// Layout accepts exactly the requested gap; internal collision checks count a
// touching glyph/baseline as collision, even at zero requested clearance.
bool axisSeparated(double end, double start, double gap, bool touchingCollides) {
    if (end > start) { return false; }
    // Compare actual distance, not end+gap: that addition may round a small
    // positive gap away at large coordinates. Avoid overflow for far-apart
    // endpoints of opposite signs before subtracting them.
    if (end < 0 && start > std::numeric_limits<double>::max()+end) { return true; }
    const double distance = start-end;
    return touchingCollides ? distance > gap : distance >= gap;
}
bool separated(const Bounds2& a, const Bounds2& b, double gap, bool touchingCollides = false) {
    return axisSeparated(a.maximum.x,b.minimum.x,gap,touchingCollides) ||
        axisSeparated(b.maximum.x,a.minimum.x,gap,touchingCollides) ||
        axisSeparated(a.maximum.y,b.minimum.y,gap,touchingCollides) ||
        axisSeparated(b.maximum.y,a.minimum.y,gap,touchingCollides);
}
bool gapFinite(const Bounds2& b, double gap) { return finite(b.maximum.x+gap) && finite(b.maximum.y+gap); }
std::pair<double,double> exactSpan(double minimum, double maximum) {
    // Error-free TwoDiff expansion of finite endpoint differences. Comparing
    // only the rounded high part can hide a nonzero change (1e16-1 == 1e16).
    // validBounds has already excluded nonfinite/overflowing differences.
    const double high = maximum-minimum;
    const double virtualMinimum = maximum-high;
    const double virtualMaximum = high+virtualMinimum;
    const double low = (maximum-virtualMaximum)+(virtualMinimum-minimum);
    return {high,low};
}
bool sameExtents(const Bounds2& before, const Bounds2& after) {
    return exactSpan(before.minimum.x,before.maximum.x) == exactSpan(after.minimum.x,after.maximum.x) &&
        exactSpan(before.minimum.y,before.maximum.y) == exactSpan(after.minimum.y,after.maximum.y);
}
void uniqueSorted(std::vector<double>& values) {
    std::sort(values.begin(),values.end()); values.erase(std::unique(values.begin(),values.end()),values.end());
}
struct Search {
    const std::vector<ViewEnvelope>& views;
    const LayoutRequest& request;
    const std::vector<Bounds2>& originals;
    std::vector<Bounds2> occupied;
    std::vector<std::optional<PlacedView>> placed;
    std::size_t count{0};
    bool limited{false};
    bool invalid{false};

    bool place(std::size_t depth) {
        if (depth == views.size()) { return true; }
        std::vector<double> xs{request.available.minimum.x}, ys{request.available.minimum.y};
        for (const auto& b : occupied) { xs.push_back(b.maximum.x+request.clearance); ys.push_back(b.maximum.y+request.clearance); }
        uniqueSorted(xs); uniqueSorted(ys);
        for (std::size_t i = 0; i < views.size(); ++i) {
            if (placed[i] || (!request.allowReorder && i != depth)) { continue; }
            const auto& original = originals[i];
            const double width = original.maximum.x-original.minimum.x, height = original.maximum.y-original.minimum.y;
            for (const double y : ys) { for (const double x : xs) {
                if (count == request.searchBudget) { limited = true; return false; }
                ++count;
                if (x < request.available.minimum.x || y < request.available.minimum.y || x > request.available.maximum.x || y > request.available.maximum.y ||
                    width > request.available.maximum.x-x || height > request.available.maximum.y-y) { continue; }
                const Bounds2 destination{{x,y},{x+width,y+height}};
                if (!validBounds(destination) || !gapFinite(destination,request.clearance)) { invalid = true; return false; }
                if (!inside(destination,request.available) || std::any_of(occupied.begin(),occupied.end(),[&](const Bounds2& b) {
                    return !separated(destination,b,request.clearance);
                })) { continue; }
                PlacedView p; p.viewIdentity = views[i].viewIdentity;
                p.displacement = {x-original.minimum.x,y-original.minimum.y}; p.plan.viewIdentity = p.viewIdentity;
                p.context = views[i].context; p.source = views[i].source; p.occupancies = views[i].occupancies;
                p.requiredParts = views[i].requiredParts; p.complete = views[i].complete;
                if (!finite(p.displacement.x) || !finite(p.displacement.y)) { invalid = true; return false; }
                for (auto& item : p.occupancies) {
                    const auto old = item.bounds;
                    item.bounds = {{old.minimum.x+p.displacement.x,old.minimum.y+p.displacement.y},
                        {old.maximum.x+p.displacement.x,old.maximum.y+p.displacement.y}};
                    if (!validBounds(item.bounds) || !inside(item.bounds,request.available) ||
                        !sameExtents(old,item.bounds)) { invalid = true; return false; }
                }
                p.occupiedBounds = merged(p.occupancies);
                // Re-read the actual translated parts; never rely on a rounded,
                // ideal candidate box when checking final clearance.
                if (std::any_of(occupied.begin(),occupied.end(),[&](const Bounds2& b) {return !separated(p.occupiedBounds,b,request.clearance);})) { continue; }
                for (const auto& key : views[i].targets) { p.plan.translations.push_back({key,p.displacement}); }
                placed[i] = std::move(p); occupied.push_back(placed[i]->occupiedBounds);
                if (place(depth+1)) { return true; }
                occupied.pop_back(); placed[i].reset();
                if (limited || invalid) { return false; }
            }}
        }
        return false;
    }
};
}

Result<double> paperSizeInInput(double paperMm, double scaleDenominator, double mmPerInputUnit) {
    if (!positive(paperMm) || !positive(scaleDenominator) || !positive(mmPerInputUnit)) { return failure<double>("INVALID_LAYOUT_GEOMETRY"); }
    const double modelMm = paperMm*scaleDenominator;
    const double result = modelMm/mmPerInputUnit;
    if (!positive(modelMm) || !positive(result)) { return failure<double>("INVALID_LAYOUT_GEOMETRY"); }
    return {result,{}};
}

Result<std::vector<OccupancyCollision>> checkInternalCollisions(const ViewEnvelope& view,
    const std::vector<CollisionPair>& pairs, double clearance) {
    if (!finite(clearance) || clearance < 0) { return failure<std::vector<OccupancyCollision>>("INVALID_LAYOUT_GEOMETRY",view.viewIdentity); }
    std::map<std::string,Bounds2> parts;
    for (const auto& part : view.occupancies) {
        if (!named(part.part.identity) || !validBounds(part.bounds) || !gapFinite(part.bounds,clearance) || !parts.emplace(part.part.identity,part.bounds).second) {
            return failure<std::vector<OccupancyCollision>>("INVALID_LAYOUT_GEOMETRY",view.viewIdentity);
        }
    }
    Result<std::vector<OccupancyCollision>> result; result.value.emplace();
    for (const auto& pair : pairs) {
        const auto a = parts.find(pair.first), b = parts.find(pair.second);
        if (a == parts.end() || b == parts.end() || a == b) { return failure<std::vector<OccupancyCollision>>("INVALID_LAYOUT_PAIR",view.viewIdentity); }
        if (!separated(a->second,b->second,clearance,true)) { result.value->push_back({pair.first,pair.second}); }
    }
    if (!result.value->empty()) { result.issues.push_back({"LAYOUT_INTERNAL_COLLISION",u8"指定标注避让对象相交或净距不足。",{"",view.viewIdentity,std::nullopt},true}); }
    return result;
}

Result<LayoutArrangement> arrange(const std::vector<ViewEnvelope>& views, const LayoutRequest& request) {
    if (views.empty() || views.size() > 16 || request.fixed.size() > 256 || request.searchBudget == 0 || request.searchBudget > 1000000) {
        return failure<LayoutArrangement>("LAYOUT_UNSUPPORTED_RANGE");
    }
    if (!named(request.databaseIdentity) || request.expectedViews.size() != views.size()) { return failure<LayoutArrangement>("INVALID_LAYOUT_IDENTITY"); }
    if (!validBounds(request.available) || !positive(request.available.maximum.x-request.available.minimum.x) ||
        !positive(request.available.maximum.y-request.available.minimum.y) || !finite(request.clearance) || request.clearance < 0 || !gapFinite(request.available,request.clearance)) {
        return failure<LayoutArrangement>("INVALID_LAYOUT_GEOMETRY");
    }
    std::map<std::string,LayoutContext> expected;
    for (const auto& item : request.expectedViews) {
        if (!named(item.viewIdentity) || !expected.emplace(item.viewIdentity,item.context).second) { return failure<LayoutArrangement>("INVALID_LAYOUT_IDENTITY"); }
        if (!validContext(item.context)) { return failure<LayoutArrangement>("INVALID_LAYOUT_GEOMETRY",item.viewIdentity); }
    }
    std::size_t parts = 0, keys = 0;
    for (const auto& v : views) { parts += v.occupancies.size(); keys += v.targets.size(); }
    for (const auto& f : request.fixed) { parts += f.occupancies.size(); keys += f.targets.size(); }
    if (parts > 10000 || keys > 10000) { return failure<LayoutArrangement>("LAYOUT_UNSUPPORTED_RANGE"); }
    bool nativeReady = true;
    std::vector<EntityKey> seen;
    std::vector<Bounds2> originals, fixed;
    for (const auto& v : views) {
        const auto found = expected.find(v.viewIdentity);
        if (found == expected.end() || !collectKeys(v.targets,request,seen)) { return failure<LayoutArrangement>("INVALID_LAYOUT_IDENTITY",v.viewIdentity); }
        const auto code = validateParts(v.requiredParts,v.occupancies,v.complete,v.source,v.context,nativeReady);
        if (!code.empty()) { return failure<LayoutArrangement>(code,v.viewIdentity); }
        if (!sameContext(found->second,v.context)) { return failure<LayoutArrangement>("LAYOUT_CONTEXT_MISMATCH",v.viewIdentity); }
        expected.erase(found);
        const auto b = merged(v.occupancies);
        if (!positive(b.maximum.x-b.minimum.x) || !positive(b.maximum.y-b.minimum.y) ||
            !finite(request.available.minimum.x-b.minimum.x) || !finite(request.available.minimum.y-b.minimum.y) ||
            !finite(request.available.maximum.x-b.maximum.x) || !finite(request.available.maximum.y-b.maximum.y)) { return failure<LayoutArrangement>("INVALID_LAYOUT_GEOMETRY",v.viewIdentity); }
        originals.push_back(b);
    }
    std::map<std::string,bool> fixedIds;
    for (const auto& f : request.fixed) {
        if (!named(f.identity) || !fixedIds.emplace(f.identity,true).second || !collectKeys(f.targets,request,seen)) { return failure<LayoutArrangement>("INVALID_LAYOUT_IDENTITY",f.identity); }
        const auto code = validateParts(f.requiredParts,f.occupancies,f.complete,f.source,f.context,nativeReady);
        if (!code.empty()) { return failure<LayoutArrangement>(code,f.identity); }
        const auto b = merged(f.occupancies);
        if (!gapFinite(b,request.clearance)) { return failure<LayoutArrangement>("INVALID_LAYOUT_GEOMETRY",f.identity); }
        fixed.push_back(b);
    }
    // A view wider/taller than the region is a direct impossibility proof.
    for (const auto& b : originals) {
        if (b.maximum.x-b.minimum.x > request.available.maximum.x-request.available.minimum.x ||
            b.maximum.y-b.minimum.y > request.available.maximum.y-request.available.minimum.y) { return failure<LayoutArrangement>(diagnostic::noLayoutSpace); }
    }
    Search search{views,request,originals,fixed,std::vector<std::optional<PlacedView>>(views.size())};
    if (!search.place(0)) {
        if (search.invalid || search.limited) { return failure<LayoutArrangement>(search.invalid ? "INVALID_LAYOUT_GEOMETRY" : "LAYOUT_SEARCH_LIMIT"); }
        return {std::nullopt,{{diagnostic::noLayoutSpace,u8"已完整搜索 positive_edge_anchors_v1 合法布局族仍无解；此结论不代表所有连续平移布局数学无解。",{},true}}};
    }
    LayoutArrangement arrangement; arrangement.nativeReady = nativeReady; arrangement.fixed = request.fixed; arrangement.searchedCandidates = search.count;
    arrangement.available = request.available; arrangement.clearance = request.clearance; arrangement.allowReorder = request.allowReorder;
    for (auto& p : search.placed) { arrangement.views.push_back(std::move(*p)); }
    std::vector<VisibleOccupancy> all;
    for (const auto& p : arrangement.views) { all.insert(all.end(),p.occupancies.begin(),p.occupancies.end()); }
    arrangement.occupiedBounds = merged(all);
    Result<LayoutArrangement> result{std::move(arrangement),{}};
    if (!nativeReady) { result.issues.push_back({"LAYOUT_SYNTHETIC_ONLY",u8"仅合成核准占位夹具，未证明实际天正字形，不可提交原生写入。",{},true}); }
    return result;
}
}
