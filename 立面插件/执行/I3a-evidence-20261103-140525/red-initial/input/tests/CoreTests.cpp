#include "TestSupport.h"
#include <array>
#include <iostream>
#include <string>
namespace bce::test {
void factSelectorsReadActualDefinitionPoints();
void factElevationUsesConfirmedUnitsDirectionAndDatum();
void factRotatedWindowRetainsFourCorners();
void factChangesLocateOnlyRequiredSubset();
void factReopenRequiresFreshConsistentObservation();
void factUnsupportedSourcesAndNonfiniteValuesAreRejected();
void factObservationRejectsAmbiguityAndUnsupportedChanges();
void factSubsetMatchNeverAuthorizesGeneration();
void missingConfigDoesNotWriteDrawing();
void documentSwitchRejectsStaleCommit();
void repeatedBuildDoesNotDuplicate();
void manualChangesAreNotSilentlyReplaced();
void uncommittedEvidenceCannotRegister();
void changedViewInvalidatesOnlyItsState();
void sameLayerDoesNotConferOwnership();
void foreignOrMissingOutputCannotRegister();
void staleReadinessAndChangedConfigCannotAuthorize();
void registrationRequiresExactTargetAndPreservedInventory();
void manualChangeTakesPriorityOverSourceInvalidation();
void unrelatedRevisionKeepsHistoricalOwnershipButStalesPreparation();
void malformedOwnershipCannotAuthorizeAnotherWrite();
void rowsDependOnAvailableArea();
void noFitDoesNotScaleArchitecture();
void protectedSectionIsFixedObstacle();
void collisionIncludesDimensionGlyphAndBaseline();
void paperSizeTracksSelectedScale();
void incompleteGlyphEvidenceBlocksLayout();
void malformedOrOverflowingGeometryRejected();
void searchLimitIsNotNoSpace();
void duplicateLayoutIdentityAndKeysRejected();
void singleTranslationRetainsAllVisiblePartsAndTargets();
void layoutBacktrackingAndReorderAreDeterministic();
void individuallyConfirmedFloorsFormOneChain();
void annotationPlanRetainsConfirmationAndUnitEvidence();
void materialDuplicateTopologyIgnoresRedundantVertices();
void emptyMaterialFamilyIsNotAnExplicitSelection();
void materialPlanRetainsFamilyAndUnitEvidence();

void projectLevelsDoNotUseReferenceNumbers();
void openingDimensionsUseActualEdges();
void translationDoesNotChangeMeasurements();
void nonRoundCoordinatesArePreserved();
void duplicateStationsNeverCreateZeroSegments();
void dimensionFamilyKeepsItsVerifiedSpacing();
void missingDatumOrSemanticRoleBlocksAnnotation();
void axisChainRequiresConfirmedPlan();
void featureRolesAndDirectionsRemainTraceable();
void wallMaterialExcludesWindowHole();
void invalidOpenRegionNeedsCorrection();
void missingConstructionAndDetailIdBlocksItsAnnotation();
void materialDefaultsAreProjectOverridable();
void materialRegionsRequireUniqueExplicitAssociation();
void referenceAndCapabilityEvidenceRemainsUnchanged();

void fourElevationsExcludeTwoSections();
void sceneTranslationAndQuarterTurnKeepMembership();
void verticalOrGridViewsUseTwoDimensionalGaps();
void ambiguousRectangleNeedsRole();
void mirroredNestedBlockKeepsWorldGeometry();
void multipleBuildingsRemainSeparate();
void axisProjectionReversesOrder();
void levelConflictRequiresConfirmation();
void changedGeometryInvalidatesOnlyAffectedConfig();
void overlappingOrUnlabelledViewsNeedConfirmation();
void overflowingGapDoesNotRecurse();
void hugeLevelDirectionKeepsFiniteCandidate();
void overflowingAxisSpacingIsRejected();
void mirroredTextPreservesBothBasisDirections();
void nativeDetailedGeometryInvalidatesOnlyAffectedView();
void hiddenTitleDoesNotAnchorView();
void hiddenEdgeDoesNotCloseRectangle();
void hiddenLevelDoesNotAssociate();
}

namespace {
struct TestCase { const char* name; void (*run)(); };
const std::array<TestCase, 85> cases{{
    {"FactSelectorsReadActualDefinitionPoints", bce::test::factSelectorsReadActualDefinitionPoints},
    {"FactElevationUsesConfirmedUnitsDirectionAndDatum", bce::test::factElevationUsesConfirmedUnitsDirectionAndDatum},
    {"FactRotatedWindowRetainsFourCorners", bce::test::factRotatedWindowRetainsFourCorners},
    {"FactChangesLocateOnlyRequiredSubset", bce::test::factChangesLocateOnlyRequiredSubset},
    {"FactReopenRequiresFreshConsistentObservation", bce::test::factReopenRequiresFreshConsistentObservation},
    {"FactUnsupportedSourcesAndNonfiniteValuesAreRejected", bce::test::factUnsupportedSourcesAndNonfiniteValuesAreRejected},
    {"FactObservationRejectsAmbiguityAndUnsupportedChanges", bce::test::factObservationRejectsAmbiguityAndUnsupportedChanges},
    {"FactSubsetMatchNeverAuthorizesGeneration", bce::test::factSubsetMatchNeverAuthorizesGeneration},
    {"MissingConfigDoesNotWriteDrawing", bce::test::missingConfigDoesNotWriteDrawing},
    {"DocumentSwitchRejectsStaleCommit", bce::test::documentSwitchRejectsStaleCommit},
    {"RepeatedBuildDoesNotDuplicate", bce::test::repeatedBuildDoesNotDuplicate},
    {"ManualChangesAreNotSilentlyReplaced", bce::test::manualChangesAreNotSilentlyReplaced},
    {"UncommittedEvidenceCannotRegister", bce::test::uncommittedEvidenceCannotRegister},
    {"ChangedViewInvalidatesOnlyItsState", bce::test::changedViewInvalidatesOnlyItsState},
    {"SameLayerDoesNotConferOwnership", bce::test::sameLayerDoesNotConferOwnership},
    {"ForeignOrMissingOutputCannotRegister", bce::test::foreignOrMissingOutputCannotRegister},
    {"StaleReadinessAndChangedConfigCannotAuthorize", bce::test::staleReadinessAndChangedConfigCannotAuthorize},
    {"RegistrationRequiresExactTargetAndPreservedInventory", bce::test::registrationRequiresExactTargetAndPreservedInventory},
    {"ManualChangeTakesPriorityOverSourceInvalidation", bce::test::manualChangeTakesPriorityOverSourceInvalidation},
    {"UnrelatedRevisionKeepsHistoricalOwnershipButStalesPreparation", bce::test::unrelatedRevisionKeepsHistoricalOwnershipButStalesPreparation},
    {"MalformedOwnershipCannotAuthorizeAnotherWrite", bce::test::malformedOwnershipCannotAuthorizeAnotherWrite},
    {"RowsDependOnAvailableArea", bce::test::rowsDependOnAvailableArea},
    {"NoFitDoesNotScaleArchitecture", bce::test::noFitDoesNotScaleArchitecture},
    {"ProtectedSectionIsFixedObstacle", bce::test::protectedSectionIsFixedObstacle},
    {"CollisionIncludesDimensionGlyphAndBaseline", bce::test::collisionIncludesDimensionGlyphAndBaseline},
    {"PaperSizeTracksSelectedScale", bce::test::paperSizeTracksSelectedScale},
    {"IncompleteGlyphEvidenceBlocksLayout", bce::test::incompleteGlyphEvidenceBlocksLayout},
    {"MalformedOrOverflowingGeometryRejected", bce::test::malformedOrOverflowingGeometryRejected},
    {"SearchLimitIsNotNoSpace", bce::test::searchLimitIsNotNoSpace},
    {"DuplicateLayoutIdentityAndKeysRejected", bce::test::duplicateLayoutIdentityAndKeysRejected},
    {"SingleTranslationRetainsAllVisiblePartsAndTargets", bce::test::singleTranslationRetainsAllVisiblePartsAndTargets},
    {"LayoutBacktrackingAndReorderAreDeterministic", bce::test::layoutBacktrackingAndReorderAreDeterministic},
    {"IndividuallyConfirmedFloorsFormOneChain", bce::test::individuallyConfirmedFloorsFormOneChain},
    {"AnnotationPlanRetainsConfirmationAndUnitEvidence", bce::test::annotationPlanRetainsConfirmationAndUnitEvidence},
    {"MaterialDuplicateTopologyIgnoresRedundantVertices", bce::test::materialDuplicateTopologyIgnoresRedundantVertices},
    {"EmptyMaterialFamilyIsNotAnExplicitSelection", bce::test::emptyMaterialFamilyIsNotAnExplicitSelection},
    {"MaterialPlanRetainsFamilyAndUnitEvidence", bce::test::materialPlanRetainsFamilyAndUnitEvidence},
    {"ProjectLevelsDoNotUseReferenceNumbers", bce::test::projectLevelsDoNotUseReferenceNumbers},
    {"OpeningDimensionsUseActualEdges", bce::test::openingDimensionsUseActualEdges},
    {"TranslationDoesNotChangeMeasurements", bce::test::translationDoesNotChangeMeasurements},
    {"NonRoundCoordinatesArePreserved", bce::test::nonRoundCoordinatesArePreserved},
    {"DuplicateStationsNeverCreateZeroSegments", bce::test::duplicateStationsNeverCreateZeroSegments},
    {"DimensionFamilyKeepsItsVerifiedSpacing", bce::test::dimensionFamilyKeepsItsVerifiedSpacing},
    {"MissingDatumOrSemanticRoleBlocksAnnotation", bce::test::missingDatumOrSemanticRoleBlocksAnnotation},
    {"AxisChainRequiresConfirmedPlan", bce::test::axisChainRequiresConfirmedPlan},
    {"FeatureRolesAndDirectionsRemainTraceable", bce::test::featureRolesAndDirectionsRemainTraceable},
    {"WallMaterialExcludesWindowHole", bce::test::wallMaterialExcludesWindowHole},
    {"InvalidOpenRegionNeedsCorrection", bce::test::invalidOpenRegionNeedsCorrection},
    {"MissingConstructionAndDetailIdBlocksItsAnnotation", bce::test::missingConstructionAndDetailIdBlocksItsAnnotation},
    {"MaterialDefaultsAreProjectOverridable", bce::test::materialDefaultsAreProjectOverridable},
    {"MaterialRegionsRequireUniqueExplicitAssociation", bce::test::materialRegionsRequireUniqueExplicitAssociation},
    {"ReferenceAndCapabilityEvidenceRemainsUnchanged", bce::test::referenceAndCapabilityEvidenceRemainsUnchanged},
    {"FourElevationsExcludeTwoSections", bce::test::fourElevationsExcludeTwoSections},
    {"SceneTranslationAndQuarterTurnKeepMembership", bce::test::sceneTranslationAndQuarterTurnKeepMembership},
    {"VerticalOrGridViewsUseTwoDimensionalGaps", bce::test::verticalOrGridViewsUseTwoDimensionalGaps},
    {"AmbiguousRectangleNeedsRole", bce::test::ambiguousRectangleNeedsRole},
    {"MirroredNestedBlockKeepsWorldGeometry", bce::test::mirroredNestedBlockKeepsWorldGeometry},
    {"MultipleBuildingsRemainSeparate", bce::test::multipleBuildingsRemainSeparate},
    {"AxisProjectionReversesOrder", bce::test::axisProjectionReversesOrder},
    {"LevelConflictRequiresConfirmation", bce::test::levelConflictRequiresConfirmation},
    {"ChangedGeometryInvalidatesOnlyAffectedConfig", bce::test::changedGeometryInvalidatesOnlyAffectedConfig},
    {"OverlappingOrUnlabelledViewsNeedConfirmation", bce::test::overlappingOrUnlabelledViewsNeedConfirmation},
    {"UndefinedUnitRequiresConfirmation", bce::test::undefinedUnitRequiresConfirmation},
    {"KnownUnitsConflictNeedsConfirmation", bce::test::knownUnitsConflictNeedsConfirmation},
    {"OnlySelectedElevationIsWritable", bce::test::onlySelectedElevationIsWritable},
    {"SharedStyleConflictKeepsOtherContent", bce::test::sharedStyleConflictKeepsOtherContent},
    {"StaleDocumentOrEntityCannotCommit", bce::test::staleDocumentOrEntityCannotCommit},
    {"ResourceNameCaseVariantsConflict", bce::test::resourceNameCaseVariantsConflict},
    {"ResourceNameCaseVariantsReuse", bce::test::resourceNameCaseVariantsReuse},
    {"ResourceRequestCaseVariantsConflict", bce::test::resourceRequestCaseVariantsConflict},
    {"Utf8ResourceCaseVariantsConflict", bce::test::utf8ResourceCaseVariantsConflict},
    {"Utf8ResourceCaseVariantsReuse", bce::test::utf8ResourceCaseVariantsReuse},
    {"InvalidUtf8ResourceNamesCannotCommit", bce::test::invalidUtf8ResourceNamesCannotCommit},
    {"HandleCaseVariantCannotBypassProtection", bce::test::handleCaseVariantCannotBypassProtection},
    {"HandleLeadingZerosCannotBypassProtection", bce::test::handleLeadingZerosCannotBypassProtection},
    {"EquivalentHandlesSelectOnce", bce::test::equivalentHandlesSelectOnce},
    {"MalformedHexHandlesCannotCommit", bce::test::malformedHexHandlesCannotCommit},
    {"HugeLevelDirectionKeepsFiniteCandidate", bce::test::hugeLevelDirectionKeepsFiniteCandidate},
    {"OverflowingAxisSpacingIsRejected", bce::test::overflowingAxisSpacingIsRejected},
    {"MirroredTextPreservesBothBasisDirections", bce::test::mirroredTextPreservesBothBasisDirections},
    {"NativeDetailedGeometryInvalidatesOnlyAffectedView", bce::test::nativeDetailedGeometryInvalidatesOnlyAffectedView},
    {"HiddenTitleDoesNotAnchorView", bce::test::hiddenTitleDoesNotAnchorView},
    {"HiddenEdgeDoesNotCloseRectangle", bce::test::hiddenEdgeDoesNotCloseRectangle},
    {"HiddenLevelDoesNotAssociate", bce::test::hiddenLevelDoesNotAssociate},
    {"OverflowingGapDoesNotRecurse", bce::test::overflowingGapDoesNotRecurse}
}};
}
int main(int argc, char** argv) {
    const bool all = argc == 2 && std::string(argv[1]) == "--all";
    const bool single = argc == 3 && std::string(argv[1]) == "--case";
    if (!all && !single) { std::cerr << "usage: ElevationCoreTests.exe --all | --case <name>\n"; return 2; }
    int executed = 0;
    int failures = 0;
    for (const auto& test : cases) {
        if (!all && std::string(argv[2]) != test.name) { continue; }
        ++executed;
        try { test.run(); std::cout << "PASS " << test.name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << test.name << ": " << error.what() << '\n'; }
    }
    if (executed == 0) { std::cerr << "unknown case: " << argv[2] << '\n'; return 2; }
    std::cout << "Executed " << executed << ", failed " << failures << '\n';
    return failures == 0 ? 0 : 1;
}


