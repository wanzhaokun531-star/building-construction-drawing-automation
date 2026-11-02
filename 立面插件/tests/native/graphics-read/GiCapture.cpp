#include "GiCapture.h"
#include <dbpl.h>
#include <dbsymtb.h>
#include <dbsymutl.h>
#include <dbapserv.h>
#include <geassign.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <type_traits>
namespace bce::graphics {
using namespace native;
std::string number(double value) {
    require(std::isfinite(value), "nonfinite callback value");
    std::ostringstream out; out.imbue(std::locale::classic()); out << std::setprecision(17) << value;
    return out.str();
}
std::string handle(AcDbObjectId id) {
    if (id.isNull()) return {};
    ACHAR text[32]{}; id.handle().getIntoAsciiBuffer(text); return utf8(text);
}
namespace {
std::string q(const char* s) { return quote(std::string(s)); }
std::string b(bool v) { return v ? "true" : "false"; }
std::wstring ws(const ACHAR* s) { return s ? s : L""; }
std::string pt(const AcGePoint3d& p) { return array({number(p.x),number(p.y),number(p.z)}); }
std::string vec(const AcGeVector3d& p) { return array({number(p.x),number(p.y),number(p.z)}); }
Triple triple(const AcGePoint3d& p){return {p.x,p.y,p.z};}
Triple triple(const AcGeVector3d& p){return {p.x,p.y,p.z};}
std::string mat(const AcGeMatrix3d& m) {
    std::vector<std::string> a; for(int r=0;r<4;++r) for(int c=0;c<4;++c) a.push_back(number(m(r,c))); return array(a);
}
struct TraitValues {
    Adesk::UInt16 color{256}; AcCmEntityColor trueColor;
    AcDbObjectId layer, linetype, visual, material, plot;
    AcDb::PlotStyleNameType plotType{AcDb::kPlotStyleNameByLayer};
    AcGiFillType fill{kAcGiFillNever}; AcDb::LineWeight weight{AcDb::kLnWtByLayer};
    double scale{1}, thickness{}; Adesk::LongPtr marker{}; Adesk::UInt32 flags{};
    AcGiSubEntityTraits::ShadowFlags shadow{AcGiSubEntityTraits::kShadowsCastAndReceive};
    bool section{}, selection{}; AcCmTransparency transparency;
    std::vector<std::pair<bool,Adesk::LongPtr>> markers;
};
struct State {
    AcDbDatabase& db; Result result;
    std::function<void(const std::string&)> log;
    std::vector<AcGeMatrix3d> matrices{AcGeMatrix3d::kIdentity};
    std::vector<std::string> clips, path;
    std::vector<const AcGiDrawable*> active; TraitValues traits;
    std::size_t nodes{},pointCount{},textUnits{},byteCount{}; bool aborted{};
    State(AcDbDatabase& d, std::function<void(const std::string&)> l):db(d),log(std::move(l)){}
    void limit(const std::string& reason) { ++result.coverage.unknown; result.limits.push_back(quote(reason)); }
    std::string traitsJson() const {
        return object({{"color",std::to_string(traits.color)}, {"trueColor",std::to_string(traits.trueColor.color())},
          {"layer",quote(handle(traits.layer))},{"linetype",quote(handle(traits.linetype))},
          {"weight",std::to_string(traits.weight)},{"fill",std::to_string(traits.fill)},
          {"scale",number(traits.scale)},{"thickness",number(traits.thickness)},
          {"marker",std::to_string(traits.marker)},{"flags",std::to_string(traits.flags)},
          {"shadow",std::to_string(traits.shadow)},{"selection",b(traits.selection)},
          {"section",b(traits.section)},{"visual",quote(handle(traits.visual))},
          {"material",quote(handle(traits.material))},{"plot",quote(handle(traits.plot))},
          {"plotType",std::to_string(traits.plotType)},
          {"transparency",std::to_string(traits.transparency.serializeOut())}});
    }
    bool emit(const char* kind, Fields fields={}) {
        if (aborted) return true;
        if (++result.coverage.callbacks > 50000) { aborted=true; result.coverage.budgetExceeded=true; return true; }
        fields.push_back({"kind",q(kind)}); fields.push_back({"path",array(path)});
        fields.push_back({"modelToWorld",mat(matrices.back())}); fields.push_back({"clips",array(clips)});
        fields.push_back({"traits",traitsJson()});
        auto line=object(fields);byteCount+=line.size();require(byteCount<=64*1024*1024,"callback byte budget exceeded");result.callbacks.push_back(line); log(line); return false;
    }
    bool unsupported(const char* kind) { limit(kind); return emit(kind,{{"supported","false"}}); }
    bool points(const char* kind, Adesk::UInt32 count, const AcGePoint3d* p, Fields extra={}) {
        require(count <= 100000 && (count == 0 || p), "invalid or over-budget point array");
        pointCount+=count;require(pointCount<=2000000,"total point budget exceeded");
        std::vector<std::string> raw,world;
        PrimitiveObservation primitive;primitive.kind=kind;
        for(Adesk::UInt32 i=0;i<count;++i) { raw.push_back(pt(p[i])); world.push_back(pt(matrices.back()*p[i]));primitive.points.push_back(triple(matrices.back()*p[i])); }
        result.observation.primitives.push_back(std::move(primitive));
        extra.push_back({"points",array(raw)});extra.push_back({"worldPoints",array(world)});
        return emit(kind,extra);
    }
};
class Traits final : public AcGiDrawableTraits {
public:
    explicit Traits(State& s):s_(s){}
    void setColor(Adesk::UInt16 v) override { s_.traits.color=v; s_.traits.trueColor.setColorIndex(v); }
    void setTrueColor(const AcCmEntityColor& v) override { s_.traits.trueColor=v; s_.traits.color=v.colorIndex(); }
    void setLayer(AcDbObjectId v) override { s_.traits.layer=v; }
    void setLineType(AcDbObjectId v) override { s_.traits.linetype=v; }
    void setSelectionMarker(Adesk::LongPtr v) override { s_.traits.marker=v; }
    void setFillType(AcGiFillType v) override { s_.traits.fill=v; }
    void setLineWeight(AcDb::LineWeight v) override { s_.traits.weight=v; }
    void setLineTypeScale(double v) override { s_.traits.scale=v; }
    void setThickness(double v) override { s_.traits.thickness=v; }
    Adesk::UInt16 color() const override {return s_.traits.color;}
    AcCmEntityColor trueColor() const override {return s_.traits.trueColor;}
    AcDbObjectId layerId() const override {return s_.traits.layer;}
    AcDbObjectId lineTypeId() const override {return s_.traits.linetype;}
    AcGiFillType fillType() const override {return s_.traits.fill;}
    AcDb::LineWeight lineWeight() const override {return s_.traits.weight;}
    double lineTypeScale() const override {return s_.traits.scale;}
    double thickness() const override {return s_.traits.thickness;}
    void setVisualStyle(AcDbObjectId v) override {s_.traits.visual=v;}
    AcDbObjectId visualStyle() const override {return s_.traits.visual;}
    void setMaterial(AcDbObjectId v) override {s_.traits.material=v;}
    AcDbObjectId materialId() const override {return s_.traits.material;}
    void setPlotStyleName(AcDb::PlotStyleNameType v,const AcDbObjectId& id) override {s_.traits.plotType=v;s_.traits.plot=id;}
    AcDb::PlotStyleNameType getPlotStyleNameId(AcDbObjectId& id) const override {id=s_.traits.plot;return s_.traits.plotType;}
    void setSectionable(bool v) override {s_.traits.section=v;}
    bool sectionable() const override {return s_.traits.section;}
    Acad::ErrorStatus setDrawFlags(Adesk::UInt32 v) override {s_.traits.flags=v;return Acad::eOk;}
    Adesk::UInt32 drawFlags() const override {return s_.traits.flags;}
    Acad::ErrorStatus setShadowFlags(ShadowFlags v) override {s_.traits.shadow=v;return Acad::eOk;}
    ShadowFlags shadowFlags() const override {return s_.traits.shadow;}
    void setSelectionGeom(bool v) override {s_.traits.selection=v;}
    bool selectionGeom() const override {return s_.traits.selection;}
    void setTransparency(const AcCmTransparency& v) override {s_.traits.transparency=v;}
    AcCmTransparency transparency() const override {return s_.traits.transparency;}
    void setMapper(const AcGiMapper* v) override {if(v)s_.unsupported("materialMapper");}
    const AcGiMapper* mapper() const override {return nullptr;}
    void setFill(const AcGiFill* v) override {if(v)s_.unsupported("fillTrait");}
    const AcGiFill* fill() const override {return nullptr;}
    void pushMarkerOverride(bool flag,Adesk::LongPtr marker) override {s_.traits.markers.emplace_back(flag,marker);s_.limit("markerOverrideNotApplied");}
    void popMarkerOverride() override {require(!s_.traits.markers.empty(),"marker stack underflow");s_.traits.markers.pop_back();}
    void clearMarkerOverride() override {s_.traits.markers.clear();}
private: State& s_;
};
class Context final : public AcGiContext {
public:
    explicit Context(State& s):s_(s){}
    Adesk::Boolean isPsOut() const override {return false;}
    Adesk::Boolean isPlotGeneration() const override {return false;}
    AcDbDatabase* database() const override {return &s_.db;}
    bool isBoundaryClipping() const override {return !s_.clips.empty();}
    bool isNesting() const override {return s_.active.size()>1;}
    AcCmEntityColor effectiveColor() const override {return s_.traits.trueColor;}
private: State& s_;
};
struct TextRequest {
    std::wstring text,font,bigfont,styleName;
    AcString typeface;bool bold{},italic{};Charset charset{};
    Autodesk::AutoCAD::PAL::FontUtils::FontPitch pitch{};
    Autodesk::AutoCAD::PAL::FontUtils::FontFamily family{};
    Acad::ErrorStatus fontStatus{Acad::eNotApplicable};
    double height{},width{},oblique{},tracking{};
    bool backward{},upsideDown{},vertical{},over{},under{},strike{},raw{};
    AcGePoint3d position; AcGeVector3d normal,direction; AcGeMatrix3d matrix;
    std::size_t callback{};
};
class World;
class Geometry final : public AcGiWorldGeometry {
public:
    Geometry(State& s, World& w):s_(s),w_(w){}
    void getModelToWorldTransform(AcGeMatrix3d& m) const override {m=s_.matrices.back();}
    void getWorldToModelTransform(AcGeMatrix3d& m) const override {m=s_.matrices.back().inverse();}
    Adesk::Boolean pushModelTransform(const AcGeVector3d& n) override {
        require(n.length()>1e-12,"zero transform normal"); return pushModelTransform(AcGeMatrix3d::planeToWorld(n));
    }
    Adesk::Boolean pushModelTransform(const AcGeMatrix3d& m) override {
        require(s_.matrices.size()<128,"matrix budget exceeded");
        s_.matrices.push_back(s_.matrices.back()*m); s_.emit("pushModelTransform",{{"pushed",mat(m)}});return true;
    }
    Adesk::Boolean popModelTransform() override {require(s_.matrices.size()>1,"matrix stack underflow");s_.emit("popModelTransform");s_.matrices.pop_back();return true;}
    AcGeMatrix3d unknownTransform(const char* kind) {s_.unsupported(kind);pushModelTransform(AcGeMatrix3d::kIdentity);s_.result.coverage.transformsRecorded=false;return AcGeMatrix3d::kIdentity;}
    AcGeMatrix3d pushPositionTransform(AcGiPositionTransformBehavior,const AcGePoint3d&) override {return unknownTransform("positionTransform3dRequiresView");}
    AcGeMatrix3d pushPositionTransform(AcGiPositionTransformBehavior,const AcGePoint2d&) override {return unknownTransform("positionTransform2dRequiresView");}
    AcGeMatrix3d pushScaleTransform(AcGiScaleTransformBehavior,const AcGePoint3d&) override {return unknownTransform("scaleTransform3dRequiresView");}
    AcGeMatrix3d pushScaleTransform(AcGiScaleTransformBehavior,const AcGePoint2d&) override {return unknownTransform("scaleTransform2dRequiresView");}
    AcGeMatrix3d pushOrientationTransform(AcGiOrientationTransformBehavior) override {return unknownTransform("orientationTransformRequiresView");}
    Adesk::Boolean draw(AcGiDrawable* d) const override;
    Adesk::Boolean pushClipBoundary(AcGiClipBoundary* c) override {
        require(c && s_.clips.size()<128,"clip missing/budget");std::vector<std::string> points;
        require(c->m_aptPoints.length()<100000,"clip point budget");
        for(int i=0;i<c->m_aptPoints.length();++i)points.push_back(array({number(c->m_aptPoints[i].x),number(c->m_aptPoints[i].y)}));
        s_.clips.push_back(object({{"normal",vec(c->m_vNormal)},{"point",pt(c->m_ptPoint)},
         {"points",array(points)},{"toClip",mat(c->m_xToClipSpace)},{"inverseBlock",mat(c->m_xInverseBlockRefXForm)},
         {"front",b(c->m_bClippingFront)},{"back",b(c->m_bClippingBack)},
         {"frontZ",number(c->m_dFrontClipZ)},{"backZ",number(c->m_dBackClipZ)},{"drawBoundary",b(c->m_bDrawBoundary)}}));
        s_.limit("clipRecordedButNotAppliedToPrimitives");s_.emit("pushClip");return true;
    }
    void popClipBoundary() override {require(!s_.clips.empty(),"clip underflow");s_.emit("popClip");s_.clips.pop_back();}
    void setExtents(AcGePoint3d* p) const override {s_.points("auxiliarySetExtentsNotGlyph",2,p);}
    void startAttributesSegment() override {s_.emit("startAttributesSegment");}
    Adesk::Boolean circle(const AcGePoint3d& c,double r,const AcGeVector3d& n) const override {return s_.emit("circle",{{"center",pt(c)},{"radius",number(r)},{"normal",vec(n)}});}
    Adesk::Boolean circle(const AcGePoint3d& a,const AcGePoint3d& c,const AcGePoint3d& e) const override {AcGePoint3d p[]={a,c,e};return s_.points("circleThreePoints",3,p);}
    Adesk::Boolean circularArc(const AcGePoint3d& c,double r,const AcGeVector3d& n,const AcGeVector3d& start,double sweep,AcGiArcType type) const override {
        PrimitiveObservation p;p.kind="circularArc";p.center=triple(s_.matrices.back()*c);p.normal=triple(s_.matrices.back()*n);p.startVector=triple(s_.matrices.back()*start);p.radius=r;p.sweep=sweep;
        s_.result.observation.primitives.push_back(std::move(p));
        return s_.emit("circularArc",{{"center",pt(c)},{"radius",number(r)},{"normal",vec(n)},{"startVector",vec(start)},{"sweep",number(sweep)},{"arcType",std::to_string(type)}});
    }
    Adesk::Boolean circularArc(const AcGePoint3d& a,const AcGePoint3d& c,const AcGePoint3d& e,AcGiArcType type) const override {AcGePoint3d p[]={a,c,e};return s_.points("arcThreePoints",3,p,{{"arcType",std::to_string(type)}});}
    Adesk::Boolean polyline(Adesk::UInt32 n,const AcGePoint3d* p,const AcGeVector3d* normal,Adesk::LongPtr marker) const override {return s_.points("polyline",n,p,{{"normal",normal?vec(*normal):"null"},{"baseMarker",std::to_string(marker)}});}
    Adesk::Boolean polygon(Adesk::UInt32 n,const AcGePoint3d* p) const override {return s_.points("polygon",n,p);}
    Adesk::Boolean polyline(const AcGiPolyline& p) const override {
        const auto n=p.points();require(n<=100000,"AcGiPolyline point budget");
        // Optional per-segment array lengths are not assumed. Preserve points and
        // public scalar settings; report non-null unparsed arrays as partial.
        if(p.subEntMarkerList()||p.arcSegmentFlags()||p.bulge()||p.width())s_.limit("AcGiPolylineOptionalSegmentArraysNotDecoded");
        return s_.points("AcGiPolyline",n,p.vertexList(),{{"normal",p.normal()?vec(*p.normal()):"null"},
          {"linetypeGeneration",std::to_string(p.linetypeGen())},{"baseMarker",std::to_string(p.baseSubEntMarker())},
          {"closed",b(p.isClosed())},{"elevation",number(p.elevation())},{"thickness",number(p.thickness())},{"constantWidth",number(p.constantWidth())}});
    }
    Adesk::Boolean polyPolyline(Adesk::UInt32 n,const AcGiPolyline* p) const override {require(n<=10000&&(n==0||p),"polyPolyline budget/null");for(Adesk::UInt32 i=0;i<n;++i)if(polyline(p[i]))return true;return false;}
    Adesk::Boolean polyPolygon(Adesk::UInt32,const Adesk::UInt32*,const AcGePoint3d*,const Adesk::UInt32*,const AcGePoint3d*,const AcCmEntityColor*,const AcGiLineType*,const AcCmEntityColor*,const AcCmTransparency*) const override {return s_.unsupported("polyPolygon");}
    Adesk::Boolean mesh(Adesk::UInt32 rows,Adesk::UInt32 cols,const AcGePoint3d* p,const AcGiEdgeData* e,const AcGiFaceData* f,const AcGiVertexData* v,bool normals) const override {
        require(rows<=100000 && cols<=100000 && static_cast<unsigned long long>(rows)*cols<=100000,"mesh budget");
        if(e||f||v)s_.limit("meshAttributesNotDecoded");return s_.points("mesh",rows*cols,p,{{"rows",std::to_string(rows)},{"columns",std::to_string(cols)},{"autoNormals",b(normals)}});
    }
    Adesk::Boolean shell(Adesk::UInt32 n,const AcGePoint3d* p,Adesk::UInt32 count,const Adesk::Int32* faces,const AcGiEdgeData* e,const AcGiFaceData* f,const AcGiVertexData* v,const resbuf* rb,bool normals) const override {
        require(count<=100000 && (count==0||faces),"shell face budget/null");std::vector<std::string> list;
        for(Adesk::UInt32 i=0;i<count;++i)list.push_back(std::to_string(faces[i]));
        if(e||f||v||rb)s_.limit("shellAttributesNotDecoded");return s_.points("shell",n,p,{{"faces",array(list)},{"autoNormals",b(normals)}});
    }
    Adesk::Boolean text(const AcGePoint3d& p,const AcGeVector3d& n,const AcGeVector3d& d,double h,double width,double oblique,const ACHAR* msg) const override {
        s_.limit("shortTextEffectiveBuiltInFontUnknown");return recordText(p,n,d,msg,-1,false,nullptr,h,width,oblique);
    }
    Adesk::Boolean text(const AcGePoint3d& p,const AcGeVector3d& n,const AcGeVector3d& d,const ACHAR* msg,Adesk::Int32 length,Adesk::Boolean raw,const AcGiTextStyle& style) const override {
        return recordText(p,n,d,msg,length,raw,&style,style.textSize(),style.xScale(),style.obliquingAngle());
    }
    Adesk::Boolean xline(const AcGePoint3d& a,const AcGePoint3d& c) const override {AcGePoint3d p[]={a,c};return s_.points("xline",2,p);}
    Adesk::Boolean ray(const AcGePoint3d& a,const AcGePoint3d& c) const override {AcGePoint3d p[]={a,c};return s_.points("ray",2,p);}
    Adesk::Boolean pline(const AcDbPolyline&,Adesk::UInt32,Adesk::UInt32) const override {return s_.unsupported("lightweightPolyline");}
    Adesk::Boolean image(const AcGiImageBGRA32&,const AcGePoint3d&,const AcGeVector3d&,const AcGeVector3d&,TransparencyMode) const override {return s_.unsupported("image");}
    Adesk::Boolean rowOfDots(int count,const AcGePoint3d& p,const AcGeVector3d& step) const override {return s_.emit("rowOfDots",{{"count",std::to_string(count)},{"start",pt(p)},{"step",vec(step)}});}
    Adesk::Boolean ellipticalArc(const AcGePoint3d& p,const AcGeVector3d& n,double major,double minor,double start,double end,double tilt,AcGiArcType type) const override {return s_.emit("ellipticalArc",{{"center",pt(p)},{"normal",vec(n)},{"major",number(major)},{"minor",number(minor)},{"start",number(start)},{"end",number(end)},{"tilt",number(tilt)},{"arcType",std::to_string(type)}});}
    Adesk::Boolean edge(const AcArray<AcGeCurve2d*>&) const override {return s_.unsupported("curveEdges");}
    Adesk::Boolean ownerDraw(AcGiGdiDrawObject*,const AcGePoint3d&,const AcGeVector3d&,const AcGeVector3d&) const override {return s_.unsupported("ownerDraw");}
    mutable std::vector<TextRequest> texts;
private:
    bool recordText(const AcGePoint3d& p,const AcGeVector3d& n,const AcGeVector3d& d,const ACHAR* msg,int length,bool raw,const AcGiTextStyle* style,double h,double width,double oblique) const {
        require(msg && length>=-1 && length<=65536,"invalid text pointer/length");
        const auto size=length<0?wcsnlen_s(msg,65537):static_cast<std::size_t>(length);
        require(size<=65536,"unterminated/over-budget text");
        s_.textUnits+=size;require(s_.textUnits<=1000000,"total text budget exceeded");
        // ACHAR is UTF16 in this build; retain explicit count and every code unit.
        // SDK wording says bytes; therefore positive-length requests remain qualified.
        std::wstring text(msg,size); std::vector<std::string> units;bool nonAscii=false;
        for(auto c:text){units.push_back(std::to_string(static_cast<unsigned short>(c)));nonAscii=nonAscii||c>127;}
        if(length>=0 && nonAscii)s_.limit("positiveUnicodeLengthContractNeedsIndependentRender");
        Fields fields={{"text",quote(text)},{"utf16",array(units)},{"rawLength",std::to_string(length)},
         {"copiedCodeUnits",std::to_string(size)},{"raw",b(raw)},{"position",pt(p)},
         {"worldPosition",pt(s_.matrices.back()*p)},{"normal",vec(n)},{"direction",vec(d)},
         {"height",number(h)},{"width",number(width)},{"oblique",number(oblique)},
         {"styleComplete",b(style!=nullptr)}};
        TextObservation observed;observed.text=text;observed.position=triple(s_.matrices.back()*p);
        observed.height=h;observed.width=width;observed.oblique=oblique;observed.styleComplete=style!=nullptr;
        if(style){
            TextRequest t; t.text=text;t.font=ws(style->fileName());t.bigfont=ws(style->bigFontFileName());t.styleName=ws(style->styleName());
            t.height=h;t.width=width;t.oblique=oblique;t.tracking=style->trackingPercent();
            t.backward=style->isBackward();t.upsideDown=style->isUpsideDown();t.vertical=style->isVertical();
            t.over=style->isOverlined();t.under=style->isUnderlined();t.strike=style->isStrikethrough();t.raw=raw;
            t.position=p;t.normal=n;t.direction=d;t.matrix=s_.matrices.back();t.callback=s_.result.coverage.callbacks+1;
            t.fontStatus=style->font(t.typeface,t.bold,t.italic,t.charset,t.pitch,t.family);
            observed.font=t.font;observed.bigfont=t.bigfont;observed.tracking=t.tracking;
            if(n.length()>1e-12&&d.length()>1e-12&&std::abs(n.normal().dotProduct(d.normal()))<1e-8){
                const auto x=d.normal()*h*width*(t.backward?-1:1);
                const auto y=n.normal().crossProduct(d.normal())*h*(t.upsideDown?-1:1);
                observed.xAxis=triple(s_.matrices.back()*x);observed.yAxis=triple(s_.matrices.back()*y);observed.basisValid=true;
                fields.push_back({"effectiveWorldXAxis",vec(s_.matrices.back()*x)});fields.push_back({"effectiveWorldYAxis",vec(s_.matrices.back()*y)});
            }
            fields.insert(fields.end(),{{"font",quote(t.font)},{"bigfont",quote(t.bigfont)},{"styleName",quote(t.styleName)},
              {"tracking",number(t.tracking)},{"backward",b(t.backward)},{"upsideDown",b(t.upsideDown)},
              {"vertical",b(t.vertical)},{"overlined",b(t.over)},{"underlined",b(t.under)},{"strikethrough",b(t.strike)},
              {"fontStatus",std::to_string(t.fontStatus)},{"typeface",quote(ws(t.typeface.constPtr()))},{"bold",b(t.bold)},{"italic",b(t.italic)},
              {"charset",std::to_string(static_cast<int>(t.charset))},{"pitch",std::to_string(static_cast<int>(t.pitch))},{"family",std::to_string(static_cast<int>(t.family))}});
            texts.push_back(std::move(t));
        }
        s_.result.observation.texts.push_back(std::move(observed));
        s_.result.textRequests.push_back(object(fields));++s_.result.textCount;
        return s_.emit("text",fields);
    }
    State& s_; World& w_;
};
class World final : public AcGiWorldDraw {
public:
    explicit World(State& s):s_(s),traits_(s),context_(s),geometry_(s,*this){}
    AcGiRegenType regenType() const override {return kAcGiStandardDisplay;}
    Adesk::Boolean regenAbort() const override {return s_.aborted;}
    AcGiSubEntityTraits& subEntityTraits() const override {return traits_;}
    AcGiGeometry* rawGeometry() const override {return &geometry_;}
    AcGiWorldGeometry& geometry() const override {return geometry_;}
    Adesk::Boolean isDragging() const override {return false;}
    double deviation(AcGiDeviationType,const AcGePoint3d&) const override {return 0.01;}
    Adesk::UInt32 numberOfIsolines() const override {return 4;}
    AcGiContext* context() override {return &context_;}
    bool enter(AcGiDrawable* d) {
        ++s_.result.coverage.drawRequests;require(d,"null child drawable");
        if(d->isPersistent())require(!d->id().isNull()&&d->id().database()==&s_.db,"drawable dependency is not in target db");
        if(s_.active.size()>=32 || ++s_.nodes>10000) {s_.result.coverage.budgetExceeded=true;s_.aborted=true;return true;}
        require(std::find(s_.active.begin(),s_.active.end(),d)==s_.active.end(),"recursive drawable cycle");
        const auto oldTraits=s_.traits;const auto depth=s_.matrices.size(),clipDepth=s_.clips.size();
        s_.active.push_back(d);s_.path.push_back(quote(utf8(d->isA()->name())+":"+std::to_string(s_.nodes)));
        ++s_.result.coverage.drawsEntered;
        s_.emit("drawEnter",{{"persistent",b(d->isPersistent())},{"objectId",quote(handle(d->id()))}});
        const auto flags=d->setAttributes(&traits_);s_.emit("setAttributes",{{"flags",std::to_string(flags)}});
        // 2020 drawable.h: kDrawableViewIndependentViewportDraw explicitly
        // does not use worldDraw, regardless of what a direct call might return.
        if(requiresWorldDraw((flags&AcGiDrawable::kDrawableViewIndependentViewportDraw)!=0)){
            const bool complete=d->worldDraw(this);s_.emit("worldDrawReturn",{{"value",b(complete)}});
            if(!complete){++s_.result.coverage.viewportRequired;s_.result.limits.push_back(q("viewportNotCollected"));}
        }else{
            ++s_.result.coverage.viewportRequired;s_.result.limits.push_back(q("viewportNotCollected:attributeViewIndependentViewportDraw"));
            s_.emit("worldDrawSkippedViewportOnlyAttribute",{{"viewportNotCollected","true"}});
        }
        if(s_.matrices.size()!=depth||s_.clips.size()!=clipDepth||s_.traits.markers.size()!=oldTraits.markers.size()){
            s_.result.coverage.stackBalanced=false;throw std::runtime_error("child transform/clip/marker stack unbalanced");
        }
        s_.path.pop_back();s_.active.pop_back();s_.traits=oldTraits;return s_.aborted;
    }
    void metrics() {
        for(const auto& t:geometry_.texts){
            AcGiTextStyle style(t.font.c_str(),t.bigfont.c_str(),t.height,t.width,t.oblique,t.tracking,t.backward,t.upsideDown,t.vertical,t.over,t.under,t.strike,t.styleName.c_str());
            if(t.fontStatus==Acad::eOk && !t.typeface.isEmpty())require(style.setFont(t.typeface.constPtr(),t.bold,t.italic,t.charset,t.pitch,t.family)==Acad::eOk,"reconstruct public font failed");
            const int load=style.loadStyleRec(&s_.db);AcGePoint2d lo,hi;
            const auto status=(load&1)?style.extentsBox(t.text.c_str(),true,static_cast<int>(t.text.size()),t.raw,lo,hi,nullptr):Acad::eNotApplicable;
            // extentsBox is a string range, never glyph strokes. Context=null uses host default pipeline.
            Fields f={{"callback",std::to_string(t.callback)},{"loadFlags",std::to_string(load)},
              {"requestedFont",quote(t.font)},{"requestedBigfont",quote(t.bigfont)},
              {"resolvedFont",quote(ws(style.fileName()))},{"resolvedBigfont",quote(ws(style.bigFontFileName()))},
              {"status",std::to_string(status)},{"kind",q("stringRangeNotGlyph")},{"renderVerified","false"}};
            auto fontFile=[&](const ACHAR* name){AcString path;const auto find=(name&&*name)?acdbHostApplicationServices()->findFile(path,name,&s_.db,AcDbHostApplicationServices::kFontFile):Acad::eInvalidInput;
                Fields file={{"findStatus",std::to_string(find)},{"path",quote(ws(path.constPtr()))}};
                if(find==Acad::eOk&&!path.isEmpty())file.push_back({"sha256",quote(sha256(ws(path.constPtr())))});
                return object(file);};
            f.push_back({"fontFile",fontFile(style.fileName())});
            if(style.bigFontFileName()&&*style.bigFontFileName())f.push_back({"bigfontFile",fontFile(style.bigFontFileName())});
            if(status==Acad::eOk){
                f.push_back({"min",array({number(lo.x),number(lo.y)})});f.push_back({"max",array({number(hi.x),number(hi.y)})});
                if(t.normal.length()>1e-12 && t.direction.length()>1e-12 && std::abs(t.normal.normal().dotProduct(t.direction.normal()))<1e-8){
                    auto x=t.direction.normal(),y=t.normal.normal().crossProduct(x);std::vector<std::string> corners;
                    for(const auto& p:std::vector<AcGePoint2d>{{lo.x,lo.y},{hi.x,lo.y},{hi.x,hi.y},{lo.x,hi.y}})
                        corners.push_back(pt(t.matrix*(t.position+x*p.x+y*p.y)));
                    f.push_back({"worldCorners",array(corners)});
                }else s_.limit("textBasisInvalid");
            }else {require((load&1)==0,"loaded font extentsBox returned hard error status="+std::to_string(status));s_.limit("extentsBoxSkippedFontNotLoaded");}
            if((load&1)==0 || (load&30)!=0)s_.limit("fontLoadMissingOrFallback");
            s_.result.metrics.push_back(object(f));
        }
    }
private:State& s_;mutable Traits traits_;Context context_;mutable Geometry geometry_;
};
Adesk::Boolean Geometry::draw(AcGiDrawable* d) const {return w_.enter(d);}
static_assert(!std::is_abstract<World>::value && !std::is_abstract<Geometry>::value && !std::is_abstract<Traits>::value && !std::is_abstract<Context>::value,"2020 interfaces must be concrete");
class ProtocolDrawable final : public AcGiDrawable {
public:
    ProtocolDrawable* child{}; bool needViewport{},attributeViewportOnly{};unsigned worldCalls{},viewportCalls{};
    bool isPersistent() const override {return false;}
    AcDbObjectId id() const override {return AcDbObjectId::kNull;}
protected:
    Adesk::UInt32 subSetAttributes(AcGiDrawableTraits* t) override {t->setColor(3);return attributeViewportOnly?kDrawableViewIndependentViewportDraw:0;}
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* w) override {
        ++worldCalls;
        if(child){AcGeMatrix3d m; m.setToTranslation(AcGeVector3d(10,20,0));w->geometry().pushModelTransform(m);w->geometry().draw(child);w->geometry().popModelTransform();}
        else {AcGePoint3d points[]={{1,2,0},{3,4,0}};w->geometry().polyline(2,points);
            AcGiTextStyle style(L"simplex.shx",L"gbcbig.shx",3,0.8,0,1,false,false,false,false,false);
            const wchar_t text[]={L'1',L'2',L'3',L'X',L'Y'};
            w->geometry().text({1,2,0},AcGeVector3d::kZAxis,AcGeVector3d::kXAxis,text,3,true,style);
            w->geometry().text({1,6,0},AcGeVector3d::kZAxis,AcGeVector3d::kXAxis,2,1,0,L"short");
            const wchar_t chinese[]={0x4E2D,0x6587,L'X',L'Y'};
            w->geometry().text({1,10,0},AcGeVector3d::kZAxis,AcGeVector3d::kXAxis,chinese,2,true,style);}
        return !needViewport;
    }
    void subViewportDraw(AcGiViewportDraw*) override {++viewportCalls;}
};
}
std::string Result::json() const {
    return object({{"requestComplete",coverage.requestComplete()?"true":"false"},{"glyphComplete","false"},
      {"callbackCount",std::to_string(coverage.callbacks)},{"textCount",std::to_string(textCount)},
      {"drawRequests",std::to_string(coverage.drawRequests)},{"drawsEntered",std::to_string(coverage.drawsEntered)},
      {"viewportRequired",std::to_string(coverage.viewportRequired)},{"unknown",std::to_string(coverage.unknown)},
      {"limits",array(limits)},{"textRequests",array(textRequests)},{"stringRanges",array(metrics)}});
}
Result capture(AcGiDrawable& entity,AcDbDatabase& db,const std::function<void(const std::string&)>& log){
    State state(db,log);World world(state);world.enter(&entity);state.result.coverage.requireSafe();
    world.metrics();state.result.observation.coverage=state.result.coverage;return std::move(state.result);
}
Result controlProtocol(AcDbDatabase& db,const std::function<void(const std::string&)>& log){
    State state(db,log);World world(state);ProtocolDrawable leaf,middle,root;middle.child=&leaf;root.child=&middle;
    root.needViewport=true;world.enter(&root);
    require(state.result.coverage.drawsEntered==3 && state.result.coverage.viewportRequired==1,"protocol recursion/viewport control failed");
    require(state.result.textRequests.size()==3 && state.result.textRequests[0].find("\"text\":\"123\"")!=std::string::npos,"explicit text slice failed");
    require(state.result.textRequests[0].find("\"worldPosition\":[21,42,0]")!=std::string::npos,"accumulated transform failed");
    require(state.result.textRequests[2].find("\"utf16\":[20013,25991]")!=std::string::npos,"explicit Unicode code-unit slice failed");
    require(!state.result.coverage.requestComplete(),"missing viewport/short style falsely complete");
    world.metrics();state.result.observation.coverage=state.result.coverage;return std::move(state.result);
}
Result controlViewportProtocol(AcDbDatabase& db,const std::function<void(const std::string&)>& log,bool nested){
    State state(db,log);World world(state);ProtocolDrawable root,child;
    if(nested){root.child=&child;child.attributeViewportOnly=true;}else root.attributeViewportOnly=true;
    world.enter(&root);
    require(state.result.coverage.viewportRequired==1&&!state.result.coverage.requestComplete(),"viewport-only flag did not block complete capture");
    require((nested?child.worldCalls:root.worldCalls)==0&&root.viewportCalls==0&&child.viewportCalls==0,"viewport-only drawable must not call worldDraw or invented viewport");
    require(state.result.coverage.drawsEntered==(nested?2u:1u)&&state.result.textCount==0,"viewport-only protocol unexpectedly elaborated geometry");
    state.result.observation.coverage=state.result.coverage;return std::move(state.result);
}
}
