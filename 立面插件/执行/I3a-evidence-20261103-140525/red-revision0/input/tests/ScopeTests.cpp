#include "TestSupport.h"

namespace bce::test {
// Catches layer/rectangle expansion, preserved overlap and whole-target expansion.
void onlySelectedElevationIsWritable() {
    auto snapshot = scene();
    const auto original = snapshot;
    snapshot.targets.push_back({{"database-1", "E2", 7}, "shared", {{0, 100}, {100, 200}}, "elevation-E"});
    const auto selected = planWriteScope(snapshot, request());
    require(selected.canCommit() && selected.value->writableEntities.size() == 1 &&
        selected.value->writableEntities.front().handle == "E1", "only explicitly requested E1 is writable");
    require(selected.value->documentIdentity == "document-1" && selected.value->databaseIdentity == "database-1" &&
        selected.value->revision == 7 && selected.value->units.mmPerInputUnit == 1.0,
        "plan must retain snapshot identity and resolved scale");
    auto other = request();
    other.modifications = {{"database-1", "B1", 7}};
    const auto denied = planWriteScope(snapshot, other);
    require(!denied.value && hasCode(denied, "OUTSIDE_WRITE_SCOPE"), "adjacent shared-layer section must be rejected");
    require(denied.issues.front().source.viewIdentity == "elevation-E" &&
        denied.issues.front().source.entity && denied.issues.front().source.entity->handle == "B1",
        "denial must identify requested view and offending object");
    snapshot.preserved.push_back(snapshot.targets.front());
    const auto overlap = planWriteScope(snapshot, request());
    require(!overlap.value && hasCode(overlap, "OUTSIDE_WRITE_SCOPE"), "overlapping target/preserved sets must fail closed");
    snapshot = original;
    snapshot.units = {0, false, std::nullopt, "mm"};
    const auto unknown = planWriteScope(snapshot, request());
    require(!unknown.value && hasCode(unknown, "UNIT_REQUIRED"), "scope cannot commit unknown drawing units");
    auto missing = request();
    missing.modifications = {{"database-1", "FFFF", 7}};
    require(hasCode(planWriteScope(original, missing), "OUTSIDE_WRITE_SCOPE"), "unknown handle must be rejected");
}

// Catches shared-style overwrite, auto-renaming and duplicate resource additions.
void sharedStyleConflictKeepsOtherContent() {
    auto snapshot = scene();
    snapshot.resources = {{"TextStyle", "Shared", "font-A", true}};
    auto write = request();
    write.requiredResources = {{"TextStyle", "Shared", "font-B", false}};
    const auto conflict = planWriteScope(snapshot, write);
    require(!conflict.value && !conflict.canCommit() && hasCode(conflict, "RESOURCE_CONFLICT"),
        "protected content's shared style must not be overwritten");
    snapshot.resources.front().referencedByPreserved = false;
    require(hasCode(planWriteScope(snapshot, write), "RESOURCE_CONFLICT"),
        "minimal service must reject same-name different definitions even if unprotected");
    snapshot.resources.front().referencedByPreserved = true;
    write.requiredResources = {{"TextStyle", "Shared", "font-A", false},
        {"Layer", "New", "color-3", false}, {"Layer", "New", "color-3", false}};
    const auto accepted = planWriteScope(snapshot, write);
    require(accepted.canCommit() && accepted.value->reusedResources.size() == 1 &&
        accepted.value->newResources.size() == 1, "identical resources reused, new resources deduplicated");
    require(accepted.value->reusedResources.front().referencedByPreserved &&
        accepted.value->newResources.front().name == "New", "reuse retains actual resource, new resource keeps requested name");
    require(snapshot.resources.size() == 1 && snapshot.resources.front().definitionSignature == "font-A",
        "planning must not modify actual resource definitions");
    write.requiredResources.push_back({"Layer", "New", "color-4", false});
    require(hasCode(planWriteScope(snapshot, write), "RESOURCE_CONFLICT"), "request-internal definition conflict must fail");
}

// Catches handle-only equality, switched document and stale entity/snapshot revision.
void staleDocumentOrEntityCannotCommit() {
    const auto snapshot = scene();
    auto write = request();
    write.expectedDocumentIdentity = "different-document";
    require(hasCode(planWriteScope(snapshot, write), "STALE_DOCUMENT"), "document switch must fail");
    write = request();
    write.expectedRevision = 6;
    require(hasCode(planWriteScope(snapshot, write), "STALE_DOCUMENT"), "changed snapshot revision must fail");
    write = request();
    write.modifications.front().revision = 6;
    auto result = planWriteScope(snapshot, write);
    require(!result.value && !result.canCommit() && hasCode(result, "STALE_DOCUMENT"), "old entity revision must fail");
    write = request();
    write.modifications.front().databaseIdentity = "foreign-database";
    result = planWriteScope(snapshot, write);
    require(!result.value && !result.canCommit() && hasCode(result, "STALE_DOCUMENT"), "same handle in different database must fail");
    auto staleScene = snapshot;
    staleScene.targets.front().key.revision = 6;
    require(hasCode(planWriteScope(staleScene, request()), "STALE_DOCUMENT"), "inconsistent snapshot entity must fail");
    staleScene = snapshot;
    staleScene.documentIdentity.clear();
    write = request();
    write.expectedDocumentIdentity.clear();
    require(hasCode(planWriteScope(staleScene, write), "STALE_DOCUMENT"), "empty matching document identities cannot authorize writes");
    staleScene = snapshot;
    staleScene.preserved.front().key.databaseIdentity = "foreign-database";
    require(hasCode(planWriteScope(staleScene, request()), "STALE_DOCUMENT"), "all snapshot keys must belong to the database");
}

// Catches case-sensitive symbol matching that treats protected Shared as a new shared style.
void resourceNameCaseVariantsConflict() {
    auto snapshot = scene();
    snapshot.resources = {{"TextStyle", "Shared", "font-A", true}};
    auto write = request();
    write.requiredResources = {{"TextStyle", "shared", "font-B", false}};
    const auto result = planWriteScope(snapshot, write);
    require(!result.value && !result.canCommit() && hasCode(result, "RESOURCE_CONFLICT"),
        "Shared/shared with different definitions must conflict, never become a new resource");
}

// Catches duplicate resources when an equivalent name/kind differs only in letter case.
void resourceNameCaseVariantsReuse() {
    auto snapshot = scene();
    snapshot.resources = {{"TextStyle", "Shared", "font-A", true}};
    auto write = request();
    write.requiredResources = {{"textstyle", "sHaReD", "font-A", false}};
    const auto result = planWriteScope(snapshot, write);
    require(result.canCommit() && result.value->newResources.empty() && result.value->reusedResources.size() == 1 &&
        result.value->reusedResources.front().name == "Shared" && result.value->reusedResources.front().referencedByPreserved,
        "case variants must reuse the actual protected resource without changing its spelling");
}

// Catches request-local collision bypass through mixed spelling, plus duplicate new-resource creation.
void resourceRequestCaseVariantsConflict() {
    auto write = request();
    write.requiredResources = {{"Layer", "New", "color-3", false}, {"layer", "NEW", "color-4", false}};
    const auto conflict = planWriteScope(scene(), write);
    require(!conflict.value && hasCode(conflict, "RESOURCE_CONFLICT"),
        "request-local New/NEW with different definitions must conflict");
    write.requiredResources.back().definitionSignature = "color-3";
    const auto duplicate = planWriteScope(scene(), write);
    require(duplicate.canCommit() && duplicate.value->newResources.size() == 1,
        "request-local equivalent new resources must be added only once");
}

// Catches byte-wise ASCII-only folding or UTF-8 corruption while retaining Chinese text.
void utf8ResourceCaseVariantsConflict() {
    auto snapshot = scene();
    snapshot.resources = {{"TextStyle", u8"共享样式É", "font-A", true}};
    auto write = request();
    write.requiredResources = {{"TextStyle", u8"共享样式é", "font-B", false}};
    const auto result = planWriteScope(snapshot, write);
    require(!result.value && hasCode(result, "RESOURCE_CONFLICT"),
        "UTF-8 Chinese names with Unicode case variants must share symbol identity");
}

void utf8ResourceCaseVariantsReuse() {
    auto snapshot = scene();
    snapshot.resources = {{"TextStyle", u8"共享样式É", "font-A", true}};
    auto write = request();
    write.requiredResources = {{"TextStyle", u8"共享样式é", "font-A", false},
        {"TextStyle", u8"另一样式é", "font-A", false}};
    const auto result = planWriteScope(snapshot, write);
    require(result.canCommit() && result.value->reusedResources.size() == 1 && result.value->newResources.size() == 1 &&
        result.value->newResources.front().name == u8"另一样式é" && result.value->reusedResources.front().name == u8"共享样式É",
        "Unicode case-equivalent names reuse actual spelling; distinct Chinese names remain distinct");
}

// Catches replacement decoding, empty names and embedded NUL truncation in native name comparison.
void invalidUtf8ResourceNamesCannotCommit() {
    for (const auto& invalid : {std::string("\xC3\x28", 2), std::string("Shared\0suffix", 13), std::string{}}) {
        auto write = request();
        write.requiredResources = {{"TextStyle", invalid, "font-A", false}};
        const auto result = planWriteScope(scene(), write);
        require(!result.value && hasCode(result, "RESOURCE_CONFLICT"), "invalid UTF-8 or unrepresentable symbol names must fail closed");
        auto snapshot = scene();
        snapshot.resources = {{invalid, "Shared", "font-A", true}};
        require(hasCode(planWriteScope(snapshot, request()), "RESOURCE_CONFLICT"), "invalid actual symbol kinds must also fail closed");
    }
}

// Catches target/preserved overlap hidden by hexadecimal case spelling.
void handleCaseVariantCannotBypassProtection() {
    auto snapshot = scene();
    snapshot.targets.front().key.handle = "A1";
    snapshot.preserved.front().key.handle = "a1";
    auto write = request();
    write.modifications.front().handle = "A1";
    const auto result = planWriteScope(snapshot, write);
    require(!result.value && !result.canCommit() && hasCode(result, "OUTSIDE_WRITE_SCOPE"),
        "A1/a1 target-preserved overlap must fail closed");
}

void handleLeadingZerosCannotBypassProtection() {
    auto snapshot = scene();
    snapshot.targets.front().key.handle = "A1";
    snapshot.preserved.front().key.handle = "000a1";
    auto write = request();
    write.modifications.front().handle = "A1";
    const auto result = planWriteScope(snapshot, write);
    require(!result.value && !result.canCommit() && hasCode(result, "OUTSIDE_WRITE_SCOPE"),
        "leading-zero aliases must not bypass target-preserved overlap checks");
}

void equivalentHandlesSelectOnce() {
    auto snapshot = scene();
    snapshot.targets.front().key.handle = "A1";
    auto write = request();
    write.modifications = {{"database-1", "a1", 7}, {"database-1", "000A1", 7}};
    const auto selected = planWriteScope(snapshot, write);
    require(selected.canCommit() && selected.value->writableEntities.size() == 1,
        "equivalent hexadecimal request handles must select one authorized entity");
    snapshot.targets.front().key.handle = "FFFFFFFFFFFFFFFF";
    write.modifications = {{"database-1", "000ffffffffffffffff", 7}};
    require(planWriteScope(snapshot, write).canCommit(), "full-width uint64 handle with leading zeros must remain valid");
    write.modifications.front().revision = 6;
    require(hasCode(planWriteScope(snapshot, write), "STALE_DOCUMENT"), "normalizing a handle must not erase revision identity");
    write.modifications.front().revision = 7;
    write.modifications.front().databaseIdentity = "other-database";
    require(hasCode(planWriteScope(snapshot, write), "STALE_DOCUMENT"), "normalizing a handle must not erase database identity");
}

void malformedHexHandlesCannotCommit() {
    for (const std::string invalid : {"S1", "0", "10000000000000000", "-A1", " A1", "0xA1", ""}) {
        auto snapshot = scene();
        auto write = request();
        snapshot.targets.front().key.handle = invalid;
        write.modifications.front().handle = invalid;
        const auto result = planWriteScope(snapshot, write);
        require(!result.value && !result.canCommit() && hasCode(result, "STALE_DOCUMENT"),
            "malformed, null or overflowing hexadecimal handles must not authorize writes");
    }
}
}
