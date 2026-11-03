#include "../src/ApprovedArea.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
using namespace bcpproduct;
int main(){int tests=0;auto equal=[&](double a,double b){if(std::abs(a-b)>1e-8)throw std::runtime_error("area differs");++tests;};
 equal(polygonAreaM2({{0,0},{2920,0},{2920,1780},{0,1780}}),5.1976);
 equal(polygonAreaM2({{1e9,1e9},{1e9+2920,1e9},{1e9+2920,1e9+1780},{1e9,1e9+1780}}),5.1976);
 equal(polygonAreaM2({{0,1780},{2920,1780},{2920,0},{0,0}}),5.1976);
 equal(polygonAreaM2({{0,0},{4000,0},{4000,2000},{2000,2000},{2000,4000},{0,4000}}),12);
 equal(approvedBuildingArea({186.608},{{4600,1380}}),189.782);
 equal(approvedBuildingArea({153.4856},{}),153.4856);
 if(formatArea(5.1976)!="5.20m")throw std::runtime_error("rounding/units");++tests;
 try{polygonAreaM2({{0,0},{1,1}});return 1;}catch(const std::invalid_argument&){++tests;}
 try{polygonAreaM2({{0,0},{NAN,1},{1,0}});return 1;}catch(const std::invalid_argument&){++tests;}
 std::cout<<tests<<" approved area tests passed\n";
}
