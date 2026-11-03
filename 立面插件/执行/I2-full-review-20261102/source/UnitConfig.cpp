#include "UnitConfig.h"
#include <Windows.h>
#include <wincrypt.h>
#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace bce {
namespace {
Result<StoreReceipt> rejected(const char* code, const std::string& message) {
    return {std::nullopt, {{code, message, {}, true}}};
}
// Cryptographic random UUIDs identify a project and each explicit confirmation;
// no filename, fixture hash, handle or geometric identity is used as a project ID.
std::string randomId() {
    HCRYPTPROV provider{};
    if (!CryptAcquireContextW(&provider, nullptr, nullptr, PROV_RSA_AES,
                              CRYPT_VERIFYCONTEXT))
        throw std::runtime_error("project identity entropy provider unavailable");
    struct ProviderScope {
        HCRYPTPROV value;
        ~ProviderScope() noexcept { CryptReleaseContext(value, 0); }
    } scope{provider};
    std::array<BYTE, 16> bytes{};
    if (!CryptGenRandom(provider, static_cast<DWORD>(bytes.size()), bytes.data()))
        throw std::runtime_error("project identity entropy generation failed");
    bytes[6] = static_cast<BYTE>((bytes[6] & 0x0F) | 0x40);
    bytes[8] = static_cast<BYTE>((bytes[8] & 0x3F) | 0x80);
    std::ostringstream out;
    out.imbue(std::locale::classic());
    for (std::size_t n = 0; n < bytes.size(); ++n) {
        if (n == 4 || n == 6 || n == 8 || n == 10) out << '-';
        out << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(bytes[n]);
    }
    return out.str();
}
}
bool isUnitOnlyProject(const StoredProject& p) noexcept {
    const auto& zero = p.config.zeroLevel;
    // Every geometry-bearing collection and provenance field is enumerated.
    // Even empty generation metadata is kept out of this deliberately narrow route.
    if (zero.confirmed || !zero.value.empty() || !zero.source.empty() ||
        !p.config.views.empty() || !p.rules.views.empty() ||
        !p.rules.semanticFeatures.empty() || !p.rules.materialOverrides.empty() ||
        !p.ruleInputsConfirmationSource.empty() || !p.confirmedEntities.empty() ||
        !p.generation.views.empty() || !p.generation.documentIdentity.empty() ||
        !p.generation.databaseIdentity.empty()) return false;
    const auto scale = p.config.units.confirmedMmPerInputUnit;
    return !scale || (std::isfinite(*scale) && *scale > 0 &&
                      !p.config.unitConfirmationSource.empty());
}
Result<StoreReceipt> configureProjectUnits(NativeSceneContext& context, double scale) {
    try {
        if (!std::isfinite(scale) || scale <= 0)
            return rejected("UNIT_CONFIG_INVALID", u8"单位比例必须是有限正数；未修改档案。");
        auto* db = context.currentDatabase();
        if (!db) return rejected(diagnostic::staleDocument, u8"当前文档上下文已失效。");
        const auto identity = context.documentIdentity();
        const auto revision = context.revision();
        const auto existing = readProjectStore(*db);
        ProjectArchive archive;
        if (existing.readable()) {
            if (!existing.issues.empty() || !isUnitOnlyProject(existing.value->project))
                return rejected("PROJECT_CONFIG_REQUIRES_FULL_BINDING",
                    u8"档案含零点、视图、规则或生成历史；本单位首次配置入口拒绝改写，原档案保留。");
            archive = *existing.value;
        } else {
            if (existing.issues.size() != 1 || existing.issues[0].code != "PROJECT_NOT_FOUND")
                return {std::nullopt, existing.issues};
            archive.project.persistentProjectId = randomId();
        }
        // This binding authorizes exactly the non-geometric unit write. It is
        // not a bindProjectArchive result or an ObservedScene qualification.
        archive.project.config.databaseIdentity = identity + "/database";
        archive.project.config.units.confirmedMmPerInputUnit = scale;
        archive.project.config.units.trustedInsunits = false;
        archive.project.config.units.insunits = 0;
        std::ostringstream source;
        source.imbue(std::locale::classic());
        source << "BCE_CONFIG:user-explicit; mmPerInputUnit="
               << std::setprecision(std::numeric_limits<double>::max_digits10) << scale
               << "; confirmation=" << randomId();
        archive.project.config.unitConfirmationSource = source.str();
        if (!isUnitOnlyProject(archive.project))
            return rejected("PROJECT_CONFIG_REQUIRES_FULL_BINDING", "unit-only contract changed");
        archive.bound = true;
        return writeProjectStore(context, {identity, revision, std::move(archive)});
    } catch (const std::exception& error) {
        return rejected("UNIT_CONFIG_FAILED", error.what());
    } catch (...) {
        return rejected("UNIT_CONFIG_FAILED", "unknown unit configuration failure");
    }
}
}
