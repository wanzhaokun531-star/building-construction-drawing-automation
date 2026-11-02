#include "TestSupport.h"
#include "../src/Recognition.h"
#include <Windows.h>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>

namespace bce::test {
namespace {
namespace fs = std::filesystem;
fs::path outputDirectory() {
    std::array<wchar_t, 32768> path{};
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    require(size > 0 && size < path.size(), "test executable path unavailable");
    return fs::path(path.data()).parent_path();
}
fs::path fixture(const char* name) { return outputDirectory().parent_path() / "input" / "tests" / "fixtures" / name; }
std::vector<std::string> fields(const std::string& row) {
    std::vector<std::string> values;
    std::size_t start = 0;
    for (;;) {
        const auto tab = row.find('\t', start);
        values.push_back(row.substr(start, tab == std::string::npos ? tab : tab - start));
        if (tab == std::string::npos) { break; }
        start = tab + 1;
    }
    if (!values.empty() && !values.back().empty() && values.back().back() == '\r') { values.back().pop_back(); }
    return values;
}
SceneSnapshot emptyScene() {
    SceneSnapshot scene;
    scene.documentIdentity = "scheme-document"; scene.databaseIdentity = "scheme-database"; scene.revision = 1;
    scene.units = {0, false, 1.0, ""};
    return scene;
}
EntityKey key(const std::string& handle) { return {"scheme-database", handle, 1}; }
SceneSnapshot realScene() {
    auto scene = emptyScene();
    scene.sourceSha256 = "595CE82FBD20C81E83A857AB569385BEFA98ADAF743300A42668E280561559C1";
    std::ifstream input(fixture("scheme-scene.tsv"));
    require(input.good(), "frozen fixture missing");
    std::string row;
    std::getline(input, row);
    while (std::getline(input, row)) {
        if (row.empty()) { continue; }
        const auto f = fields(row);
        require(f.size() == 18, "invalid fixture columns");
        Primitive2 p; p.key = key(f[0]); p.layer = f[2];
        p.kind = f[1] == "AcDbLine" ? PrimitiveKind::line : f[1] == "AcDbText" ? PrimitiveKind::text :
            f[1] == "AcDbDimension" ? PrimitiveKind::dimension : PrimitiveKind::polyline;
        if (!f[3].empty() && !f[4].empty() && !f[5].empty() && !f[6].empty()) {
            p.bounds = Bounds2{{std::stod(f[3]), std::stod(f[4])}, {std::stod(f[5]), std::stod(f[6])}};
        }
        if (!f[7].empty() && !f[8].empty()) { p.start = Point2{std::stod(f[7]), std::stod(f[8])}; }
        if (!f[9].empty() && !f[10].empty()) { p.end = Point2{std::stod(f[9]), std::stod(f[10])}; }
        if (!f[11].empty()) { p.text = f[11]; }
        if (!f[12].empty() && !f[13].empty()) { p.textPosition = Point2{std::stod(f[12]), std::stod(f[13])}; }
        if (!f[14].empty()) { p.textHeight = std::stod(f[14]); }
        if (!f[15].empty()) { p.textRotation = std::stod(f[15]); }
        if (!f[16].empty()) { p.dimensionMeasurement = std::stod(f[16]); }
        p.topologyComplete = f[17] == "1";
        scene.primitives.push_back(p);
    }
    require(scene.primitives.size() == 741, "real fixture record count");
    return scene;
}
std::map<std::string, std::pair<std::string, ViewKind>> expected() {
    std::ifstream input(fixture("scheme-expected.tsv"));
    require(input.good(), "frozen expected fixture missing");
    std::map<std::string, std::pair<std::string, ViewKind>> values;
    std::string row; std::getline(input, row);
    while (std::getline(input, row)) {
        if (row.empty()) { continue; }
        const auto f = fields(row);
        require(f.size() == 4, "invalid expected columns");
        values.emplace(f[0], std::make_pair(f[1], f[2] == "elevation" ? ViewKind::elevation : ViewKind::section));
    }
    require(values.size() == 741, "expected fixture count");
    return values;
}
bool issueCode(const AnalysisResult& result, const char* code) {
    for (const auto& issue : result.issues) { if (issue.code == code) { return true; } }
    return false;
}
void checkMembership(const AnalysisResult& result) {
    const auto truth = expected();
    require(result.views.size() == 6, "expected six views from title/gap evidence");
    std::set<std::string> seen, sections;
    std::map<std::string, std::size_t> counts;
    for (const auto& view : result.views) {
        require(view.kind != ViewKind::ambiguous, "real fixture unexpectedly ambiguous");
        for (const auto& member : view.members) {
            require(seen.insert(member.handle).second, "member duplicated across views");
            const auto at = truth.find(member.handle);
            require(at != truth.end(), "unexpected member");
            require(at->second.first == view.titleKey.handle, "member assigned to wrong title");
            require(at->second.second == view.kind, "view type mismatch");
            if (view.kind == ViewKind::section) { sections.insert(member.handle); }
        }
        counts[view.titleKey.handle] = view.members.size();
    }
    require(seen.size() == 741 && result.unassigned.empty(), "741 entities must be covered");
    require(counts == std::map<std::string, std::size_t>{{"6AC",159},{"737",139},{"7BC",133},{"844",136},{"885",65},{"8F2",109}}, "exact view counts");
    std::set<std::string> preserved;
    for (const auto& member : result.preserved) { preserved.insert(member.handle); }
    require(preserved == sections, "all section members must be preserved");
    require(issueCode(result, diagnostic::planRequired), "no plan must remain explicit");
}
std::string quote(const std::string& text) {
    std::ostringstream out; out << '"';
    for (const auto c : text) {
        if (c == '"' || c == '\\') { out << '\\' << c; }
        else if (static_cast<unsigned char>(c) < 32) { out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<unsigned int>(static_cast<unsigned char>(c)); }
        else { out << c; }
    }
    out << '"'; return out.str();
}
void writeKeys(std::ostream& out, const std::vector<EntityKey>& keys) {
    out << '['; bool first = true;
    for (const auto& k : keys) { if (!first) { out << ','; } first = false; out << quote(k.handle); }
    out << ']';
}
void writeIssues(std::ostream& out, const std::vector<Issue>& issues) {
    out << '['; bool first = true;
    for (const auto& i : issues) {
        if (!first) { out << ','; } first = false;
        out << "{\"code\":" << quote(i.code) << ",\"message\":" << quote(i.message)
            << ",\"entity\":" << (i.source.entity ? quote(i.source.entity->handle) : "null") << '}';
    }
    out << ']';
}
void writeFeature(std::ostream& out, const Feature& f) {
    out << "{\"kind\":" << quote(f.kind) << ",\"role\":" << quote(f.role == FeatureRole::candidate ? "candidate" : "confirmed") << ",\"sources\":";
    writeKeys(out, f.sources);
    out << ",\"declaredMm\":"; if (f.declaredMm) { out << *f.declaredMm; } else { out << "null"; }
    out << ",\"geometryMm\":"; if (f.geometryMm) { out << *f.geometryMm; } else { out << "null"; }
    out << '}';
}
void exportAnalysis(const AnalysisResult& result) {
    std::ofstream out(outputDirectory() / "recognition-analysis.json");
    require(out.good(), "analysis export unavailable");
    out << std::setprecision(17) << "{\"contract\":\"bce.r1.analysis.v1\",\"sameAnalyseInvocation\":true,\"sourceSHA256\":" << quote(result.sourceSha256) << ",\"views\":[";
    bool first = true;
    for (const auto& v : result.views) {
        if (!first) { out << ','; } first = false;
        out << "{\"titleHandle\":" << quote(v.titleKey.handle) << ",\"kind\":" << quote(v.kind == ViewKind::elevation ? "elevation" : v.kind == ViewKind::section ? "section" : "ambiguous") << ",\"members\":";
        writeKeys(out, v.members);
        out << ",\"bounds\":[" << v.bounds.minimum.x << ',' << v.bounds.minimum.y << ',' << v.bounds.maximum.x << ',' << v.bounds.maximum.y << "],\"levelCandidates\":[";
        bool levelFirst = true;
        for (const auto& f : v.levelCandidates) { if (!levelFirst) { out << ','; } levelFirst = false; writeFeature(out, f); }
        out << "],\"unconfirmed\":"; writeIssues(out, v.unconfirmed); out << '}';
    }
    out << "],\"features\":["; first = true;
    for (const auto& f : result.model.features) { if (!first) { out << ','; } first = false; writeFeature(out, f); }
    out << "],\"preserved\":"; writeKeys(out, result.preserved);
    out << ",\"unassigned\":"; writeKeys(out, result.unassigned);
    out << ",\"issues\":"; writeIssues(out, result.issues); out << "}\n";
}
Primitive2 line(const char* handle, Point2 a, Point2 b) {
    Primitive2 p; p.key = key(handle); p.kind = PrimitiveKind::line; p.start = a; p.end = b;
    p.bounds = Bounds2{{(std::min)(a.x,b.x),(std::min)(a.y,b.y)},{(std::max)(a.x,b.x),(std::max)(a.y,b.y)}};
    p.topologyComplete = true; return p;
}
Primitive2 title(const char* handle, Point2 p, const char* text = "正立面") {
    Primitive2 t; t.key = key(handle); t.kind = PrimitiveKind::text; t.text = text;
    t.textPosition = p; t.textHeight = 1; t.textRotation = 0;
    t.bounds = Bounds2{p, {p.x + 3, p.y + 1}}; t.topologyComplete = true; return t;
}
void group(SceneSnapshot& s, const char* label, const char* a, const char* b, double x, double y) {
    s.primitives.push_back(title(label,{x,y}));
    s.primitives.push_back(line(a,{x,y+3},{x+10,y+3}));
    s.primitives.push_back(line(b,{x+7,y+8},{x+9,y+8})); // separate component is intentional
}
void checkNear(double got, double want, const char* message) { require(std::abs(got-want) < 1e-7, message); }
}
// Catches fixed-count, nearest-title and largest-component classification.
void fourElevationsExcludeTwoSections() {
    const auto result = analyse(realScene(), {}, {});
    checkMembership(result);
    exportAnalysis(result); // This exact result, never a second classification algorithm.
}
// Catches global-X-only grouping, untranslated bounds or ignored text direction.
void sceneTranslationAndQuarterTurnKeepMembership() {
    for (const bool turn : {false,true}) {
        auto s = realScene();
        const auto move = [turn](Point2 p) { return turn ? Point2{90000-p.y,-50000+p.x} : Point2{p.x+12345,p.y-9876}; };
        for (auto& p : s.primitives) {
            const auto b = *p.bounds;
            const auto a = move(b.minimum), c = move(b.maximum);
            p.bounds = Bounds2{{(std::min)(a.x,c.x),(std::min)(a.y,c.y)},{(std::max)(a.x,c.x),(std::max)(a.y,c.y)}};
            if (p.start) { p.start = move(*p.start); } if (p.end) { p.end = move(*p.end); }
            if (p.textPosition) { p.textPosition = move(*p.textPosition); }
            if (turn && p.textRotation) { *p.textRotation += 1.57079632679489661923; }
        }
        const auto r = analyse(s, {}, {}); checkMembership(r);
        const auto at = std::find_if(r.views.begin(),r.views.end(),[](const View& v){ return v.titleKey.handle == "6AC"; });
        require(at != r.views.end(), "transformed front view absent");
        checkNear(at->bounds.minimum.x, turn ? 90000-33227.065940573 : 20300.44442866703+12345,"world minX");
        checkNear(at->bounds.maximum.y, turn ? -50000+41620.02804049577 : 33227.065940573-9876,"world maxY");
    }
}
void verticalOrGridViewsUseTwoDimensionalGaps() {
    auto s = emptyScene();
    group(s,"A1","A2","A3",0,0); group(s,"B1","B2","B3",0,30);
    auto r = analyse(s,{},{}); require(r.views.size() == 2,"overlapping X needs Y split");
    group(s,"C1","C2","C3",30,0); group(s,"D1","D2","D3",30,30);
    r = analyse(s,{},{}); require(r.views.size() == 4,"grid must split along both axes");
    for (const auto& v : r.views) { require(v.members.size()==3 && v.kind==ViewKind::elevation,"grid member leakage"); }
}
void ambiguousRectangleNeedsRole() {
    auto s=emptyScene(); s.primitives={line("1",{0,0},{10,0}),line("2",{10,0},{10,20}),line("3",{10,20},{0,20}),line("4",{0,20},{0,0})};
    const auto r=analyse(s,{},{});
    require(r.model.features.size()==1,"rectangle candidate missing");
    const auto& f=r.model.features.front();
    require(f.kind=="rectangle" && f.role==FeatureRole::candidate && f.sources.size()==4,"rectangle must not become window");
    require(r.unassigned.size()==4 && issueCode(r,diagnostic::ambiguousFeature),"unlabelled rectangle needs confirmation");
    std::set<std::string> located;
    for(const auto& i:r.issues) { if(i.source.entity) { located.insert(i.source.entity->handle); } }
    require(located==std::set<std::string>{"1","2","3","4"},"all four source lines must be locatable");
}
void mirroredNestedBlockKeepsWorldGeometry() {
    const Affine2 inner{0,-1,1,0,10,20,true};
    const Affine2 outer{-2,0,0,2,100,-10,true};
    const auto combined=compose(outer,inner); require(combined.canCommit(),"valid affine composition rejected");
    const auto r=transformed(line("1",{3,4},{5,6}),*combined.value);
    require(r.canCommit(),"valid world line rejected");
    // Hand result: inner (6,23)/(4,25), outer (88,36)/(92,40).
    checkNear(r.value->start->x,88,"nested mirror startX"); checkNear(r.value->start->y,36,"nested mirror startY");
    checkNear(r.value->end->x,92,"nested mirror endX"); checkNear(r.value->end->y,40,"nested mirror endY");
    Primitive2 arc; arc.key=key("2"); arc.kind=PrimitiveKind::arc;
    arc.arc=Arc2{{3,4},2,0,1.5707963267948966}; arc.bounds=Bounds2{{3,4},{5,6}};
    const auto mirrored=transformed(arc,*combined.value);
    require(mirrored.canCommit() && mirrored.value->arc->sweepAngle<0,"reflection must reverse arc orientation");
    checkNear(mirrored.value->arc->center.x,88,"arc world centerX"); checkNear(mirrored.value->arc->radius,4,"arc world radius");
    require(!compose({1,0,0,0,0,0,true},inner).canCommit(),"singular transform accepted");
    require(!compose({1,0,0,1,0,0,false},inner).canCommit(),"3D transform silently projected");
    require(!compose({1,0,0,1,std::numeric_limits<double>::infinity(),0,true},inner).canCommit(),"nonfinite transform accepted");
    require(!transformed(arc,{2,0,0,1,0,0,true}).canCommit(),"noncircular affine arc silently retained as circle");
    auto hugeText=title("3",{0,0}); hugeText.textHeight=(std::numeric_limits<double>::max)();
    require(!transformed(hugeText,{2,0,0,2,0,0,true}).canCommit(),"overflowed text height must not escape as a finite transform result");
}
void multipleBuildingsRemainSeparate() {
    auto s=emptyScene(); group(s,"A1","A2","A3",0,0); group(s,"B1","B2","B3",50,0);
    const auto r=analyse(s,{},{}); require(r.views.size()==2,"both labelled buildings required");
    std::map<std::string,std::set<std::string>> members;
    for(const auto& v:r.views) { for(const auto& k:v.members) { members[v.titleKey.handle].insert(k.handle); } }
    require(members==std::map<std::string,std::set<std::string>>{{"A1",{"A1","A2","A3"}},{"B1",{"B1","B2","B3"}}},"disconnected members lost or borrowed");
}
void axisProjectionReversesOrder() {
    PlanSnapshot p; p.databaseIdentity="matching-plan"; p.confirmedMmPerInputUnit=1;
    p.axes={{key("1"),"1",{0,0}},{key("2"),"2",{3000,0}},{key("3"),"3",{7000,0}},{key("4"),"4",{10000,0}}};
    const auto r=projectAxes(p,{-1,0}); require(r.canCommit(),"real plan projection failed");
    require(r.value->labels==std::vector<std::string>{"4","3","2","1"},"axis order must reverse");
    require(r.value->spacingsMm==std::vector<double>{3000,4000,3000},"real axis spacing mismatch");
    require(hasCode(projectAxes({}, {1,0}),diagnostic::planRequired),"empty plan must not invent axes");
    auto s=emptyScene(); group(s,"A1","A2","A3",0,0);
    const auto analysis=analyse(s,{},{}); require(issueCode(analysis,diagnostic::planRequired),"analyse no-plan limitation absent");
    require(!analysis.views.front().projectedAxes,"axes fabricated without plan");
    ProjectConfig c; c.databaseIdentity=s.databaseIdentity;
    const auto& view=analysis.views.front();
    c.views.push_back({view.identity,view.geometrySignature,{}, {"matching-plan direction","user axis confirmation",true},Point2{-1,0}});
    const auto withPlan=analyse(s,p,c);
    require(withPlan.views.front().projectedAxes.has_value(),"analyse did not use confirmed real-plan mapping");
    require(withPlan.views.front().projectedAxes->labels==std::vector<std::string>{"4","3","2","1"},"analyse projected order differs from real plan");
    require(withPlan.views.front().projectedAxes->spacingsMm==std::vector<double>{3000,4000,3000},"analyse real-plan spacings differ");
}
void levelConflictRequiresConfirmation() {
    auto s=emptyScene(); group(s,"A1","A2","A3",0,0);
    s.primitives.push_back(title("A4",{5,5},"二层平面图 +3900 mm"));
    s.levelAssociations.push_back({key("A2"),key("A3"),key("A4"),{0,0},{0,3000},{0,1},1});
    const auto r=analyse(s,{},{});
    require(issueCode(r,diagnostic::levelConflict),"geometry 3000/text 3900 must conflict");
    const auto& levels=r.views.front().levelCandidates;
    require(levels.size()==1 && levels.front().declaredMm==3900 && levels.front().geometryMm==3000,"both level values must survive");
    require(levels.front().role==FeatureRole::candidate && levels.front().sources.size()==3,"conflict must retain provenance and candidate role");
    bool located=false;
    for(const auto& i:r.issues) { if(i.code==diagnostic::levelConflict && i.source.entity && i.source.entity->handle=="A4" && i.message.find("3000")!=std::string::npos && i.message.find("3900")!=std::string::npos) { located=true; } }
    require(located,"conflict diagnosis must locate text and report both values");
    s.primitives.back().text="二层平面图 +"+std::string(400,'9')+" mm";
    const auto extreme=analyse(s,{},{});
    require(extreme.views.size()==1,"unreadable level number must not abort view analysis");
    const auto& pending=extreme.views.front().levelCandidates;
    require(pending.size()==1 && !pending.front().declaredMm && pending.front().role==FeatureRole::candidate,"unreadable declared height must remain a located candidate without fake zero");
}
void changedGeometryInvalidatesOnlyAffectedConfig() {
    auto s=emptyScene(); group(s,"A1","A2","A3",0,0); group(s,"B1","B2","B3",50,0);
    const auto before=analyse(s,{},{}); require(before.views.size()==2,"config fixture partition failed");
    ProjectConfig c; c.databaseIdentity=s.databaseIdentity; c.units=s.units; c.unitConfirmationSource="user: one unit=one mm";
    c.zeroLevel={"first-floor +0 mm","user, geometry coordinate pending",true};
    for(const auto& v:before.views) { c.views.push_back({v.identity,v.geometrySignature,{"brick","user material",true},{"plan mapping","user mapping",true},Point2{1,0}}); }
    s.primitives[1].end->x=11; s.primitives[1].bounds->maximum.x=11;
    const auto validated=validateConfig(s,c); require(validated.value.has_value(),"changed view should invalidate locally");
    const auto& result=*validated.value;
    for(const auto& v:result.views) {
        const auto original=std::find_if(before.views.begin(),before.views.end(),[&](const View& a){return a.identity==v.viewIdentity;});
        const bool changed=original->titleKey.handle=="A1";
        require(v.material.confirmed==!changed && v.axisMapping.confirmed==!changed,"only changed view confirmations should be cleared");
        require(!v.material.source.empty() && !v.axisMapping.source.empty(),"confirmation sources must be retained");
    }
    require(result.units.confirmedMmPerInputUnit==1 && result.unitConfirmationSource==c.unitConfirmationSource && result.zeroLevel.confirmed,"independent confirmations lost");
    s.databaseIdentity="different-database";
    require(!validateConfig(s,c).value.has_value(),"old database config must be rejected");
}
void overlappingOrUnlabelledViewsNeedConfirmation() {
    auto s=emptyScene(); s.primitives={title("A1",{0,0}),title("B1",{0,0}),line("A2",{0,3},{10,3})};
    auto r=analyse(s,{},{}); require(issueCode(r,diagnostic::ambiguousFeature),"coincident titles cannot be forced apart");
    for(const auto& v:r.views) { require(v.kind==ViewKind::ambiguous,"overlapping labels cannot claim clear view"); }
    require(r.unassigned.size()==3,"ambiguous content must remain retained/unassigned");
    s.primitives={line("1",{0,0},{10,0})}; r=analyse(s,{},{});
    require(r.views.empty() && r.unassigned.size()==1 && issueCode(r,diagnostic::ambiguousFeature),"unlabelled scene cannot invent views");
    s.primitives={title("A1",{0,0}),title("B1",{5,0}),line("A2",{-2,-2},{12,12})};
    r=analyse(s,{},{}); require(issueCode(r,diagnostic::ambiguousFeature) && r.unassigned.size()==3,"overlapping occupancy has no empty cut");
    s=emptyScene(); group(s,"A1","A2","A3",0,0);
    s.primitives.push_back(line("FF",{1000,1000},{1001,1001}));
    r=analyse(s,{},{});
    bool extraPending=false;
    for(const auto& k:r.unassigned) { if(k.handle=="FF") { extraPending=true; } }
    for(const auto& f:r.model.features) {
        if(f.role==FeatureRole::candidate && f.kind=="unresolvedGeometry") {
            for(const auto& k:f.sources) { if(k.handle=="FF") { extraPending=true; } }
        }
    }
    require(extraPending,"isolated extra content must remain explicitly pending, never silently accepted or removed");
}
// A finite input extent may have an unrepresentable span. It must not produce an
// infinite cut and recurse with the original complete member set.
void overflowingGapDoesNotRecurse() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    auto s=emptyScene(); group(s,"A1","A2","A3",-1e308,0); group(s,"B1","B2","B3",1e308,0);
    const auto r=analyse(s,{},{});
    require(r.views.empty() && r.unassigned.size()==6 && r.preserved.size()==6,"unrepresentable span must preserve all as ambiguous");
    require(issueCode(r,diagnostic::ambiguousFeature),"unrepresentable spatial split needs a located ambiguity");
}
void hugeLevelDirectionKeepsFiniteCandidate() {
    auto s=emptyScene(); group(s,"A1","A2","A3",0,0);
    s.primitives.push_back(title("A4",{5,5},"二层平面图 +3900 mm"));
    const double largest=(std::numeric_limits<double>::max)();
    s.levelAssociations.push_back({key("A2"),key("A3"),key("A4"),{0,0},{0,3000},{largest,largest},1});
    auto r=analyse(s,{},{});
    require(r.views.size()==1 && r.views.front().levelCandidates.size()==1,"large-direction level candidate missing");
    const auto& level=r.views.front().levelCandidates.front();
    require(level.geometryMm && std::isfinite(*level.geometryMm),"finite huge direction must not inject NaN geometry");
    checkNear(*level.geometryMm,2121.3203435596426,"stable unit direction must preserve actual geometric projection");
    require(level.declaredMm==3900 && issueCode(r,diagnostic::levelConflict),"large-direction conflict cannot be silently suppressed");
    s.levelAssociations.front().lowerPoint={-1e308,0};
    s.levelAssociations.front().upperPoint={1e308,0};
    s.levelAssociations.front().heightDirection={1,0};
    r=analyse(s,{},{});
    const auto& pending=r.views.front().levelCandidates.front();
    require(!pending.geometryMm && pending.declaredMm==3900 && pending.sources.size()==3,"unrepresentable geometric difference must remain unread, with all sources");
    bool located=false;
    for(const auto& i:r.issues) { if(i.code==diagnostic::ambiguousFeature && i.source.entity && i.source.entity->handle=="A4") { located=true; } }
    require(located,"nonfinite derived level needs located diagnostic");
}
void overflowingAxisSpacingIsRejected() {
    PlanSnapshot p; p.databaseIdentity="matching-plan"; p.confirmedMmPerInputUnit=1;
    p.axes={{key("1"),"1",{-1e308,0}},{key("2"),"2",{1e308,0}}};
    const auto result=projectAxes(p,{1,0});
    require(!result.canCommit() && !result.value && hasCode(result,diagnostic::ambiguousFeature),"infinite spacing must not be accepted from finite stations");
}
void nativeDetailedGeometryInvalidatesOnlyAffectedView() {
    auto scene=emptyScene();group(scene,"A1","A2","A3",0,0);group(scene,"B1","B2","B3",1000,0);
    const auto before=analyse(scene,{},{});require(before.views.size()==2,"native signature fixture");
    scene.primitives[1].nativeGeometrySignature="poly:0,0;4,3;10,0;bulge=0.1;width=2";
    scene.primitives[1].sourceRootHandle="A2";scene.primitives[1].instancePath="A2";
    const auto changed=analyse(scene,{},{});require(changed.views.size()==2,"native signature grouping");
    require(before.views[0].geometrySignature!=changed.views[0].geometrySignature,"complete interior geometry signature ignored");
    require(before.views[1].geometrySignature==changed.views[1].geometrySignature,"unaffected view signature changed");
    scene.primitives[1].nativeGeometrySignature.clear();scene.primitives[1].sourceRootHandle.clear();scene.primitives[1].instancePath.clear();
    const auto defaults=analyse(scene,{},{});require(defaults.views[0].geometrySignature==before.views[0].geometrySignature,"empty defaults changed historical signature");
    scene.primitives[1].effectiveVisible=false;require(analyse(scene,{},{}).views[0].geometrySignature!=before.views[0].geometrySignature,"visibility change ignored");
}
void hiddenTitleDoesNotAnchorView() {
    auto scene=emptyScene();group(scene,"A1","A2","A3",0,0);scene.primitives[0].effectiveVisible=false;
    const auto result=analyse(scene,{},{});
    require(result.views.empty(),"hidden title must not anchor a visible elevation");
    require(result.preserved.size()==3&&result.unassigned.size()==3,"hidden title and remaining roots must remain locatable");
}
void hiddenEdgeDoesNotCloseRectangle() {
    auto scene=emptyScene();scene.primitives={line("1",{0,0},{10,0}),line("2",{10,0},{10,20}),line("3",{10,20},{0,20}),line("4",{0,20},{0,0})};
    scene.primitives[0].effectiveVisible=false;
    const auto result=analyse(scene,{},{});
    require(std::none_of(result.model.features.begin(),result.model.features.end(),[](const Feature& f){return f.kind=="rectangle";}),"hidden edge must not close a visible rectangle");
    require(result.preserved.size()==4,"hidden rectangle edge lost");
}
void hiddenLevelDoesNotAssociate() {
    auto scene=emptyScene();group(scene,"A1","A2","A3",0,0);scene.primitives.push_back(title("A4",{5,5},"二层 +3900 mm"));scene.primitives.back().effectiveVisible=false;
    scene.levelAssociations.push_back({key("A2"),key("A3"),key("A4"),{0,0},{0,3000},{0,1},1});
    const auto result=analyse(scene,{},{});
    require(result.views.size()==1&&result.views[0].members.size()==4,"hidden level source must keep membership evidence");
    require(result.views[0].levelCandidates.empty()&&!issueCode(result,diagnostic::levelConflict),"hidden level must not form an association or conflict");
}
void mirroredTextPreservesBothBasisDirections() {
    auto text=title("A1",{3,4},"正立面"); text.textHeight=2;
    const Affine2 mirrorX{-1,0,0,1,10,0,true};
    const auto result=transformed(text,mirrorX);
    require(result.canCommit() && result.value->textRotation,"similarity mirror of text must be representable");
    const auto basis=[](const Primitive2& p) {
        const double x=p.textMirroredX ? -1.0 : 1.0, y=p.textMirroredY ? -1.0 : 1.0;
        const double c=std::cos(*p.textRotation), s=std::sin(*p.textRotation);
        return std::array<double,4>{{x*c,x*s,-y*s,y*c}};
    };
    // Hand calculation: reflection x -> -x maps original glyph bases (1,0)/(0,1)
    // to (-1,0)/(0,1). Rotation pi alone would incorrectly produce (0,-1) for Y.
    auto b=basis(*result.value);
    checkNear(b[0],-1,"mirrored glyph X.x"); checkNear(b[1],0,"mirrored glyph X.y");
    checkNear(b[2],0,"mirrored glyph Y.x"); checkNear(b[3],1,"mirrored glyph Y.y");
    checkNear(result.value->textPosition->x,7,"mirrored insertion x");
    checkNear(*result.value->textHeight,2,"mirrored positive text height");
    const auto restored=transformed(*result.value,mirrorX); require(restored.canCommit(),"second mirror rejected");
    b=basis(*restored.value);
    checkNear(b[0],1,"two reflections restore glyph X"); checkNear(b[3],1,"two reflections restore glyph Y");
    const Affine2 quarterTurn{0,-1,1,0,0,0,true};
    const auto rotated=transformed(*result.value,quarterTurn); require(rotated.canCommit(),"nested mirrored text rotation rejected");
    b=basis(*rotated.value);
    checkNear(b[0],0,"nested reflected X.x"); checkNear(b[1],-1,"nested reflected X.y");
    checkNear(b[2],-1,"nested reflected Y.x"); checkNear(b[3],0,"nested reflected Y.y");
    text.textMirroredX=true;
    const auto preMirrored=transformed(text,mirrorX); require(preMirrored.canCommit(),"pre-mirrored text rejected");
    b=basis(*preMirrored.value);
    checkNear(b[0],1,"existing X reflection must accumulate"); checkNear(b[3],1,"existing mirrored glyph Y preserved");
    require(hasCode(transformed(text,{2,0,0,1,0,0,true}),diagnostic::invalidTransform),"non-similarity text transform must be explicit rejection");
    auto scene=emptyScene(); group(scene,"B1","B2","B3",0,0);
    const auto before=analyse(scene,{},{}); require(before.views.size()==1,"signature mirror fixture missing");
    scene.primitives.front().textMirroredY=true;
    const auto after=analyse(scene,{},{});
    require(after.views.size()==1 && before.views.front().geometrySignature!=after.views.front().geometrySignature,"glyph handedness must participate in geometry signature");
    scene.primitives.front().textMirroredY=false; scene.primitives.front().textMirroredX=true;
    const auto changedX=analyse(scene,{},{});
    require(changedX.views.size()==1 && before.views.front().geometrySignature!=changedX.views.front().geometrySignature,"glyph X handedness must also participate in geometry signature");
    text.textRotation.reset(); text.textHeight.reset();
    require(hasCode(transformed(text,mirrorX),diagnostic::invalidTransform),"reflection cannot invent missing text orientation");
    require(hasCode(transformed(text,{2,0,0,1,0,0,true}),diagnostic::invalidTransform),"missing metadata cannot bypass non-similarity text rejection");
}
}

