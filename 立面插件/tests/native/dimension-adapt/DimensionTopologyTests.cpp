#include "DimensionTopology.h"
#include <iostream>
#include <limits>

template<class F> void rejects(F f) { bool rejected=false;try {f();} catch(const std::runtime_error&) {rejected=true;} n8a::ensure(rejected,"expected invalid topology rejection"); }
int main() {
    try {
        using namespace n8a;
        // A shuffled chain whose individual definition directions disagree.
        const auto actual=path({{{10,0,0},{4,0,0}},{{0,0,0},{4,0,0}}},{1,0,0});
        ensure(actual.nodes.size()==3&&actual.nodes[0].x==0&&actual.nodes[1].x==4&&actual.nodes[2].x==10,"path must follow geometry not explode order/direction");
        ensure(actual.segments==std::vector<std::size_t>({1,0}),"ordered segment identity");
        ensure(unique_grip({4,0,0},{{99,3,0},{10,0,0},{4,0,0},{0,0,0}})==2,"grip index must derive from actual geometry");
        ensure(move_order({0,4,10,15,20},{-1,6,8,18,22})==std::vector<std::size_t>({0,2,4,3,1}),"negative left-to-right then positive right-to-left");
        rejects([]{path({{{0,0,0},{4,0,0}},{{4,0,0},{8,0,0}},{{4,0,0},{6,0,0}}},{1,0,0});});
        rejects([]{path({{{0,0,0},{4,0,0}},{{0,0,0},{4,0,0}}},{1,0,0});});
        rejects([]{unique_grip({4,0,0},{{4,0,0},{4,0,0}});});
        rejects([]{unique_grip({4,0,0},{{0,0,0}});});
        rejects([]{move_order({0,1,2},{0,1,1});});
        rejects([]{move_order({0,1,2},{0,2,1});});
        rejects([]{move_order({0,1,2},{0,std::numeric_limits<double>::infinity(),3});});
        const auto vertical=path({{{2,9,0},{2,4,0}}},{0,1,0});ensure(vertical.nodes.front().y==4,"vertical direction");
        std::cout<<"N8A_TOPOLOGY_TESTS_PASSED\n";return 0;
    } catch(const std::exception& e) {std::cerr<<"N8A_TOPOLOGY_TESTS_FAILED: "<<e.what()<<'\n';return 1;}
}
