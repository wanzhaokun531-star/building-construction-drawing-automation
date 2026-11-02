#pragma once
#include "Recognition.h"
#include "SceneReader.h"
#include <functional>
#include <map>
namespace bce {
struct NativeGeometryRecord {
    EntityKey root;
    EntityKey leaf;
    std::string instancePath;
    std::string nativeType;
    std::string canonicalGeometry;
    Primitive2 worldGeometry; // Read-only leaf geometry; key is an actual CAD handle.
    bool effectiveVisible{true};
    bool complete{false};
};
struct NativeAnalysis {
    SceneSnapshot scene;
    AnalysisResult recognition;
    std::vector<NativeGeometryRecord> geometry;
    std::map<std::string, std::size_t> rootTypeCounts;
    std::vector<Issue> readIssues;
};
struct AnalysisReadOptions {
    std::optional<double> confirmedMmPerInputUnit;
    std::string sourceSha256;
    PlanSnapshot plan;
    ProjectConfig project;
    // Optional diagnostic observer, called synchronously; no SDK pointer escapes.
    std::function<void(const char *, const EntityKey &)> diagnosticObserver;
};
// Current unsaved memory. Context identity and revision are checked before and after.
Result<NativeAnalysis> readCurrentAnalysis(NativeSceneContext &, const AnalysisReadOptions &);
// Explicit external database read: never claims current-document identity.
Result<NativeAnalysis> readDatabaseAnalysis(AcDbDatabase &, const std::string &databaseIdentity,
                                            std::uint64_t revision, const AnalysisReadOptions &);
} // namespace bce
