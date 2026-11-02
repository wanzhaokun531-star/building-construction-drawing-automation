#include "GraphicsControlPolicy.h"
#include <iostream>
using namespace bce::graphics;
ControlObservation valid(std::size_t index) {
    ControlObservation o;o.coverage.callbacks=3;o.coverage.drawRequests=o.coverage.drawsEntered=1;
    if(index==0){PrimitiveObservation p;p.kind="polyline";p.points={{{1,2,0}},{{4,6,0}}};o.primitives.push_back(p);}
    if(index==1){PrimitiveObservation p;p.kind="circularArc";p.center={10,10,0};p.normal={0,0,1};p.startVector={1,0,0};p.radius=5;p.sweep=1.5707963267948966;o.primitives.push_back(p);}
    auto text=[&](const wchar_t* value,Triple position,Triple x,Triple y,double height,double width){
        TextObservation t;t.text=value;t.position=position;t.xAxis=x;t.yAxis=y;t.height=height;t.width=width;
        t.font=L"simplex.shx";t.bigfont=L"gbcbig.shx";t.tracking=1;t.styleComplete=t.basisValid=true;o.texts.push_back(t);
    };
    if(index==2)text(L"123.45",{0,0,0},{2.25,0,0},{0,3,0},3,0.75);
    if(index==3)text(L"\x4E2D\x6587\x7ACB\x9762",{0,5,0},{3,0,0},{0,3,0},3,1);
    if(index==4)text(L"270",{10,5,0},{0,-3,0},{-3,0,0},3,1);
    if(index==5){text(L"AB",{0,0,0},{3,0,0},{0,3,0},3,1);text(L"2",{0,-5,0},{1.5,0,0},{0,1.5,0},1.5,1);
        text(L"7",{3,-4,0},{2.1,0,0},{0,2.1,0},2.1,1);text(L"9",{3,-6,0},{2.1,0,0},{0,2.1,0},2.1,1);}
    if(index==6){o.coverage.drawRequests=o.coverage.drawsEntered=3;
        text(L"nested",{40.79631404657234,62.71401620098915,0},{3.510330247561491,1.917702154416812,0},{-0.958851077208406,1.755165123780746,0},2,1);}
    return o;
}
int main(){
    int checks=0,failures=0;
    auto expect=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cout<<"FAIL "<<name<<'\n';}};
    expect(!requiresWorldDraw(true),"viewport-only attribute skips worldDraw");
    expect(requiresWorldDraw(false),"ordinary drawable invokes worldDraw");
    for(std::size_t i=0;i<7;++i){auto o=valid(i);auto v=validateControl(i,o);expect(v.valuesMatch,"known fixture values accepted");
        expect(v.complete==(i!=5),"formatted MText remains partial until layout verification");
        o.texts.clear();o.primitives.clear();v=validateControl(i,o);expect(!v.valuesMatch&&!v.complete,"missing real control geometry/text rejected");}
    auto wrong=[&](std::size_t i,ControlObservation o,const char* name){const auto v=validateControl(i,o);expect(!v.valuesMatch&&!v.complete,name);};
    auto o=valid(0);o.primitives[0].points[1][0]=400;wrong(0,o,"line coordinate mismatch");
    o=valid(1);o.primitives[0].sweep=-1.5707963267948966;wrong(1,o,"arc orientation mismatch");
    for(std::size_t i:{2u,3u,4u,6u}){
        o=valid(i);o.texts[0].text=L"incorrect";wrong(i,o,"actual text content mismatch");
        o=valid(i);o.texts[0].font=L"txt.shx";wrong(i,o,"actual font mismatch");
        o=valid(i);o.texts[0].bigfont=L"wrong.shx";wrong(i,o,"actual bigfont mismatch");
        o=valid(i);o.texts[0].xAxis[0]+=1;wrong(i,o,"effective width/direction mismatch");
        o=valid(i);o.texts[0].yAxis[1]+=1;wrong(i,o,"effective height/handedness mismatch");
        o=valid(i);o.texts[0].position[0]+=1;wrong(i,o,"actual position mismatch");
        o=valid(i);o.texts[0].basisValid=false;wrong(i,o,"unresolved basis rejected");
        o=valid(i);o.coverage.viewportRequired=1;expect(!validateControl(i,o).complete,"viewport cannot upgrade control");
    }
    o=valid(4);o.texts[0].xAxis={0,3,0};wrong(4,o,"mirror ignored");
    o=valid(6);o.texts[0].position={1,2,0};wrong(6,o,"nested matrix replaced with identity");
    o=valid(6);o.coverage.drawsEntered=1;expect(!validateControl(6,o).complete,"missing nested recursion rejects complete");
    o=valid(5);o.texts.pop_back();wrong(5,o,"missing MText denominator");
    o=valid(5);o.texts[1].height=3;o.texts[1].xAxis={3,0,0};o.texts[1].yAxis={0,3,0};wrong(5,o,"MText inline height ignored");
    o=valid(5);o.texts[2].position[1]=-7;wrong(5,o,"MText stacked numerator inverted");
    o=valid(1);o.primitives[0].startVector={5,0,0};expect(validateControl(1,o).complete,"real native radius-length Arc start vector accepted");
    o=valid(1);o.primitives[0].startVector={10,0,0};expect(validateControl(1,o).complete,"Arc start vector direction is scale independent");
    o=valid(1);o.primitives[0].startVector={0,0,0};wrong(1,o,"zero Arc start vector rejected");
    o=valid(1);o.primitives[0].startVector={-5,0,0};wrong(1,o,"opposite Arc start direction rejected");
    o=valid(1);o.primitives[0].startVector={5,1,0};wrong(1,o,"incorrect Arc start direction rejected");
    std::cout<<"CONTROL_CHECKS="<<checks<<" FAILED="<<failures<<'\n';return failures?1:0;
}
