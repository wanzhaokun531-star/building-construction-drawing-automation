#include "../../src/TextOccupancy.h"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
std::size_t size(const char* s) {
    const std::string text(s); if(text.empty() || text.find_first_not_of("0123456789")!=std::string::npos) throw std::runtime_error("unsigned integer required");
    std::size_t used=0;const auto n=std::stoull(text,&used);
    if(used!=text.size() || n>std::numeric_limits<std::size_t>::max())throw std::runtime_error("size overflow");
    return static_cast<std::size_t>(n);
}
double number(const char* s) {
    std::size_t used=0;const std::string text(s);const auto n=std::stod(text,&used);
    if(used!=text.size())throw std::runtime_error("invalid number");return n;
}
void point(bce::Point2 p){std::cout<<'['<<p.x<<','<<p.y<<']';}
void bounds(bce::Bounds2 b){std::cout<<'[';point(b.minimum);std::cout<<',';point(b.maximum);std::cout<<']';}
void quoted(const std::string& s) {
    std::cout<<'"';for(const unsigned char ch:s){if(ch=='"'||ch=='\\')std::cout<<'\\'<<ch;
        else if(ch<32)std::cout<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(ch)<<std::dec;
        else std::cout<<ch;}std::cout<<'"';
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=18)throw std::runtime_error("rgb width height stride a b tx c d ty dpi device frame font source phaseX phaseY required");
        std::ifstream input(argv[1],std::ios::binary);if(!input)throw std::runtime_error("RGB open failed");
        const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());
        const bce::TextAffine2 m={number(argv[5]),number(argv[6]),number(argv[7]),number(argv[8]),number(argv[9]),number(argv[10])};
        const bce::TextRenderContext c={argv[12],argv[13],argv[14],argv[15],number(argv[11]),number(argv[11]),{number(argv[16]),number(argv[17])}};
        const auto r=bce::measureTextFootprint({bytes.data(),bytes.size(),size(argv[2]),size(argv[3]),size(argv[4])},m,c);
        if(!r.canCommit()){std::cerr<<r.issues[0].code<<": "<<r.issues[0].message<<'\n';return 2;}
        const auto& f=*r.value; std::cout<<std::setprecision(17);
        std::cout<<"{\"kind\":\"observed device footprint\",\"inkPixels\":"<<f.inkPixels<<",\"deviceCellBounds\":";bounds(f.deviceCellBounds);
        std::cout<<",\"worldCorners\":[";for(std::size_t k=0;k<4;++k){if(k)std::cout<<',';point(f.world.corners[k]);}
        std::cout<<"],\"worldBounds\":";bounds(f.world.bounds);
        std::cout<<",\"worldToPixel\":["<<f.worldToPixel.a<<','<<f.worldToPixel.b<<','<<f.worldToPixel.tx<<','<<f.worldToPixel.c<<','<<f.worldToPixel.d<<','<<f.worldToPixel.ty<<']';
        std::cout<<",\"context\":{\"deviceIdentity\":";quoted(f.context.deviceIdentity);std::cout<<",\"frameIdentity\":";quoted(f.context.frameIdentity);
        std::cout<<",\"fontIdentity\":";quoted(f.context.fontIdentity);std::cout<<",\"sourceIdentity\":";quoted(f.context.sourceIdentity);
        std::cout<<",\"dpi\":["<<f.context.dpiX<<','<<f.context.dpiY<<"],\"pixelBoundaryPhase\":";point(f.context.pixelBoundaryPhase);std::cout<<"}}\n";
        return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
