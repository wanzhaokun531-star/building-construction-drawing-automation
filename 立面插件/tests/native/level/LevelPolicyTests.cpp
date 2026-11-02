#include "LevelPolicy.h"
#include "N1ProbePolicy.h"
#include <iostream>
#include <stdexcept>
struct Observation { bool hardFailure{}; bool complete{}; };
int main() {
    int checks=0;int failed=0;
    const auto expect=[&](bool value,const char* name){++checks;if(!value){++failed;std::cerr << "FAIL " << name << '\n';}};
    using namespace bce::native;
    double value{};
    expect(level_policy::parse_numeric("2.800",value)&&std::abs(value-2.8)<1e-12,"actual numeric parse");
    expect(level_policy::parse_numeric("2.923",value)&&std::abs(value-2.923)<1e-12,"numeric delta uses observed text");
    for(const auto* bad:{"","missing","2.800m"," 2.800","2.800 ","NaN","inf"})expect(!level_policy::parse_numeric(bad,value),"missing/non-real numeric rejected");
    expect(level_policy::eligible(7,7,true,true,false),"all branches");
    expect(!level_policy::eligible(6,7,true,true,false),"incomplete branches rejected");
    expect(!level_policy::eligible(7,7,false,true,false),"missing actual numeric rejected");
    expect(!level_policy::eligible(7,7,true,false,false),"source SHA change rejected");
    expect(!level_policy::eligible(7,7,true,true,true),"hard failure rejected");
    bool failedExecution=false;
    try {policy::ExecutionGuard guard(failedExecution);policy::checked_read([]{return Observation{true,false};});guard.commit();}catch(const std::runtime_error&){}
    policy::checked_read([]{return Observation{false,true};});
    expect(failedExecution,"healthy final snapshot cannot clear hard intermediate failure");
    expect(!policy::checked_read([]{return Observation{false,false};}).complete,"known incomplete remains partial");
    expect(!policy::observation_admissible(false,false,{}),"ownership rejected");
    expect(!policy::observation_admissible(true,true,{}),"getter exception rejected");
    expect(!policy::observation_admissible(true,false,{"bounds: 77"}),"unexpected getter status rejected");
    expect(policy::observation_admissible(true,false,{"known empty public TDbMText placeholder; actual geometry unavailable"}),"known missing representation stays observable partial");
    std::cout << "CHECKS=" << checks << " FAILED=" << failed << '\n';return failed?1:0;
}
