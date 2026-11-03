#pragma once
#include "ProjectFacts.h"
#include "SceneReader.h"
namespace bce {
struct ProjectFactSourceRead {
    bool readCompleted{false};
    FactReadProvenance read;
    std::string modelSpaceOwnerHandle;
    std::vector<FactSource> sources;
};
struct ProjectFactsReadRequest {
    NativeSceneContext& context;
    FactReadProvenance expected;
    std::vector<std::string> sourceHandles;
};
// Limited typed observation of explicitly selected current-memory sources only.
// Completed does not mean supported, matched, configured or eligible for generation.
// No project ID or confirmed units are invented here; configuration owns those fields.
// Missing sources are omitted with FACT_SOURCE_MISSING; unsupported sources retain
// their actual class/owner and monostate. An unavailable read never completes a batch.
[[nodiscard]] Result<ProjectFactSourceRead> readCurrentProjectFacts(const ProjectFactsReadRequest&);
}

