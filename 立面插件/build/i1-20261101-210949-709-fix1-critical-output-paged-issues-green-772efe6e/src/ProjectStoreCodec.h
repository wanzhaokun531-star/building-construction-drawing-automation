#pragma once
#include "GenerationState.h"
#include <cstddef>

namespace bce {
// Historical identities are deliberately retained in the archive. Decoding never
// produces a current command/write qualification.
struct StoredProject {
    std::string persistentProjectId;
    ProjectConfig config;
    RuleInputs rules;
    std::string ruleInputsConfirmationSource;
    GenerationState generation;
    std::vector<EntityFingerprint> confirmedEntities;
};
inline constexpr std::size_t projectStoreMaxBytes = 4 * 1024 * 1024;
inline constexpr std::size_t projectStoreMaxItems = 16384;
struct ProjectArchive {
    StoredProject project;
    bool bound{false};
};
// A readable historical archive is not commit-ready. Keep the distinction in
// the type contract, rather than depending on every caller remembering a flag.
template<> struct Result<ProjectArchive> {
    std::optional<ProjectArchive> value;
    std::vector<Issue> issues;
    [[nodiscard]] bool readable() const noexcept {
        return value.has_value() && std::none_of(issues.begin(),issues.end(),
            [](const Issue& i){return i.blocking;});
    }
    [[nodiscard]] bool canCommit() const noexcept {
        return readable() && value->bound;
    }
};
// Filters unconfirmed candidates with explicit nonblocking diagnostics. Malformed
// confirmed records are errors, never downgraded silently.
[[nodiscard]] Result<StoredProject> confirmedProject(const StoredProject&);
[[nodiscard]] Result<std::string> encodeProject(const StoredProject&);
[[nodiscard]] Result<ProjectArchive> decodeProject(const std::string&);
// The caller must read currentPersistentProjectId from THIS database's archive.
// An exact complete root inventory and full fingerprints are mandatory. On any
// mismatch no keys are rebound; historical records remain available in value.
[[nodiscard]] Result<ProjectArchive> bindProjectArchive(const ProjectArchive&,
    const std::string& currentPersistentProjectId, const ObservedScene&, const std::vector<View>& currentViews);
}
