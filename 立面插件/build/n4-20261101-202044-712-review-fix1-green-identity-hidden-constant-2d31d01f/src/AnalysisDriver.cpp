#include "AnalysisFixtures.h"
#include "AnalysisReader.h"
#include "ExpectedMembership.h"
#include "NativeEvidence.h"
#include <acdocman.h>
#include <aced.h>
#include <acutads.h>
#include <acutmem.h>
#include <adscodes.h>
#include <dbapserv.h>
#include <dbents.h>
#include <dbidmap.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <exception>
#include <memory>
#include <rxregsvc.h>
#include <set>
namespace {
HMODULE moduleHandle{};
std::string globals() {
    bce::native::Fields fields;
    for (const auto *name : {L"INSUNITS", L"LTSCALE", L"CELTSCALE", L"DIMSTYLE", L"TEXTSTYLE", L"FRAME",
                             L"WIPEOUTFRAME", L"IMAGEFRAME", L"XCLIPFRAME", L"FONTALT", L"MIRRTEXT"}) {
        resbuf value{};
        bce::native::require(acedGetVar(name, &value) == RTNORM, "global read");
        std::string encoded;
        if (value.restype == RTSHORT)
            encoded = std::to_string(value.resval.rint);
        else if (value.restype == RTLONG)
            encoded = std::to_string(value.resval.rlong);
        else if (value.restype == RTREAL)
            encoded = bce::analysis_test::number(value.resval.rreal);
        else if (value.restype == RTSTR) {
            encoded = bce::native::quote(std::wstring(value.resval.rstring));
            acutDelString(value.resval.rstring);
        } else
            throw std::runtime_error("unsupported global type");
        fields.push_back({bce::native::utf8(name), encoded});
    }
    return bce::native::object(fields);
}
void run_case() noexcept {
    bce::native::Config config;
    std::unique_ptr<bce::native::Evidence> evidence;
    try {
        using namespace bce;
        using namespace bce::native;
        config = load_config(moduleHandle);
        evidence = std::make_unique<Evidence>(config);
        validate_identity(config);
        require(config.caseName == L"NativeAnalysisAndRecognition", "unsupported case");
        bool passed = true;
        auto expect = [&](const std::string &name, bool ok, const std::string &detail) {
            evidence->step(name, ok ? "passed" : "failed", detail);
            passed = passed && ok;
        };
        AnalysisReadOptions options;
        options.confirmedMmPerInputUnit = 1.0;
        options.sourceSha256 = config.schemeSHA256;
        require(acDocManager->curDocument() != nullptr, "no current fixture document");
        auto *ambientDb = acDocManager->curDocument()->database();
        analysis_test::Monitor ambientMonitor(*ambientDb);
        const auto globalsBefore = globals();
        const auto *workingBefore = acdbHostApplicationServices()->workingDatabase();
        evidence->checkpoint("before-read-only-scheme-database");
        AcDbDatabase source(false, true);
        require(source.readDwgFile(config.scheme.c_str(), AcDbDatabase::kForReadAndReadShare, false) ==
                    Acad::eOk,
                "read scheme failed");
        source.closeInput(true);
        analysis_test::Monitor sourceMonitor(source);
        sourceMonitor.phase = "before-resources";
        const auto beforeResources = native_scene::readResources(source);
        options.diagnosticObserver = [&](const char *stage, const EntityKey &key) {
            sourceMonitor.phase = stage;
            sourceMonitor.readingHandle = key.handle;
            ambientMonitor.phase = stage;
            ambientMonitor.readingHandle = key.handle;
        };
        const auto result = readDatabaseAnalysis(source, "side-database:" + config.schemeSHA256, 1, options);
        std::size_t elevations = 0, sections = 0, members = 0, preserved = 0;
        if (result.value) {
            for (const auto &view : result.value->recognition.views) {
                elevations += view.kind == ViewKind::elevation;
                sections += view.kind == ViewKind::section;
                members += view.members.size();
            }
            preserved = result.value->recognition.preserved.size();
        }
        expect("ActualSchemeFeedsRecognition",
               result.value && elevations == 4 && sections == 2 && members == 741 && preserved == 174,
               "elevations=" + std::to_string(elevations) + " sections=" + std::to_string(sections) +
                   " members=" + std::to_string(members) + " preserved=" + std::to_string(preserved));
        std::map<std::string, std::string> actual;
        bool noDuplicate = true, noAxes = true, missingPlan = true;
        if (result.value) {
            for (const auto &view : result.value->recognition.views) {
                for (const auto &member : view.members)
                    noDuplicate = actual.emplace(member.handle, view.titleKey.handle).second && noDuplicate;
                noAxes = noAxes && !view.projectedAxes;
                missingPlan = missingPlan &&
                              std::any_of(view.unconfirmed.begin(), view.unconfirmed.end(),
                                          [](const Issue &i) { return i.code == diagnostic::planRequired; });
            }
            evidence->capability(analysis_test::details(*result.value));
        } else
            for (const auto &i : result.issues)
                evidence->step("ReadDiagnostic", "failed", i.message);
        expect("Exact741RootMembership", result.value && noDuplicate && actual == expectedMembership(),
               "independent frozen expected TSV embedded only in test module; every actual root mapped once");
        expect("AbsentPlanRemainsExplicit", result.value && noAxes && missingPlan,
               "PLAN_REQUIRED in every view; no invented axes");
        const auto afterResources = native_scene::readResources(source);
        bool resourcesEqual = beforeResources.size() == afterResources.size();
        for (std::size_t i = 0; resourcesEqual && i < beforeResources.size(); ++i)
            resourcesEqual = beforeResources[i].kind == afterResources[i].kind &&
                             beforeResources[i].name == afterResources[i].name &&
                             beforeResources[i].definitionSignature == afterResources[i].definitionSignature;
        expect("SourceDatabaseAndResourcesUnchanged",
               sourceMonitor.changes == 0 && ambientMonitor.changes == 0 && resourcesEqual,
               "source events=" + std::to_string(sourceMonitor.changes) +
                   " ambient current events=" + std::to_string(ambientMonitor.changes));
        evidence->capability(object({{"diagnostic", quote(std::string("source-modification-events"))},
                                     {"events", array(sourceMonitor.events)},
                                     {"loggingFailed", sourceMonitor.loggingFailed ? "true" : "false"}}));
        options.diagnosticObserver = {};
        const auto *documentBeforeFailure = acDocManager->curDocument();
        const auto ambientBeforeFailure = ambientMonitor.changes;
        const auto sourceBeforeFailure = sourceMonitor.changes;
        bool injected = false;
        options.diagnosticObserver = [&](const char *stage, const EntityKey &) {
            if (std::string(stage) == "sandbox-working-database-bound") {
                injected = true;
                throw std::runtime_error("injected observer failure after owned working DB switch");
            }
        };
        const auto injectedResult = readDatabaseAnalysis(source, "exception-source", 1, options);
        expect("WorkingDatabaseRestoredAfterException",
               injected && !injectedResult.value &&
                   workingBefore == acdbHostApplicationServices()->workingDatabase() &&
                   documentBeforeFailure == acDocManager->curDocument() &&
                   ambientBeforeFailure == ambientMonitor.changes &&
                   sourceBeforeFailure == sourceMonitor.changes,
               "exception after owned workingDatabase switch restores exact pointer/document with zero "
               "source or ambient events");
        options.diagnosticObserver = {};
        evidence->checkpoint("before-current-unsaved-read");
        NativeSceneContext context;
        auto *db = context.currentDatabase();
        require(db != nullptr, "current database unavailable");
        // Match the existing SceneReader/InPlaceWriter contract without calling
        // their source-bound geometry getters (which could warm the font cache).
        const auto identityBlock = analysis_test::block(*db, L"N4_IdentityLines");
        analysis_test::append(identityBlock, std::make_unique<AcDbLine>(
                                                  AcGePoint3d(0, 0, 0), AcGePoint3d(1, 1, 0)));
        analysis_test::append(acdbSymUtil()->blockModelSpaceId(db),
                              std::make_unique<AcDbBlockReference>(AcGePoint3d(0, 0, 0), identityBlock));
        const auto identityRevision = context.revision();
        const auto identityEvents = ambientMonitor.changes;
        const auto identityRead = readCurrentAnalysis(context, options);
        const auto documentToken = context.documentIdentity();
        const auto databaseToken = documentToken + "/database";
        bool identities = identityRead.value &&
                          identityRead.value->scene.documentIdentity == documentToken &&
                          identityRead.value->scene.databaseIdentity == databaseToken &&
                          identityRead.value->scene.revision == identityRevision;
        std::size_t identityLeaves = 0;
        if (identityRead.value) {
            const auto validKey = [&](const EntityKey &key) {
                return key.databaseIdentity == databaseToken && key.revision == identityRevision &&
                       !key.handle.empty();
            };
            for (const auto &p : identityRead.value->scene.primitives)
                identities = identities && validKey(p.key);
            for (const auto &r : identityRead.value->geometry) {
                identities = identities && validKey(r.root) && validKey(r.leaf) &&
                             validKey(r.worldGeometry.key);
                identityLeaves += r.root.handle != r.leaf.handle;
            }
            for (const auto &e : identityRead.value->scene.preserved)
                identities = identities && validKey(e.key);
            for (const auto &i : identityRead.value->readIssues)
                identities = identities && i.source.documentIdentity == documentToken &&
                             (!i.source.entity || validKey(*i.source.entity));
        }
        expect("CurrentIdentityMatchesSceneWriterContract",
               identities && identityLeaves > 0 && context.revision() == identityRevision &&
                   ambientMonitor.changes == identityEvents,
               "document=" + documentToken + " expected database=" + databaseToken +
                   " actual=" + (identityRead.value ? identityRead.value->scene.databaseIdentity : "none") +
                   "; actual root/leaf keys and diagnostics share the existing context contract");
        // Construct away from Main: Text construction/close can already load fonts.
        // Import raw geometry/style, then prove Main's font cache stayed untouched.
        const auto fontEventsBeforeFixture = ambientMonitor.fontEvents;
        ambientMonitor.phase = "install-cold-current-fixture";
        ambientMonitor.readingHandle.clear();
        AcDbObjectId id;
        {
            AcDbDatabase fixtureDb(true, true);
            struct FixtureWorkingScope {
                AcDbDatabase *previous;
                explicit FixtureWorkingScope(AcDbDatabase &own)
                    : previous(acdbHostApplicationServices()->workingDatabase()) {
                    acdbHostApplicationServices()->setWorkingDatabase(&own);
                }
                ~FixtureWorkingScope() { acdbHostApplicationServices()->setWorkingDatabase(previous); }
            } fixtureWorking(fixtureDb);
            AcDbObjectId coldStyle;
            {
                analysis_test::Open<AcDbTextStyleTable> styles(fixtureDb.textStyleTableId(), AcDb::kForWrite);
                auto style = std::make_unique<AcDbTextStyleTableRecord>();
                analysis_test::ok(style->setName(L"N4_ColdFontSource"), "cold style name");
                analysis_test::ok(style->setFileName(L"simhei.ttf"), "cold font");
                analysis_test::ok(styles->add(coldStyle, style.get()), "cold style append");
                style.release()->close();
            }
            const auto temporaryText = analysis_test::append(
                acdbSymUtil()->blockModelSpaceId(&fixtureDb),
                std::make_unique<AcDbText>(AcGePoint3d(10, 20, 0), L"未保存中文立面图", coldStyle, 3.0));
            AcDbObjectIdArray ids;
            ids.append(temporaryText);
            AcDbIdMapping mapping;
            analysis_test::ok(fixtureDb.wblockCloneObjects(ids, acdbSymUtil()->blockModelSpaceId(db), mapping,
                                                           AcDb::kDrcIgnore),
                              "cold current fixture import");
            AcDbIdPair pair;
            pair.setKey(temporaryText);
            require(mapping.compute(pair) && pair.isCloned() && !pair.value().isNull(),
                    "cold fixture identity mapping");
            id = pair.value();
        }
        expect("CurrentFixtureFontCacheStillCold", ambientMonitor.fontEvents == fontEventsBeforeFixture,
               "current source font-table events during fixture installation=" +
                   std::to_string(ambientMonitor.fontEvents - fontEventsBeforeFixture));
        const auto revision = context.revision();
        const auto beforeCurrentEvents = ambientMonitor.changes;
        const auto currentResourcesBefore = native_scene::readResources(*db);
        ambientMonitor.phase = "cold-current-memory-read";
        options.sourceSha256.clear();
        const auto current = readCurrentAnalysis(context, options);
        const auto handle = native_scene::handle(id);
        bool found = false;
        if (current.value)
            for (const auto &p : current.value->scene.primitives)
                if (p.key.handle == handle && p.text && *p.text == u8"未保存中文立面图")
                    found = true;
        expect("CurrentMemoryIncludesUnsavedChinese",
               found && current.value->scene.documentIdentity == context.documentIdentity() &&
                   context.revision() == revision,
               "actual current-memory text and unchanged revision");
        if (current.value)
            evidence->capability(analysis_test::details(*current.value));
        const auto currentResourcesAfter = native_scene::readResources(*db);
        bool currentResourcesSame = currentResourcesBefore.size() == currentResourcesAfter.size();
        for (std::size_t i = 0; currentResourcesSame && i < currentResourcesBefore.size(); ++i)
            currentResourcesSame =
                currentResourcesBefore[i].kind == currentResourcesAfter[i].kind &&
                currentResourcesBefore[i].name == currentResourcesAfter[i].name &&
                currentResourcesBefore[i].definitionSignature == currentResourcesAfter[i].definitionSignature;
        expect("ColdCurrentFontReadDoesNotModifySource",
               current.value && ambientMonitor.changes == beforeCurrentEvents &&
                   context.revision() == revision && currentResourcesSame,
               "unwarmed simhei.ttf text/style source; native events, resources and revision unchanged");
        analysis_test::synthetic(expect, *evidence);
        expect("ScratchReadsDoNotModifyAmbientDocument",
               ambientMonitor.changes == beforeCurrentEvents && context.revision() == revision,
               "all synthetic database reads keep actual current document unchanged");
        const auto globalsAfter = globals();
        expect("GlobalSettingsAndWorkingDatabaseUnchanged",
               globalsBefore == globalsAfter &&
                   workingBefore == acdbHostApplicationServices()->workingDatabase(),
               "11 actual native system variables and workingDatabase identity unchanged");
        evidence->capability(object({{"globalsBefore", globalsBefore}, {"globalsAfter", globalsAfter}}));
        evidence->capability(object({{"diagnostic", quote(std::string("ambient-current-events"))},
                                     {"beforeReads", std::to_string(beforeCurrentEvents)},
                                     {"afterReads", std::to_string(ambientMonitor.changes)},
                                     {"events", array(ambientMonitor.events)}}));
        require(sha256(config.scheme) == config.schemeSHA256, "source changed");
        require(sha256(config.reference) == config.referenceSHA256, "reference changed");
        evidence->checkpoint("analysis-verification-complete");
        evidence->finish(config, passed ? "passed" : "failed");
        acutPrintf(passed ? L"\nBCE_NATIVE_CASE_PASSED\n" : L"\nBCE_NATIVE_CASE_FAILED\n");
    } catch (const std::exception &failure) {
        try {
            if (evidence) {
                evidence->error(failure.what());
                evidence->finish(config, "failed");
            }
        } catch (...) {
        }
    } catch (...) {
        try {
            if (evidence) {
                evidence->error("unknown exception");
                evidence->finish(config, "failed");
            }
        } catch (...) {
        }
    }
}
} // namespace
BOOL APIENTRY DllMain(HMODULE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        moduleHandle = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
extern "C" __declspec(dllexport) AcRx::AppRetCode acrxEntryPoint(AcRx::AppMsgCode message, void *id) {
    try {
        if (message == AcRx::kInitAppMsg) {
            acrxDynamicLinker->unlockApplication(id);
            acrxRegisterAppMDIAware(id);
            return acedRegCmds->addCommand(L"BCE_ANALYSIS_CASES", L"BCE_TEST_RUN", L"BCE_TEST_RUN",
                                           ACRX_CMD_MODAL, run_case) == Acad::eOk
                       ? AcRx::kRetOK
                       : AcRx::kRetError;
        }
        if (message == AcRx::kUnloadAppMsg)
            return acedRegCmds->removeGroup(L"BCE_ANALYSIS_CASES") == Acad::eOk ? AcRx::kRetOK
                                                                                : AcRx::kRetError;
        return AcRx::kRetOK;
    } catch (...) {
        return AcRx::kRetError;
    }
}
