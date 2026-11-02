#ifndef BCE_GENERATION_STATE_H
#define BCE_GENERATION_STATE_H
#include "Recognition.h"

namespace bce {
// Opaque, complete reader signatures. Use an explicit nonempty sentinel for absent
// text/resources; an empty signature means the reader has not checked that part.
struct EntityFingerprint {
    EntityKey key;
    std::string geometry;
    std::string text;
    std::string resources;
    std::string nativeClass;
};
struct ObservedScene {
    SceneSnapshot scene;
    std::vector<EntityFingerprint> fingerprints;
};
// A caller-supplied qualification, bound to this exact command context. The core
// checks the contract; only a real adapter can establish the stated native facts.
struct CommandQualification {
    std::string documentIdentity;
    std::string databaseIdentity;
    std::uint64_t revision{0};
    std::string viewIdentity;
    std::string sourceSignature;
    std::string configSignature;
    std::string evidenceSource;
    bool verified{false};
};
struct CommandReadiness {
    CommandQualification rules;
    CommandQualification nativePreparation;
    CommandQualification glyphs;
    CommandQualification layout;
};
struct GenerationRequest {
    std::string moduleIdentity;
    std::string moduleVersion;
    std::string viewIdentity;
    std::string sourceSignature;
    std::vector<EntityKey> targets;
    CommandReadiness readiness;
};
struct GenerationCommandPlan {
    std::string documentIdentity;
    std::string databaseIdentity;
    std::uint64_t revision{0};
    std::string moduleIdentity;
    std::string moduleVersion;
    std::string viewIdentity;
    std::string sourceSignature;
    std::string configSignature;
    std::vector<EntityFingerprint> targets;
    std::vector<EntityFingerprint> preserved;
    CommandReadiness readiness;
};
struct GeneratedViewState {
    std::string viewIdentity;
    std::string moduleIdentity;
    std::string moduleVersion;
    std::string generationId;
    std::string sourceSignature;
    std::string configSignature;
    std::uint64_t committedRevision{0};
    std::vector<EntityFingerprint> sourceEntities;
    std::vector<EntityFingerprint> outputs;
    bool requiresReconfirmation{false};
    std::string commitEvidence;
    std::string postCommitEvidence;
};
struct GenerationState {
    std::string documentIdentity;
    std::string databaseIdentity;
    std::vector<GeneratedViewState> views;
};
enum class GenerationAction { generate, alreadyExists, reconfirmAndPrepare, manualChange };
struct GenerationDecision { GenerationAction action{GenerationAction::generate}; };
struct GenerationCheckEvidence { bool verified{false}; std::string source; };
// No SDK receipt is fabricated here. These are explicit adapter assertions with
// provenance, checked against a fresh snapshot; not a substitute for SDK tests.
struct ValidatedGenerationEvidence {
    std::string documentIdentity;
    std::string databaseIdentity;
    std::uint64_t plannedRevision{0};
    std::uint64_t committedRevision{0};
    std::string generationId;
    bool committed{false};
    bool cancelled{false};
    std::string commitEvidence;
    std::string postCommitEvidence;
    GenerationCheckEvidence measurementsEvidence;
    GenerationCheckEvidence nativeClassesEvidence;
    GenerationCheckEvidence resourcesEvidence;
    GenerationCheckEvidence glyphsEvidence;
    GenerationCheckEvidence preservedEvidence;
    std::vector<EntityFingerprint> targetsAfter;
    std::vector<EntityFingerprint> outputs;
};
struct InvalidatedGenerationState { ProjectConfig config; GenerationState state; };

[[nodiscard]] std::string generationConfigSignature(const ProjectConfig&, const std::string& viewIdentity);
[[nodiscard]] Result<GenerationCommandPlan> planGenerationCommand(
    const ObservedScene&, const ProjectConfig&, const GenerationRequest&);
[[nodiscard]] Result<GenerationCommandPlan> revalidateGenerationCommand(
    const GenerationCommandPlan&, const ObservedScene&, const ProjectConfig&);
[[nodiscard]] Result<GenerationDecision> decideGeneration(
    const GenerationState&, const ObservedScene&, const ProjectConfig&, const GenerationRequest&);
[[nodiscard]] Result<GenerationState> registerGeneration(
    const GenerationState&, const GenerationCommandPlan&, const ObservedScene&,
    const ValidatedGenerationEvidence&);
[[nodiscard]] InvalidatedGenerationState invalidateChangedGenerationViews(
    const GenerationState&, const ProjectConfig&, const std::vector<View>& currentViews);
}
#endif
