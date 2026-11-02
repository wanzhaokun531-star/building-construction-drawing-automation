#pragma once
// Every public geometry call preserves the host call and return value.
// Logging is observation only; unrecorded geometry remains explicitly partial.
// Shared preflight seam: exact legacy predicate, exercised without rendering a foreign drawable.
void validateChildDrawable(Session& s,AcGiDrawable* d) {
    require(d,"null child drawable");const auto* entity=AcDbEntity::cast(d);
    require(!entity||!entity->database()||entity->database()==s.target,"child draw references protected external database");
}
template<class Base> class GeometryForward:public Base {
public:
    GeometryForward(Base& host,Session& state):g(host),s(state){}
    void getModelToWorldTransform(AcGeMatrix3d& m) const override {g.getModelToWorldTransform(m);}
    void getWorldToModelTransform(AcGeMatrix3d& m) const override {g.getWorldToModelTransform(m);}
    Adesk::Boolean pushModelTransform(const AcGeVector3d& n) override {s.emit(g,"pushNormal",{{"normal",vec(n)}});return g.pushModelTransform(n);}
    Adesk::Boolean pushModelTransform(const AcGeMatrix3d& m) override {s.emit(g,"pushModelTransform",{{"matrix",mat(m)}});return g.pushModelTransform(m);}
    Adesk::Boolean popModelTransform() override {s.emit(g,"popModelTransform");return g.popModelTransform();}
    AcGeMatrix3d pushPositionTransform(AcGiPositionTransformBehavior b,const AcGePoint3d& p) override {s.emit(g,"pushPosition3d",{{"behavior",std::to_string(b)},{"offset",pt(p)}});return g.pushPositionTransform(b,p);}
    AcGeMatrix3d pushPositionTransform(AcGiPositionTransformBehavior b,const AcGePoint2d& p) override {s.emit(g,"pushPosition2d");return g.pushPositionTransform(b,p);}
    AcGeMatrix3d pushScaleTransform(AcGiScaleTransformBehavior b,const AcGePoint3d& p) override {s.emit(g,"pushScale3d");return g.pushScaleTransform(b,p);}
    AcGeMatrix3d pushScaleTransform(AcGiScaleTransformBehavior b,const AcGePoint2d& p) override {s.emit(g,"pushScale2d");return g.pushScaleTransform(b,p);}
    AcGeMatrix3d pushOrientationTransform(AcGiOrientationTransformBehavior b) override {s.emit(g,"pushOrientation");return g.pushOrientationTransform(b);}
    Adesk::Boolean polyline(Adesk::UInt32 count,const AcGePoint3d* p,const AcGeVector3d* n,Adesk::LongPtr marker) const override {s.points(g,"polyline",count,p);return g.polyline(count,p,n,marker);}
    Adesk::Boolean polyline(const AcGiPolyline& p) const override {s.points(g,"AcGiPolyline",p.points(),p.vertexList());return g.polyline(p);}
    Adesk::Boolean circularArc(const AcGePoint3d& c,double r,const AcGeVector3d& n,const AcGeVector3d& v,double sweep,AcGiArcType type) const override {
        AcGeMatrix3d m;g.getModelToWorldTransform(m);PrimitiveObservation p;p.kind="circularArc";p.center=triple(m*c);p.normal=triple(m*n);p.startVector=triple(m*v);p.radius=r;p.sweep=sweep;if(auto* o=s.observation())o->primitives.push_back(p);
        s.emit(g,"circularArc",{{"center",pt(c)},{"radius",number(r)},{"normal",vec(n)},{"startVector",vec(v)},{"sweep",number(sweep)},{"arcType",std::to_string(type)}});return g.circularArc(c,r,n,v,sweep,type);}
    Adesk::Boolean text(const AcGePoint3d& p,const AcGeVector3d& n,const AcGeVector3d& d,double h,double w,double o,const ACHAR* msg) const override {s.text(g,p,n,d,msg,-1,false,nullptr,h,w,o);return g.text(p,n,d,h,w,o,msg);}
    Adesk::Boolean text(const AcGePoint3d& p,const AcGeVector3d& n,const AcGeVector3d& d,const ACHAR* msg,Adesk::Int32 len,Adesk::Boolean raw,const AcGiTextStyle& style) const override {s.text(g,p,n,d,msg,len,raw,&style,style.textSize(),style.xScale(),style.obliquingAngle());return g.text(p,n,d,msg,len,raw,style);}
#define N7_FWD(name,args,call) Adesk::Boolean name args const override {s.emit(g,#name);return g.name call;}
    N7_FWD(circle,(const AcGePoint3d& p,double r,const AcGeVector3d& n),(p,r,n))
    N7_FWD(circle,(const AcGePoint3d& a,const AcGePoint3d& b,const AcGePoint3d& c),(a,b,c))
    N7_FWD(circularArc,(const AcGePoint3d& a,const AcGePoint3d& b,const AcGePoint3d& c,AcGiArcType t),(a,b,c,t))
    N7_FWD(polyPolyline,(Adesk::UInt32 n,const AcGiPolyline* p),(n,p))
    N7_FWD(polygon,(Adesk::UInt32 n,const AcGePoint3d* p),(n,p))
    N7_FWD(polyPolygon,(Adesk::UInt32 n,const Adesk::UInt32* a,const AcGePoint3d* b,const Adesk::UInt32* c,const AcGePoint3d* d,const AcCmEntityColor* e,const AcGiLineType* f,const AcCmEntityColor* h,const AcCmTransparency* i),(n,a,b,c,d,e,f,h,i))
    N7_FWD(mesh,(Adesk::UInt32 a,Adesk::UInt32 b,const AcGePoint3d* c,const AcGiEdgeData* d,const AcGiFaceData* e,const AcGiVertexData* f,bool h),(a,b,c,d,e,f,h))
    N7_FWD(shell,(Adesk::UInt32 a,const AcGePoint3d* b,Adesk::UInt32 c,const Adesk::Int32* d,const AcGiEdgeData* e,const AcGiFaceData* f,const AcGiVertexData* h,const resbuf* i,bool j),(a,b,c,d,e,f,h,i,j))
    N7_FWD(xline,(const AcGePoint3d& a,const AcGePoint3d& b),(a,b))
    N7_FWD(ray,(const AcGePoint3d& a,const AcGePoint3d& b),(a,b))
    N7_FWD(pline,(const AcDbPolyline& a,Adesk::UInt32 b,Adesk::UInt32 c),(a,b,c))
    N7_FWD(image,(const AcGiImageBGRA32& a,const AcGePoint3d& b,const AcGeVector3d& c,const AcGeVector3d& d,AcGiGeometry::TransparencyMode t),(a,b,c,d,t))
    N7_FWD(rowOfDots,(int a,const AcGePoint3d& b,const AcGeVector3d& c),(a,b,c))
    N7_FWD(ellipticalArc,(const AcGePoint3d& a,const AcGeVector3d& b,double c,double d,double e,double f,double h,AcGiArcType t),(a,b,c,d,e,f,h,t))
    N7_FWD(polypoint,(Adesk::UInt32 a,const AcGePoint3d* b,const AcGeVector3d* c,const Adesk::LongPtr* d),(a,b,c,d))
    N7_FWD(ownerDraw,(AcGiGdiDrawObject* a,const AcGePoint3d& b,const AcGeVector3d& c,const AcGeVector3d& d),(a,b,c,d))
    N7_FWD(edge,(const AcArray<AcGeCurve2d*>& e),(e))
#undef N7_FWD
    Adesk::Boolean draw(AcGiDrawable* d) const override {
        validateChildDrawable(s,d);
        s.emit(g,"draw",{{"class",quote(std::wstring(d->isA()->name()))},{"persistent",d->isPersistent()?"true":"false"}});
        s.authorizedTransient.push_back(d);struct Pop {Session& state;~Pop(){state.authorizedTransient.pop_back();}} pop{s};
        return g.draw(d);
    }
    Adesk::Boolean worldLine(const AcGePoint3d p[2]) override {s.points(g,"worldLine",2,p);return g.worldLine(p);}
    Adesk::Boolean pushClipBoundary(AcGiClipBoundary* c) override {require(c,"null clip");std::vector<std::string> p;require(c->m_aptPoints.length()<100000,"clip budget");for(int i=0;i<c->m_aptPoints.length();++i)p.push_back(array({number(c->m_aptPoints[i].x),number(c->m_aptPoints[i].y)}));s.emit(g,"pushClip",{{"points",array(p)},{"toClipSpace",mat(c->m_xToClipSpace)},{"inverseBlock",mat(c->m_xInverseBlockRefXForm)},{"clipAppliedToCollectedPrimitives","false"}});return g.pushClipBoundary(c);}
    void popClipBoundary() override {s.emit(g,"popClip");g.popClipBoundary();}
protected: Base& g;Session& s;
};
class WorldGeometry final:public GeometryForward<AcGiWorldGeometry> {
public: using GeometryForward::GeometryForward;
    void setExtents(AcGePoint3d* p) const override {s.emit(g,"setExtentsNotGlyph");g.setExtents(p);}
    void startAttributesSegment() override {s.emit(g,"startAttributesSegment");g.startAttributesSegment();}
};
class ViewportGeometry final:public GeometryForward<AcGiViewportGeometry> {
public:using GeometryForward::GeometryForward;
#define N7_VIEW(name) Adesk::Boolean name(Adesk::UInt32 n,const AcGePoint3d* p) const override {s.points(g,#name,n,p);return g.name(n,p);}
    N7_VIEW(polylineEye) N7_VIEW(polygonEye) N7_VIEW(polylineDc) N7_VIEW(polygonDc)
#undef N7_VIEW
    Adesk::Boolean rasterImageDc(const AcGePoint3d& a,const AcGeVector3d& b,const AcGeVector3d& c,const AcGeMatrix2d& d,AcDbObjectId e,AcGiImageOrg f,Adesk::UInt32 h,Adesk::UInt32 i,Adesk::Int16 j,Adesk::Boolean k,ImageSource l,const AcGeVector3d& m,AcGiImageOrg n,const AcGeMatrix2d& o,Adesk::UInt32 p,Adesk::UInt32 r) const override {s.emit(g,"rasterImageDc");return g.rasterImageDc(a,b,c,d,e,f,h,i,j,k,l,m,n,o,p,r);}
    Adesk::Boolean ownerDrawDc(Adesk::Int32 a,Adesk::Int32 b,Adesk::Int32 c,Adesk::Int32 d,Adesk::Int32 e,const OwnerDraw* f) const override {s.emit(g,"ownerDrawDc");return g.ownerDrawDc(a,b,c,d,e,f);}
    Adesk::Boolean ownerDraw3d(AcGePoint3d& a,AcGePoint3d& b,OwnerDraw3d* c) const override {s.emit(g,"ownerDraw3d");return g.ownerDraw3d(a,b,c);}
};
template<class Base,class Geo>class DrawForward:public Base {
public:
    DrawForward(Base& d,Session& s):host(d),geometry_(d.geometry(),s){}
    AcGiRegenType regenType() const override{return host.regenType();}
    Adesk::Boolean regenAbort() const override{return host.regenAbort();}
    AcGiSubEntityTraits& subEntityTraits() const override{return host.subEntityTraits();}
    AcGiGeometry* rawGeometry() const override{return &geometry_;}
    Adesk::Boolean isDragging() const override{return host.isDragging();}
    double deviation(AcGiDeviationType t,const AcGePoint3d& p) const override{return host.deviation(t,p);}
    Adesk::UInt32 numberOfIsolines() const override{return host.numberOfIsolines();}
    AcGiContext* context() override{return host.context();}
    bool secondaryCall() const override{return host.secondaryCall();}
    Geo& geometry() const override{return geometry_;}
protected:Base& host;mutable Geo geometry_;
};
using WorldDraw=DrawForward<AcGiWorldDraw,WorldGeometry>;
class ViewportDraw final:public DrawForward<AcGiViewportDraw,ViewportGeometry> {
public:using DrawForward::DrawForward;
    AcGiViewport& viewport() const override{return host.viewport();}
    Adesk::UInt32 sequenceNumber() const override{return host.sequenceNumber();}
    Adesk::Boolean isValidId(Adesk::ULongPtr id) const override{return host.isValidId(id);}
    AcDbObjectId viewportObjectId() const override{return host.viewportObjectId();}
};
