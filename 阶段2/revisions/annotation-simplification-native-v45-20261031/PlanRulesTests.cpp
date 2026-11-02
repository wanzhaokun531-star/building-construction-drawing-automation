#include "PlanRules.h"
#include "UniqueRawParts.h"
#include <iostream>
#include <cmath>
#include <sstream>
#include <limits>
#include <algorithm>
int failed=0,checks=0;
void check(bool v,const char* n){++checks;if(!v){++failed;std::cerr<<"FAIL "<<n<<'\n';}}
bool issue(const bcp::Result& r,const std::string& c){return std::any_of(r.issues.begin(),r.issues.end(),[&](const bcp::Issue& i){return i.code==c;});}
int main(){using namespace bcp;
 check(axisSupportedDisplayIntervals({{0,3000},{7000,10000}},0,10000,false,false,1000)==std::vector<std::pair<double,double>>({{0,3000},{7000,10000}}),"empty courtyard gap not bridged between aligned walls");
 check(axisSupportedDisplayIntervals({{7000,10000},{0,3000}},0,10000,true,true,1000)==std::vector<std::pair<double,double>>({{-1000,3000},{7000,11000}}),"outer visible ends extend only first and last wall spans");
 check(axisSupportedDisplayIntervals({{0,3000},{3000,5000},{4500,8000}},0,10000,false,false,1000)==std::vector<std::pair<double,double>>({{0,8000}}),"touching overlapping host walls continuous without duplicate strokes");
 check(axisSupportedDisplayIntervals({{1000,9000}},0,10000,false,false,1000)==std::vector<std::pair<double,double>>({{1000,9000}}),"continuous host wall keeps axis through hosted door window opening");
 check(axisSupportedDisplayIntervals({{1000,2000},{2001,3000}},0,10000,false,false,1000).size()==2,"real small unsupported gap not bridged by engineering tolerance");
 bool badSupport=false;try{axisSupportedDisplayIntervals({},0,10000,false,false,1000);}catch(...){badSupport=true;}check(badSupport,"missing wall support blocks display generation");
 badSupport=false;try{axisSupportedDisplayIntervals({{1000,900}},0,10000,false,false,1000);}catch(...){badSupport=true;}check(badSupport,"reversed supported range rejected");
 badSupport=false;try{axisSupportedDisplayIntervals({{-200,900}},0,10000,false,false,1000);}catch(...){badSupport=true;}check(badSupport,"out of body wall support rejected");
 check(axisWallDisplaySpan(3000,6000,0,10000,false,false,1000)==std::make_pair(3000.0,6000.0),"local axis has wall span without exterior circles");
 check(axisWallDisplaySpan(7000,9900,0,10000,false,true,1000)==std::make_pair(7000.0,11000.0),"top-only axis does not cross unrelated lower rooms");
 check(axisWallDisplaySpan(100,3500,0,10000,true,false,1000)==std::make_pair(-1000.0,3500.0),"bottom-only axis ends at last associated wall");
 check(axisWallDisplaySpan(1000,9000,0,10000,true,true,1000)==std::make_pair(-1000.0,11000.0),"both visible axis ends connect original exterior short strokes");
 bool invalidSpan=false;try{axisWallDisplaySpan(7000,3000,0,10000,false,false,1000);}catch(...){invalidSpan=true;}check(invalidSpan,"reversed wall span rejected");
 invalidSpan=false;try{axisWallDisplaySpan(-200,3000,0,10000,false,false,1000);}catch(...){invalidSpan=true;}check(invalidSpan,"wall span outside body not silently clipped");
 invalidSpan=false;try{axisWallDisplaySpan(200,3000,0,10000,false,false,-1);}catch(...){invalidSpan=true;}check(invalidSpan,"negative short line connection overlap rejected");
 invalidSpan=false;try{axisWallDisplaySpan(200,std::numeric_limits<double>::infinity(),0,10000,false,false,1000);}catch(...){invalidSpan=true;}check(invalidSpan,"nonfinite wall end rejected");
 const std::vector<AxisTextBox> axisText{{450,400,550,600}};
 check(axisDisplayIntervals(true,500,0,1000,axisText,80)==std::vector<std::pair<double,double>>({{0,320},{680,1000}}),"axis text gap uses physical box plus display clearance");
 check(axisDisplayIntervals(false,500,0,1000,axisText,0)==std::vector<std::pair<double,double>>({{0,450},{550,1000}}),"horizontal axis subtracts text across x");
 check(axisDisplayIntervals(true,700,0,1000,axisText,80)==std::vector<std::pair<double,double>>({{0,1000}}),"nonintersecting text does not break axis");
 check(axisDisplayIntervals(true,500,0,1000,{{450,200,550,500},{450,400,550,700}},0)==std::vector<std::pair<double,double>>({{0,200},{700,1000}}),"overlapping text gaps merge without duplicate axis pieces");
 check(axisDisplayIntervals(true,500,0,1000,{{450,-100,550,100},{450,900,550,1100}},0)==std::vector<std::pair<double,double>>({{100,900}}),"text gap clipped at logical endpoints");
 check(axisDisplayIntervals(true,500,0,1000,{{450,-100,550,1100}},0).empty(),"fully blocked axis explicitly has no display interval");
 const auto retainedEnds=exteriorDimensionStations({100,4000,6200,10200,14000,17600},0,17700,100,true);
 check(retainedEnds.size()==8&&retainedEnds[1].coordinate==100&&retainedEnds[6].coordinate==17600,"both 100 terminal face-to-axis dimensions explicitly retained");
 check(retainedEnds[6].coordinate-retainedEnds[5].coordinate==3600&&retainedEnds[7].coordinate-retainedEnds[6].coordinate==100,"8 to9 stays3600 then9 to exterior100");
 const auto exterior=DimensionPointRole::exteriorFace;
 auto rightExterior=exteriorDimensionStations({2100,7700,11300,16300,18300},0,18400,100);
 check(rightExterior.size()==6&&rightExterior.front().coordinate==0&&rightExterior.front().role==exterior&&rightExterior.back().coordinate==18400&&rightExterior.back().role==exterior,"second row reaches both exterior faces");
 check(rightExterior[1].coordinate==2100&&rightExterior[1].role==DimensionPointRole::axis,"right lower B retained with independent 2100 segment");
 auto topExterior=exteriorDimensionStations({100,4000,6200,10200,14000,17600},0,17700,100);
 check(topExterior.size()==6&&topExterior[1].coordinate==4000&&topExterior[4].coordinate==14000,"outermost wall axes merged into faces without 100 slivers");
 auto internalExterior=exteriorDimensionStations({1100,2000},0,4000,100);
 check(internalExterior.size()==4&&internalExterior[1].coordinate==1100&&internalExterior[2].coordinate==2000,"interior end axes never swallowed to fill exterior gap");
 auto noVisibleExterior=exteriorDimensionStations({},0,4000,100);
 check(noVisibleExterior.size()==2&&noVisibleExterior.front().role==exterior&&noVisibleExterior.back().role==exterior,"wall-to-wall endpoints valid with no visible axis");
 bool invalidExterior=false;try{exteriorDimensionStations({2000,1100},0,4000,100);}catch(...){invalidExterior=true;}check(invalidExterior,"exterior chain rejects unsorted stations");
 invalidExterior=false;try{exteriorDimensionStations({-20,1100},0,4000,100);}catch(...){invalidExterior=true;}check(invalidExterior,"exterior chain rejects axes outside body envelope");
 invalidExterior=false;try{exteriorDimensionStations({100},0,std::numeric_limits<double>::infinity(),100);}catch(...){invalidExterior=true;}check(invalidExterior,"exterior chain rejects nonfinite face");
 const std::vector<Point> facadeBox{{0,0},{10000,0},{10000,10000},{0,10000}};
 check(!axisFacade(facadeBox,true,4000,4100,9900,100).negative,"facade hidden bottom is independent of label spacing");
 check(axisFacade(facadeBox,true,4000,4100,9900,100).positive,"same axis may keep only exposed top");
 check(axisFacade(facadeBox,true,5000,100,9900,100).negative&&axisFacade(facadeBox,true,5000,100,9900,100).positive,"actual wall half thickness is accepted at both endpoints");
 check(!axisFacade(facadeBox,false,5000,4100,9900,100).negative&&axisFacade(facadeBox,false,5000,100,5900,100).negative,"left facade and exposed left endpoint checked");
 check(!axisFacade(facadeBox,false,5000,100,5900,100).positive&&axisFacade(facadeBox,false,5000,4100,9900,100).positive,"right facade checked independently");
 check(!axisFacade(facadeBox,true,4000,100.01,9899.99,100).negative,"real endpoint shortfall is not rounded away");
 check(!axisFacade(facadeBox,true,12000,100,9900,100).resolved,"no contour projection stays unresolved");
 const std::vector<Point> stepped{{0,0},{5000,0},{5000,5000},{10000,5000},{10000,10000},{0,10000}};
 check(axisFacade(stepped,true,8000,5100,9900,100).negative,"recessed facade uses local outline not global bounding box");
 auto reversed=stepped;std::reverse(reversed.begin(),reversed.end());check(axisFacade(reversed,true,8000,5100,9900,100).negative,"facade support independent of polygon winding");

 check(axisIntervals({0,2000,3800})==std::vector<std::pair<double,double>>({{0,2000},{2000,3800}}),"dimension chain remeasures neighbours after inner near-axis omitted");check(axisIntervals({0,3900,6100}).size()==2&&axisIntervals({0,1100,5300,8900}).size()==3,"different sides can have different retained-axis dimension chains");check(axisIntervals({100}).empty(),"one visible axis has no interval");
 check(outerAxisLabels({20441,20541},{20511,16311},true,800)==std::vector<bool>({false,true}),"left near axes retain wall closest to left exterior");check(outerAxisLabels({20441,20541},{24111,19111},false,800)==std::vector<bool>({true,false}),"right near axes prefer farther right wall");check(outerAxisLabels({0,100,2000},{10,5,20},true,800)==std::vector<bool>({false,true,true}),"outer priority does not hide remote nonoverlapping axes");check(outerAxisLabels({0,100},{10,10},true,800)==std::vector<bool>({true,true}),"equal exterior reach is not arbitrarily suppressed");
 auto jog=axisJog({10,20},{-8,8},false);check(jog.size()==3&&jog[0].x==10&&jog[0].y==20&&jog[1].x==2&&jog[1].y==20&&jog[2].x==2&&jog[2].y==28,"display jog keeps true wall-axis anchor fixed");
 auto left=axisSides({{0,4000}},5000);check(left.negative&&!left.positive,"axis only left half hides right");auto right=axisSides({{6000,10000}},5000);check(!right.negative&&right.positive,"axis only right half hides left");auto both=axisSides({{1000,9000}},5000);check(both.negative&&both.positive,"axis across divider shows both");check(!axisSides({{0,5000}},5000).positive&&!axisSides({{5000,8000}},5000).negative,"touching divider does not cross it");check(!axisSides({},5000).resolved,"empty support remains unresolved");auto lanes=axisLabelLanes({0,100,2000},800);check(lanes==std::vector<int>({0,1,0}),"close visible axis circles stagger outwards");check(axisLabelLanes({0,800},800)==std::vector<int>({0,0}),"nonoverlapping tangent circles retain baseline");
 struct RawArray{std::vector<void*> data;int length()const{return static_cast<int>(data.size());}void* operator[](int i)const{return data[i];}};int first=1,second=2,visits=0;RawArray raw{{&first,&first,nullptr,&second,&first,&second}};nativeparts::forEachDistinct(raw,[&](void*){++visits;});check(visits==2,"raw duplicate pointers skipped before ownership/cast callback");
 Plan decisions;decisions.sourceSha=std::string(64,'A');decisions.walls={{"CD9A",{0,0},{1000,0},100,100},{"CFB9",{0,0},{0,1000},100,100}};std::istringstream decisionInput("S2AXISEXCLUSIONS1 "+decisions.sourceSha+"\nCD9A\nCFB9\n");readAxisExclusions(decisionInput,decisions);check(analyse(decisions).axes.empty()&&decisions.walls[0].role==WallRole::unknown,"user no-axis decision excludes axis without inventing wall subtype");
 bool badDecision=false;try{std::istringstream d("S2AXISEXCLUSIONS1 "+std::string(64,'B')+"\nCD9A\n");readAxisExclusions(d,decisions);}catch(...){badDecision=true;}check(badDecision,"axis decisions reject another drawing identity");badDecision=false;try{std::istringstream d("S2AXISEXCLUSIONS1 "+decisions.sourceSha+"\nFF\n");readAxisExclusions(d,decisions);}catch(...){badDecision=true;}check(badDecision,"axis decisions reject unknown wall handle");
 check(openingNumber("M",900,2100)==std::optional<std::string>("M0921"),"whole hundred door");
 check(openingNumber("C",900,2350)==std::optional<std::string>("C09235"),"explicit user non-hundred example");
 check(!openingNumber("C",950,2350),"unconfirmed simultaneous non-hundred does not guess");
 check(!openingNumber("U",900,2100),"unknown prefix rejected");
 check(!openingNumber("M",900,-100),"negative height rejected");
 Plan ordinary;ordinary.walls={{"A",{0,0},{3600,0},100,100}};ordinary.openings={{"B",{600,0},{1500,0},900,2100,"D",""}};auto ordinaryResult=analyse(ordinary);check(!issue(ordinaryResult,"OPENING_CATEGORY_REQUIRED")&&toJson(ordinary,ordinaryResult).find("M0921")!=std::string::npos,"native ordinary door maps to M without asking for dimensions again");
 check(alphabetLabel(7)==std::optional<std::string>("H")&&alphabetLabel(8)==std::optional<std::string>("J")&&alphabetLabel(13)==std::optional<std::string>("P"),"skip I O Z");
 check(!alphabetLabel(23),"unconfirmed multi-letter rule not invented");
 Plan p;p.walls={{"A",{0,0},{0,4000},100,100,WallRole::majorInterior},{"B",{2000,0},{2000,4000},50,50,WallRole::majorInterior},{"C",{500,0},{500,200},100,100,WallRole::doorPier},{"D",{1000,0},{1000,200},100,100,WallRole::localShort}};
 auto r=analyse(p);check(r.axes.size()==1&&r.axes[0].confirmed,"major200 yes inner100 and pier200 no");
 p.walls[0].role=WallRole::unknown;r=analyse(p);check(r.axes.size()==1&&!r.axes[0].confirmed&&issue(r,"WALL_ROLE_REQUIRED"),"unknown200 is provisional");
 p.walls={{"A",{0,0},{4000,0},50,50,WallRole::exterior}};r=analyse(p);check(r.axes.size()==1&&r.axes[0].confirmed,"explicit exterior has axis even if100");
 auto c=bodyCentreStart({"A",{0,0},{1000,0},100,0,WallRole::unknown});check(std::abs(c.y-50)<1e-9,"asymmetric thickness body centre");
 p.walls={{"A",{0,0},{0,4000},100,100,WallRole::majorInterior},{"B",{0.0000001,5000},{0.0000001,9000},100,100,WallRole::majorInterior},{"C",{0.1,0},{0.1,4000},100,100,WallRole::majorInterior}};r=analyse(p);check(r.axes.size()==2&&r.axes[0].walls.size()==2,"numeric noise merged but real offset retained");
 p.walls={{"CD83",{3600,0},{0,0},100,100,WallRole::unknown}};p.openings={{"CD84",{3000,0},{600,0},2400,1500,"W",""}};r=analyse(p);check(r.chains.size()==1&&r.chains[0].segments.size()==3&&std::abs(r.chains[0].segments[0]-600)<1e-8&&std::abs(r.chains[0].segments[1]-2400)<1e-8&&std::abs(r.chains[0].segments[2]-600)<1e-8,"real wall600 opening2400 wall600");
 check(issue(r,"OPENING_CATEGORY_REQUIRED"),"generic window does not infer sliding subtype");
 Plan asym;asym.walls={{"A",{0,0},{0,2000},100,0,WallRole::unknown}};asym.openings={{"B",{-50,100},{-50,900},800,2400,"D","M"}};auto ar=analyse(asym);check(!issue(ar,"OPENING_HOST_REQUIRED")&&ar.chains[0].openings.size()==1,"asymmetric wall opening matches body centre not baseline");
 asym.openings[0].observedKind="G";ar=analyse(asym);check(issue(ar,"GROUP_OPENING_GEOMETRY_REQUIRED")&&!ar.dimensionMeasurementsComplete,"group grip indices not assumed even if distance equals width");
 check(std::none_of(ar.chains.begin(),ar.chains.end(),[](const WallChain& c){return c.usable;}),"unlocated group uncertainty prevents usable wall chains");
 Plan thin;thin.walls={{"A",{0,0},{1000,0},50,50,WallRole::unknown}};auto tr=analyse(thin);check(issue(tr,"THIN_WALL_ROLE_REQUIRED")&&!tr.axisCandidatesComplete,"unknown100 is not silently treated as interior partition");
 thin.openings={{"B",{100,0},{900,0},900,2100,"D","M"}};tr=analyse(thin);check(!tr.chains[0].usable,"width mismatch uncertainty does not produce usable solid wall chain");
 p.openings.clear();r=analyse(p);check(r.chains.size()==1&&r.chains[0].segments==std::vector<double>{3600},"wall without opening still has dimension");
 p.openings={{"A",{100,0},{1500,0},1400,2100,"D","M"},{"B",{1000,0},{2000,0},1000,2100,"D","M"}};r=analyse(p);check(issue(r,"OPENING_OVERLAP")&&!r.chains[0].usable&&r.chains[0].segments.empty(),"overlap not silently merged");
 p.openings={{"A",{-100,0},{1000,0},1100,2100,"D","M"}};r=analyse(p);check(issue(r,"OPENING_HOST_REQUIRED"),"beyond end not clipped");
 p.walls.push_back(p.walls[0]);p.walls.back().handle="OTHER";p.openings={{"A",{100,0},{1000,0},900,2100,"D","M"}};r=analyse(p);check(issue(r,"OPENING_HOST_AMBIGUOUS"),"multiple geometric hosts require confirmation");
 p.walls[0].end={0,3600};p.walls.erase(p.walls.begin()+1);p.openings.clear();r=analyse(p);check(issue(r,"NONORTHOGONAL_WALL"),"oblique wall not silently assigned orthogonal axis");
 bool rejected=false;try{std::istringstream x("S2READCACHE1 bad 1 inch\n");readCache(x);}catch(...){rejected=true;}check(rejected,"invalid units and identity cache rejected");
 Plan group;group.walls={{"A",{0,0},{3600,0},100,100,WallRole::majorInterior}};group.openings={{"B",{1,1},{2,2},2400,2400,"G","",{{600,0},{3000,0},{1800,800},{600,0}}}};auto gr=analyse(group);check(gr.dimensionMeasurementsComplete&&gr.chains[0].usable&&gr.chains[0].segments==std::vector<double>({600,2400,600}),"group resolves from unique public endpoints and real host");
 group.openings[0].endpointCandidates.push_back({0,0});group.openings[0].endpointCandidates.push_back({2400,0});gr=analyse(group);check(!gr.dimensionMeasurementsComplete&&!gr.chains[0].usable,"two possible group endpoint pairs rejected");
 group.openings[0].endpointCandidates={{600,100},{3000,100}};check(!resolveGroupEndpoints(group.openings[0],group.walls),"parallel frame edge not mistaken for wall centre endpoints");
 rejected=false;try{std::istringstream x("S2NATIVEENDPOINTS1 "+std::string(64,'A')+"\nB 600 0\n");readEndpointSupplement(x,group);}catch(...){rejected=true;}check(rejected,"endpoint supplement wrong source rejected");
 rejected=false;try{std::istringstream x("S2READCACHE1 "+std::string(64,'A')+" 2 mm\nW A 0 0 1 0 100 100 U\nW A 0 0 1 0 100 100 U\n");readCache(x);}catch(...){rejected=true;}check(rejected,"duplicate handles rejected");
 Plan rooms;rooms.walls={{"1",{0,0},{4000,0},100,100},{"2",{4000,0},{4000,3000},100,100},{"3",{4000,3000},{0,3000},100,100},{"4",{0,3000},{0,0},100,100},{"5",{2000,0},{2000,3000},100,100},{"6",{500,1000},{500,1500},100,100},{"7",{3000,1000},{3000,2000},100,0}};rooms.rooms={{"A",{900,1400},{1100,1600},u8"卧室"},{"B",{2900,400},{3100,600},u8"厨房"}};auto topology=recogniseWalls(rooms);auto recognised=[&](const char* h){return std::find_if(topology.walls.begin(),topology.walls.end(),[&](const WallRecognition& w){return w.handle==h;});};
 check(recognised("1")!=topology.walls.end()&&recognised("1")->role==WallRole::exterior,"closed labelled building exterior recognised");
 check(recognised("5")!=topology.walls.end()&&recognised("5")->role==WallRole::majorBoundary,"room separator is major boundary");
 check(recognised("6")==topology.walls.end()||recognised("6")->role==WallRole::unknown,"isolated wall not guessed short or major");
 check(recognised("7")!=topology.walls.end()&&recognised("7")->role==WallRole::interiorPartition,"100 wall inside known indoor face classified");
 check(!topology.outerBodyContours.empty(),"body exterior offset contour emitted");
 rooms.walls[6].start.y=0;rooms.walls[6].end.y=3000;topology=recogniseWalls(rooms);check(recognised("7")!=topology.walls.end()&&recognised("7")->role==WallRole::interiorPartition,"indoor100 touching known face boundary remains interior");
 rooms.walls[0].start.x=.1;topology=recogniseWalls(rooms);check(topology.outerBodyContours.empty(),"real gap not bridged to invent closed exterior");
 rooms.walls[0].start.x=0;for(auto& room:rooms.rooms)room.text=u8"花池";topology=recogniseWalls(rooms);check(topology.outerBodyContours.empty(),"landscape closed loop not called building");
 Plan garden;garden.walls={{"A",{0,0},{4000,0},100,100},{"B",{4000,0},{4000,3000},100,100},{"C",{4000,3000},{0,3000},100,100},{"D",{0,3000},{0,0},100,100},{"E",{4000,0},{5000,0},100,100},{"F",{5000,0},{5000,1000},100,100},{"10",{5000,1000},{4000,1000},100,100}};garden.rooms={{"11",{900,1400},{1100,1600},u8"卧室"},{"12",{4400,400},{4600,600},u8"花池"}};auto gt=recogniseWalls(garden);check(std::none_of(gt.walls.begin(),gt.walls.end(),[](const WallRecognition& w){return (w.handle=="E"||w.handle=="F"||w.handle=="10")&&w.role!=WallRole::unknown;}),"attached landscape loop is not confirmed building wall");check(gt.outerBodyContours.size()==1&&std::none_of(gt.outerBodyContours[0].begin(),gt.outerBodyContours[0].end(),[](Point p){return p.x>4100.000001;}),"attached landscape omitted from body contour");
 Plan concave;std::vector<Point> polygon{{0,0},{10000,0},{10000,10000},{3000,10000},{3000,4000},{2000,4000},{2000,10000},{0,10000}};for(std::size_t i=0;i<polygon.size();++i)concave.walls.push_back({std::to_string(i+1),polygon[i],polygon[(i+1)%polygon.size()],100,100});concave.walls.push_back({"F",{1000,5000},{9000,5000},50,50});concave.rooms={{"11",{4900,4900},{5100,5100},u8"卧室"}};auto ct=recogniseWalls(concave);check(ct.walls.back().role==WallRole::unknown,"100 wall crossing concave exterior notch remains unconfirmed");

 Plan bp;bp.walls={{"B1",{0,0},{3000,0},100,0},{"B2",{0,0},{0,2000},100,0},{"BED",{5000,0},{6000,0},50,50},{"MAIN",{0,0},{0,6000},100,100}};bp.rooms={{"WC",{500,500},{700,700},u8"卫"},{"BR",{5000,500},{5200,700},u8"卧室"}};WallTopology bt;bt.regions={{0,60000000,"indoor",{}, {"WC","BR"}}};bt.walls={{"B1",WallRole::interiorPartition,{0}},{"B2",WallRole::interiorPartition,{0}},{"BED",WallRole::interiorPartition,{0}},{"MAIN",WallRole::majorBoundary,{0}}};
 check(bathroomPartitionAnnotations(bp,bt)==std::vector<std::string>({"B1","B2"}),"bathroom connected thin partitions suppressed; separate bedroom partition and main wall retained");
 bp.rooms[1].min={1000,500};bp.rooms[1].max={1200,700};check(bathroomPartitionAnnotations(bp,bt).empty(),"mixed-use partition envelope must not suppress a bedroom partition");
 bt.regions[0].labels={"WC"};check(bathroomPartitionAnnotations(bp,bt).size()==3,"pure bathroom face excludes all recognised thin partitions");
 bp.openings={{"A",{0,0},{900,0},900,2400,"W",""},{"B",{0,0},{2400,0},2400,2400,"G",""}};bp.sourceSha=std::string(64,'A');std::istringstream pref("S2OPENINGPREFIXES1 "+bp.sourceSha+"\nA C\nB C\n");readOpeningPrefixes(pref,bp);check(bp.openings[0].confirmedPrefix=="C"&&bp.openings[1].confirmedPrefix=="C","explicit C applies to unknown window and group without changing kind");
 rejected=false;try{std::istringstream pref2("S2OPENINGPREFIXES1 "+std::string(64,'B')+"\nA C\n");readOpeningPrefixes(pref2,bp);}catch(...){rejected=true;}check(rejected,"opening prefix input must match frozen source");
 std::vector<DimensionTextRun> kitchen{{0,700,{650,350}},{700,3100,{650,1900}},{3100,3800,{650,3450}}};auto kl=continuousDimensionChainLayout(true,kitchen,{{500,-100,800,4000}});check(kl&&std::abs(kl->normalOffset)>1e-6&&std::all_of(kl->textPositions.begin(),kl->textPositions.end(),[&](Point p){return std::abs(p.x-kl->textPositions.front().x)<1e-6;}),"700/2400/700 uses one common relocated dimension plane");
 check(continuousDimensionChainLayout(false,{{0,100,{50,650}},{100,1000,{550,650}},{1000,1200,{1100,650}}},{}).has_value(),"short text can use leaders while dimension chain plane stays common");
 check(continuousDimensionChainLayout(true,{{0,100,{650,50}},{100,900,{650,500}},{900,1600,{650,1250}}},{}).has_value(),"100/800 chain must backtrack the first short text to a leader instead of abandoning the common plane");
 check(!continuousDimensionChainLayout(true,kitchen,{{-10000,-10000,10000,10000}}),"blocked chain must report no layout instead of splitting individual baselines");
 std::cout<<"compiler="<<_MSC_VER<<" checks="<<checks<<" failed="<<failed<<'\n';return failed?1:0;
}
