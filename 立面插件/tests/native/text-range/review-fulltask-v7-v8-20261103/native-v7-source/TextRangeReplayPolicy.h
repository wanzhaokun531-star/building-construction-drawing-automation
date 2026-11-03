#pragma once
#include <cstdint>
namespace bce::text_range {
// Value-only observations. Policy controls copy this object, never live Session.
struct ReplayFacts {
    bool identityMatches{};
    std::uint32_t pid{},expectedPid{},thread{},expectedThread{};
    bool prepared{},pendingEvidence{},documentHook{},commandHook{},idleHook{},errorEmpty{};
    bool openStarted{},openEnded{},queued{},capturing{},finished{},hasTarget{},hasMarker{};
    unsigned callbacks{},outputCount{};
    std::uintptr_t currentDocument{},activeDocument{},prepareDocument{},currentDatabase{},workingDatabase{},ambientDatabase{};
    bool prepareDocumentDestroyed{},ambientPathMatches{},readyHashMatches{},targetHashMatches{},targetBytesMatch{};
};
inline const char* replayRejection(const ReplayFacts& f) noexcept {
    if(!f.identityMatches)return "identity";
    if(f.pid!=f.expectedPid||f.thread!=f.expectedThread)return "execution";
    if(!f.prepared||!f.pendingEvidence||!f.documentHook||!f.commandHook||!f.idleHook||!f.errorEmpty||f.openStarted||f.openEnded||f.queued||f.capturing||f.finished||f.hasTarget||f.hasMarker||f.callbacks||f.outputCount)return "phase";
    if(!f.prepareDocument||!f.ambientDatabase||f.prepareDocumentDestroyed||f.currentDocument!=f.prepareDocument||f.activeDocument!=f.prepareDocument||f.currentDatabase!=f.ambientDatabase||f.workingDatabase!=f.ambientDatabase||!f.ambientPathMatches)return "ambient";
    if(!f.readyHashMatches||!f.targetHashMatches||!f.targetBytesMatch)return "artifacts";
    return nullptr;
}
}
