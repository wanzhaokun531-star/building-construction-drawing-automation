#include "TextRangeReplayPolicy.h"
#include <cstring>
#include <iostream>
int main(){
    using namespace bce::text_range;
    ReplayFacts healthy;
    healthy.identityMatches=true;healthy.pid=healthy.expectedPid=12;healthy.thread=healthy.expectedThread=34;
    healthy.prepared=healthy.pendingEvidence=healthy.documentHook=healthy.commandHook=healthy.idleHook=healthy.errorEmpty=true;
    healthy.currentDocument=healthy.activeDocument=healthy.prepareDocument=100;
    healthy.currentDatabase=healthy.workingDatabase=healthy.ambientDatabase=200;
    healthy.ambientPathMatches=healthy.readyHashMatches=healthy.targetHashMatches=healthy.targetBytesMatch=true;
    unsigned passed{};
    auto check=[&](const ReplayFacts& facts,const char* expected){const auto* actual=replayRejection(facts);const bool same=expected?(actual&&std::strcmp(actual,expected)==0):!actual;if(same)++passed;else std::cerr<<"unexpected replay decision\n";return same;};
    bool all=check(healthy,nullptr);auto f=healthy;f.identityMatches=false;all=check(f,"identity")&&all;
    f=healthy;++f.thread;all=check(f,"execution")&&all;
    f=healthy;f.openStarted=true;all=check(f,"phase")&&all;
    f=healthy;f.finished=true;f.pendingEvidence=false;all=check(f,"phase")&&all;
    f=healthy;f.queued=true;all=check(f,"phase")&&all;
    f=healthy;++f.currentDocument;all=check(f,"ambient")&&all;
    f=healthy;f.prepareDocumentDestroyed=true;all=check(f,"ambient")&&all;
    f=healthy;f.targetHashMatches=false;all=check(f,"artifacts")&&all;
    f=healthy;f.targetBytesMatch=false;all=check(f,"artifacts")&&all;
    std::cout<<passed<<" value-only replay policy controls passed; no live session mutation or command replay\n";
    return all&&passed==10?0:1;
}
