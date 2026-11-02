#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

namespace bce::native::policy {
// Observation boundary shared by every native snapshot and the native-free read-chain test.
template<class Read> auto checked_read(Read&& read) {
    auto observation=std::forward<Read>(read)();
    if(observation.hardFailure)throw std::runtime_error("hard snapshot rejected at unified observation boundary");
    return observation;
}
struct Point { double x{}, y{}, z{}; };
inline Point subtract(Point a, Point b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
inline double dot(Point a, Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline double length(Point value) { return std::sqrt(dot(value,value)); }
inline bool points_near(Point a, Point b) { return length(subtract(a,b)) <= 1e-6; }
struct Segment { std::array<Point,2> endpoints; };
struct SharedMatch { bool found{}; std::size_t first{},second{},grip{},firstEndpoint{},secondEndpoint{}; Point direction; };

inline SharedMatch find_shared(const std::vector<Segment>& segments, const std::vector<Point>& grips) {
    for(std::size_t a=0;a<segments.size();++a) for(std::size_t b=a+1;b<segments.size();++b)
        for(std::size_t ea=0;ea<2;++ea) for(std::size_t eb=0;eb<2;++eb) {
            const auto shared=segments[a].endpoints[ea];
            if(!points_near(shared,segments[b].endpoints[eb]))continue;
            const auto outward=subtract(shared,segments[a].endpoints[1-ea]); const auto size=length(outward);
            const auto otherOutward=subtract(shared,segments[b].endpoints[1-eb]);const auto otherSize=length(otherOutward);
            if(size<=1e-6 || otherSize<=1e-6)continue;
            const Point direction{outward.x/size,outward.y/size,outward.z/size};
            if(std::abs(dot(direction,otherOutward)/otherSize+1)>1e-9)continue;
            for(std::size_t i=0;i<grips.size();++i)if(points_near(grips[i],shared))return {true,a,b,i,ea,eb,direction};
        }
    return {};
}
inline bool known_limit(const std::string& message) {
    return message=="bounds: 30" || message=="explode: 3" ||
        message=="dimension actual Text/MText glyphs unavailable through public read paths" ||
        message=="unknown leaf or empty public compound representation" ||
        message=="known empty public TDbMText placeholder; actual geometry unavailable";
}
inline bool observation_admissible(bool ownershipSafe, bool unexpectedException, const std::vector<std::string>& errors) {
    if(!ownershipSafe || unexpectedException)return false;
    for(const auto& message:errors)if(!known_limit(message))return false;
    return true;
}
inline bool assertions_allowed(bool core, bool grips, bool executionFailed, bool hardObservationFailure) {
    return core && grips && !executionFailed && !hardObservationFailure;
}
inline bool empty_public_placeholder(const std::wstring& type, bool standardTextCast, int boundsStatus,
    bool zeroBounds, int explodeStatus, std::size_t returned) {
    return type==L"TDbMText" && !standardTextCast && boundsStatus==0 && zeroBounds && explodeStatus==0 && returned==0;
}
class ExecutionGuard final {
public:
    explicit ExecutionGuard(bool& failed) noexcept : failed_(failed) {}
    ~ExecutionGuard() noexcept {if(!committed_)failed_=true;}
    ExecutionGuard(const ExecutionGuard&)=delete;
    ExecutionGuard& operator=(const ExecutionGuard&)=delete;
    void commit() noexcept {committed_=true;}
private:
    bool& failed_;
    bool committed_{};
};
} // namespace bce::native::policy

