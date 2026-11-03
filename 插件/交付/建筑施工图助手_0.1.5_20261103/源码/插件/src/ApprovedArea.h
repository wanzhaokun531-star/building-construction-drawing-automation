#pragma once
#include <vector>
#include <utility>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <locale>
namespace bcpproduct {
using AreaVertex=std::pair<double,double>;
inline double polygonAreaM2(const std::vector<AreaVertex>& p){
 if(p.size()<3)throw std::invalid_argument("area boundary needs at least three vertices");
 for(const auto& v:p)if(!std::isfinite(v.first)||!std::isfinite(v.second))throw std::invalid_argument("non-finite area vertex");
 const auto origin=p.front();long double twice=0;
 for(std::size_t i=0;i<p.size();++i){const auto& a=p[i];const auto& b=p[(i+1)%p.size()];twice+=(static_cast<long double>(a.first)-origin.first)*(static_cast<long double>(b.second)-origin.second)-(static_cast<long double>(a.second)-origin.second)*(static_cast<long double>(b.first)-origin.first);}
 const auto area=static_cast<double>(std::abs(twice)/2000000.0L);if(!std::isfinite(area)||area<=0)throw std::invalid_argument("empty area boundary");return area;
}
inline double approvedBuildingArea(const std::vector<double>& external,const std::vector<AreaVertex>& porches){
 double value=0;for(auto a:external){if(!std::isfinite(a)||a<=0)throw std::invalid_argument("invalid exterior area");value+=a;}
 for(const auto& p:porches){if(!std::isfinite(p.first)||!std::isfinite(p.second)||p.first<=0||p.second<=0)throw std::invalid_argument("invalid porch platform");value+=p.first*p.second/2000000.0;}return value;
}
inline std::string formatArea(double value){if(!std::isfinite(value)||value<0)throw std::invalid_argument("invalid area");std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(2)<<value<<'m';return out.str();}
}
