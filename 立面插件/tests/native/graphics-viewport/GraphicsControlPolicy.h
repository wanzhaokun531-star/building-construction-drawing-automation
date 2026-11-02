#pragma once
#include "GraphicsPolicy.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <string>
#include <vector>
namespace bce::graphics {
using Triple = std::array<double,3>;
struct TextObservation {
    std::wstring text, font, bigfont;
    Triple position{}, xAxis{}, yAxis{};
    double height{}, width{}, oblique{}, tracking{};
    bool styleComplete{}, basisValid{};
};
struct PrimitiveObservation {
    std::string kind;
    std::vector<Triple> points;
    Triple center{}, normal{}, startVector{};
    double radius{}, sweep{};
};
struct ControlObservation {
    Coverage coverage;
    std::vector<TextObservation> texts;
    std::vector<PrimitiveObservation> primitives;
};
struct ControlValidation {
    bool valuesMatch{}, complete{};
    std::vector<std::string> reasons;
};
inline ControlValidation validateControl(std::size_t index,const ControlObservation& value) {
    ControlValidation result{true,false,{}};
    auto check=[&](bool ok,const char* reason){if(!ok){result.valuesMatch=false;result.reasons.push_back(reason);}};
    auto nearValue=[](double a,double expected){return std::isfinite(a)&&std::abs(a-expected)<=1e-8;};
    auto triple=[&](const Triple& a,const Triple& expected){return nearValue(a[0],expected[0])&&nearValue(a[1],expected[1])&&nearValue(a[2],expected[2]);};
    auto direction=[&](const Triple& a,const Triple& expected){const double length=std::hypot(std::hypot(a[0],a[1]),a[2]);
        return std::isfinite(length)&&length>1e-12&&triple({a[0]/length,a[1]/length,a[2]/length},expected);};
    auto font=[](std::wstring name){const auto slash=name.find_last_of(L"/\\");if(slash!=std::wstring::npos)name.erase(0,slash+1);
        std::transform(name.begin(),name.end(),name.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
        if(name.size()>4&&name.substr(name.size()-4)==L".shx")name.resize(name.size()-4);return name;};
    auto style=[&](const TextObservation& t){return t.styleComplete&&t.basisValid&&font(t.font)==L"simplex"&&font(t.bigfont)==L"gbcbig"&&nearValue(t.oblique,0)&&nearValue(t.tracking,1);};
    auto single=[&](const wchar_t* content,const Triple& pos,const Triple& x,const Triple& y){
        check(value.texts.size()==1,"expected exactly one actual Text request");if(value.texts.size()!=1)return;
        const auto& t=value.texts.front();check(t.text==content,"actual Text content mismatch");check(style(t),"actual Text style/font/basis mismatch");
        check(triple(t.position,pos),"actual Text WCS insertion mismatch");check(triple(t.xAxis,x),"actual Text effective X axis/width/handedness mismatch");check(triple(t.yAxis,y),"actual Text effective Y axis/height/handedness mismatch");
    };
    if(index==0){
        std::vector<const PrimitiveObservation*> lines;for(const auto& p:value.primitives)if(p.kind=="polyline"||p.kind=="AcGiPolyline")lines.push_back(&p);
        check(lines.size()==1,"expected one actual Line polyline");if(lines.size()==1)check(lines[0]->points.size()==2&&triple(lines[0]->points[0],{1,2,0})&&triple(lines[0]->points[1],{4,6,0}),"actual Line WCS endpoints mismatch");
    }else if(index==1){
        std::vector<const PrimitiveObservation*> arcs;for(const auto& p:value.primitives)if(p.kind=="circularArc")arcs.push_back(&p);
        // AcGi defines startVector by direction, not unit magnitude. The actual
        // 2020 AcDbArc callback uses (5,0,0) for this R5 fixture.
        check(arcs.size()==1,"expected one actual Arc request");if(arcs.size()==1){const auto& p=*arcs[0];check(triple(p.center,{10,10,0})&&direction(p.normal,{0,0,1})&&direction(p.startVector,{1,0,0})&&nearValue(p.radius,5)&&nearValue(p.sweep,1.5707963267948966),"actual Arc center/radius/orientation/sweep mismatch");}
    }else if(index==2){
        single(L"123.45",{0,0,0},{2.25,0,0},{0,3,0});
        if(value.texts.size()==1)check(nearValue(value.texts[0].height,3)&&nearValue(value.texts[0].width,0.75),"numeric Text actual height/width mismatch");
    }else if(index==3)single(L"\x4E2D\x6587\x7ACB\x9762",{0,5,0},{3,0,0},{0,3,0});
    else if(index==4)single(L"270",{10,5,0},{0,-3,0},{-3,0,0});
    else if(index==6){
        // Hand calculation: outer +(30,40), inner +(10,20), R(0.5)*diag(2,1), local text (1,2).
        // Fixed values are independent of collector matrix operations.
        single(L"nested",{40.79631404657234,62.71401620098915,0},{3.510330247561491,1.917702154416812,0},{-0.958851077208406,1.755165123780746,0});
        check(value.coverage.drawsEntered>=3,"real nested block traversal not observed");
    }else if(index==5){
        check(value.texts.size()==4,"formatted MText expected AB, half-height 2, numerator 7, denominator 9 runs");
        const TextObservation *ab{},*half{},*numerator{},*denominator{};
        for(const auto& t:value.texts){check(style(t),"formatted MText actual style/font/basis mismatch");
            if(t.text==L"AB"&&!ab)ab=&t;else if(t.text==L"2"&&!half)half=&t;
            else if(t.text==L"7"&&!numerator)numerator=&t;else if(t.text==L"9"&&!denominator)denominator=&t;
            else check(false,"formatted MText unexpected/duplicate actual run");}
        check(ab&&half&&numerator&&denominator,"formatted MText required actual runs missing");
        if(ab&&half&&numerator&&denominator){
            check(nearValue(ab->height,3)&&triple(ab->xAxis,{3,0,0})&&triple(ab->yAxis,{0,3,0}),"MText base run height/direction mismatch");
            check(nearValue(half->height,1.5)&&triple(half->xAxis,{1.5,0,0})&&triple(half->yAxis,{0,1.5,0})&&half->position[1]<ab->position[1],"MText paragraph/half-height formatting mismatch");
            check(numerator->height>0&&numerator->height<=3&&nearValue(numerator->height,denominator->height)&&numerator->position[1]>denominator->position[1],"MText fraction height/order mismatch");
        }
    }else check(false,"unknown standard control index");
    result.complete=result.valuesMatch&&value.coverage.requestComplete();
    if(!value.coverage.requestComplete())result.reasons.push_back("callback coverage incomplete; control cannot authorize capability upgrade");
    if(index==5){result.complete=false;result.reasons.push_back("formatted MText full layout not independently rendered; control remains partial");}
    return result;
}
inline bool requiresWorldDraw(bool viewIndependentViewport) {
    return !viewIndependentViewport;
}
}
