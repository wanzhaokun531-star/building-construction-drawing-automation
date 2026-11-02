#pragma once
#include <string>
#include <vector>
#include <istream>
#include <optional>
#include <utility>
namespace bcp {
constexpr double numericalEpsilonMm=1e-6;
struct Point { double x{},y{}; };
enum class WallRole { unknown,exterior,majorInterior,doorPier,localShort,bathroomPartition,majorBoundary,interiorPartition };
struct Wall { std::string handle;Point start,end;double left{},right{};WallRole role=WallRole::unknown; };
struct Opening { std::string handle;Point a,b;double width{},height{};std::string observedKind;std::string confirmedPrefix;std::vector<Point> endpointCandidates; };
struct RoomLabel {std::string handle;Point min,max;std::string text;};
struct Plan { std::string sourceSha;int sourceObjectCount{};std::vector<Wall> walls;std::vector<Opening> openings;std::vector<RoomLabel> rooms;std::vector<std::string> axisExclusions; };
struct Region {std::size_t id{};double area{};std::string kind;std::vector<Point> boundary;std::vector<std::string> labels;};
struct WallRecognition {std::string handle;WallRole role=WallRole::unknown;std::vector<std::size_t> faces;};
struct WallTopology {std::vector<Region> regions;std::vector<WallRecognition> walls;std::vector<std::vector<Point>> outerBodyContours;};
WallTopology recogniseWalls(const Plan& plan);
struct Issue {std::string code,handle;};
struct AxisSides {bool resolved=false,negative=false,positive=false;};
AxisSides axisSides(const std::vector<std::pair<double,double>>& ranges,double divider);
struct AxisFacade {bool resolved=false,negative=false,positive=false;double negativeFace{},positiveFace{};};
AxisFacade axisFacade(const std::vector<Point>& contour,bool vertical,double station,double negativeReach,double positiveReach,double wallHalfThickness);
std::vector<int> axisLabelLanes(const std::vector<double>& stations,double diameter);
std::vector<Point> axisJog(Point anchor,Point displayOffset,bool vertical);
std::vector<bool> outerAxisLabels(const std::vector<double>& stations,const std::vector<double>& reaches,bool negative,double diameter);
std::vector<std::pair<double,double>> axisIntervals(const std::vector<double>& stations);
enum class DimensionPointRole { axis,exteriorFace };
struct DimensionStation {double coordinate;DimensionPointRole role;};
std::pair<double,double> axisWallDisplaySpan(double wallMin,double wallMax,double bodyMin,double bodyMax,bool negative,bool positive,double outsideOverlap);
std::vector<std::pair<double,double>> axisSupportedDisplayIntervals(const std::vector<std::pair<double,double>>& walls,double bodyMin,double bodyMax,bool negative,bool positive,double outsideOverlap);
struct AxisTextBox {double xmin,ymin,xmax,ymax;};
std::vector<std::pair<double,double>> axisDisplayIntervals(bool vertical,double station,double low,double high,const std::vector<AxisTextBox>& boxes,double clearance);
std::vector<DimensionStation> exteriorDimensionStations(const std::vector<double>& stations,double outsideMin,double outsideMax,double wallHalfThickness);
struct Axis {bool vertical{};double coordinate{};std::string draftLabel;bool confirmed{};std::vector<std::string> walls;std::string confirmedLabel;AxisSides sides;double negativeReach{},positiveReach{};std::vector<std::pair<double,double>> supportRanges;};
struct WallChain {std::string wall;bool usable=true;std::vector<double> stations,segments;std::vector<std::string> openings;};
struct Result {std::vector<Axis> axes;std::vector<WallChain> chains;std::vector<Issue> issues;bool dimensionMeasurementsComplete=true;bool axisCandidatesComplete=true;WallTopology topology;};
Plan readCache(std::istream& input);
void readAxisExclusions(std::istream& input,Plan& plan);
void readEndpointSupplement(std::istream& input,Plan& plan);
std::optional<std::pair<Point,Point>> resolveGroupEndpoints(const Opening& opening,const std::vector<Wall>& walls);
Point bodyCentreStart(const Wall& wall);
std::optional<std::string> openingNumber(const std::string& prefix,double width,double height);
std::optional<std::string> alphabetLabel(std::size_t zeroBased);
Result analyse(const Plan& plan);
std::string toJson(const Plan& plan,const Result& result);
}
