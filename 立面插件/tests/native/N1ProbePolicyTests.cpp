#include "N1ProbePolicy.h"
#include <iostream>
#include <stdexcept>
#include <functional>

namespace {
unsigned failed{};
void expect(bool value,const char* name) { std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n'; if(!value)++failed; }
}
int main() {
    using namespace bce::native::policy;
    bool executionFailed=false; bool core=false;
    const std::function<void()> postSaveSourceRead=[]{throw std::runtime_error("post-save source read threw");};
    try { ExecutionGuard execution(executionFailed); core=true; postSaveSourceRead(); execution.commit(); }
    catch(const std::runtime_error&) {}
    expect(!assertions_allowed(core,true,executionFailed,false),"capabilities set then real throw cannot pass");
    expect(!observation_admissible(false,false,{}),"borrowed duplicate or unknown ownership cannot pass");
    expect(!observation_admissible(true,true,{}),"caught C++ getter exception cannot pass");
    expect(!observation_admissible(true,false,{"textString: 53"}),"unexpected getter status cannot pass");
    const std::vector<std::string> limits{"bounds: 30","explode: 3","dimension actual Text/MText glyphs unavailable through public read paths"};
    expect(observation_admissible(true,false,limits),"known glyph extents and explode limits allow observed partial");
    expect(assertions_allowed(true,true,false,!observation_admissible(true,false,limits)),"known limits do not imply a hard failure");
    expect(empty_public_placeholder(L"TDbMText",false,0,true,0,0),"zero empty custom text representation is explicitly unobservable");
    expect(!empty_public_placeholder(L"AcDbText",true,0,true,0,0),"real Text at origin is not discarded");
    expect(!empty_public_placeholder(L"TDbMText",false,0,true,0,1),"nonempty custom representation is not discarded");
    expect(!empty_public_placeholder(L"TDbMText",false,0,false,0,0),"nonzero custom bounds remain observed geometry");
    for(unsigned reversed=0;reversed<4;++reversed) {
        Segment a{{Point{0,0,0},Point{10,0,0}}},b{{Point{10,0,0},Point{20,0,0}}};
        if(reversed&1)std::swap(a.endpoints[0],a.endpoints[1]);
        if(reversed&2)std::swap(b.endpoints[0],b.endpoints[1]);
        const auto match=find_shared({a,b},{{0,0,0},{10,0,0},{20,0,0}});
        expect(match.found && match.grip==1 && points_near(match.direction,{1,0,0}),"four endpoint orientation combinations match actual shared grip");
    }
    const Point shared{67462.410032101005,94855.805481089628,0};
    const auto real=find_shared({Segment{{shared,Point{67462.410032101005,95255.771873496938,0}}},
        Segment{{Point{67462.410032101005,92455.771873497288,0},shared}}},
        {{66949.641815103038,95255.771873496938,0},shared});
    expect(real.found && real.grip==1 && points_near(real.direction,{0,-1,0}),"actual runtime reversed vertical definitions match outward +40 direction");
    expect(!find_shared({Segment{{Point{0,0,0},Point{10,0,0}}},Segment{{Point{10,0,0},Point{20,0,0}}}},{{0,0,0},{20,0,0}}).found,
        "shared definition point without actual grip is rejected");
    // Exercise the same read -> checked observation boundary -> caller continuation used by snapshot().
    // Only the baseline-moved read is damaged; a later restored/source read remains healthy.
    struct CapturedObservation {bool hardFailure{};bool complete{true};};
    for(unsigned damagedRead=0;damagedRead<4;++damagedRead) {
        bool chainFailed=false,baselineVerified=false,saveReopen=false;
        unsigned reads=0;bool rejected=false;
        const auto read=[&] {const auto index=reads++;return CapturedObservation{index==damagedRead,false};};
        try {
            ExecutionGuard chain(chainFailed);
            checked_read(read); // baseline before
            checked_read(read); // only moved-baseline snapshot may be hard
            baselineVerified=true;
            checked_read(read); // baseline restored
            checked_read(read); // independent saved readback
            saveReopen=true;chain.commit();
        } catch(const std::runtime_error&) {rejected=true;}
        const auto finalHealthySource=checked_read([] {return CapturedObservation{false,true};});
        expect(rejected && chainFailed && !saveReopen && !finalHealthySource.hardFailure,
            "each snapshot position rejects a hard observation before continuation");
        expect(!assertions_allowed(baselineVerified && saveReopen,true,chainFailed,false),
            "healthy final source cannot erase an intermediate snapshot failure");
    }
    const auto limited=checked_read([] {return CapturedObservation{false,false};});
    expect(!limited.hardFailure && !limited.complete,"checked snapshot retains known incomplete glyph observation for partial");
    std::cout<<"FAILED="<<failed<<'\n'; return failed?1:0;
}

