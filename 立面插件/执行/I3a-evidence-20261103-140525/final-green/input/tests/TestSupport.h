#ifndef BCE_TEST_SUPPORT_H
#define BCE_TEST_SUPPORT_H
#include "../src/ElevationModel.h"
#include <stdexcept>
#include <string>

namespace bce::test {
inline void require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
template<class T>
bool hasCode(const Result<T>& result, const std::string& code) {
    for (const auto& issue : result.issues) {
        if (issue.code == code && issue.blocking && !issue.message.empty()) { return true; }
    }
    return false;
}
inline SceneSnapshot scene() {
    SceneSnapshot snapshot;
    snapshot.documentIdentity = "document-1";
    snapshot.databaseIdentity = "database-1";
    snapshot.revision = 7;
    snapshot.units = {4, true, std::nullopt, "mm"};
    snapshot.targets = {{{"database-1", "E1", 7}, "shared", {{0, 0}, {100, 100}}, "elevation-E"}};
    snapshot.preserved = {{{"database-1", "B1", 7}, "shared", {{101, 0}, {200, 100}}, "section-S"}};
    return snapshot;
}
inline WriteRequest request() {
    return {"document-1", 7, "elevation-E", {{"database-1", "E1", 7}}, {}};
}
void undefinedUnitRequiresConfirmation();
void knownUnitsConflictNeedsConfirmation();
void onlySelectedElevationIsWritable();
void sharedStyleConflictKeepsOtherContent();
void staleDocumentOrEntityCannotCommit();
void resourceNameCaseVariantsConflict();
void resourceNameCaseVariantsReuse();
void resourceRequestCaseVariantsConflict();
void utf8ResourceCaseVariantsConflict();
void utf8ResourceCaseVariantsReuse();
void invalidUtf8ResourceNamesCannotCommit();
void handleCaseVariantCannotBypassProtection();
void handleLeadingZerosCannotBypassProtection();
void equivalentHandlesSelectOnce();
void malformedHexHandlesCannotCommit();
}
#endif
