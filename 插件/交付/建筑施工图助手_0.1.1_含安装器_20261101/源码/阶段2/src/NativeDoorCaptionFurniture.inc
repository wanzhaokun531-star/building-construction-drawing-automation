// Read actual visible furniture/fixture ink; never alter or clone it into the DWG.
struct CaptionFurniturePart {bool area=false;InkBox box;bcpdoor::Stroke stroke;};
struct CaptionFurniture {std::string root;InkBox box;std::vector<CaptionFurniturePart> parts;};
InkBox captionFurnitureBounds(const AcDbEntity& e){AcDbExtents b;status(e.getGeomExtents(b),"furniture display bounds");return {b.minPoint().x,b.minPoint().y,b.maxPoint().x,b.maxPoint().y};}
bool captionFurnitureVisible(const AcDbEntity& e){if(e.visibility()!=AcDb::kVisible)return false;if(!e.layerId().isNull()){auto layer=open<AcDbLayerTableRecord>(e.layerId());if(layer->isOff()||layer->isFrozen())return false;}return true;}
void captionFurnitureInk(const AcDbEntity& e,std::vector<CaptionFurniturePart>& result,int depth=0){
 require(depth<12&&result.size()<50000,"furniture nesting/primitive budget");if(!captionFurnitureVisible(e))return;
 CaptionFurniturePart part;part.box=captionFurnitureBounds(e);
 if(AcDbLine::cast(&e)){std::vector<bcpdoor::Stroke> strokes;doorStrokes(e,strokes);part.stroke=strokes.at(0);result.push_back(part);return;}
 if(const auto* a=AcDbArc::cast(&e)){if(std::abs(std::abs(a->normal().z)-1)<1e-6){std::vector<bcpdoor::Stroke> strokes;doorStrokes(e,strokes);part.stroke=strokes.at(0);result.push_back(part);return;}}
 if(const auto* c=AcDbCircle::cast(&e)){if(std::abs(std::abs(c->normal().z)-1)<1e-6){part.stroke.arc=true;part.stroke.cx=c->center().x;part.stroke.cy=c->center().y;part.stroke.radius=c->radius();part.stroke.sweep=6.283185307179586;result.push_back(part);return;}}
 // Text, filled faces, solids, ellipses and opaque non-explodable objects use
 // conservative projected occupancy; open polylines/blocks use actual pieces.
 if(AcDbText::cast(&e)||AcDbMText::cast(&e)||AcDbSolid::cast(&e)){part.area=true;result.push_back(part);return;}
 TemplateParts children(&e);const auto exploded=e.explode(children.values);
 if(exploded!=Acad::eOk||children.values.isEmpty()){part.area=true;result.push_back(part);return;}
 std::set<const void*> seen;for(int i=0;i<children.values.length();++i){auto* child=AcDbEntity::cast(static_cast<AcRxObject*>(children.values[i]));require(child&&child!=&e&&child->database()==nullptr&&child->objectId().isNull()&&seen.insert(child).second,"furniture transient ownership");captionFurnitureInk(*child,result,depth+1);}
}
std::vector<CaptionFurniture> captionFurniture(AcDbDatabase& db){
 std::vector<CaptionFurniture> result;for(const auto& item:modelTypes(db)){auto e=open<AcDbEntity>(find(db,ascii(item.first)));const auto layer=symbolName(e->layerId());if(layer!=L"A-家具"&&layer!=L"A-洁具、厨具")continue;if(!captionFurnitureVisible(*e))continue;CaptionFurniture f;f.root=ascii(item.first);f.box=captionFurnitureBounds(*e);captionFurnitureInk(*e,f.parts);result.push_back(std::move(f));}return result;
}
bool captionFurnitureHit(const InkBox& box,const std::vector<CaptionFurniture>& furniture){
 const double gap=60;const auto overlaps=[&](const InkBox& b){return box.xmin<b.xmax+gap&&box.xmax>b.xmin-gap&&box.ymin<b.ymax+gap&&box.ymax>b.ymin-gap;};
 const bcpdoor::Box expanded{box.xmin-gap,box.ymin-gap,box.xmax+gap,box.ymax+gap};
 for(const auto& f:furniture)if(overlaps(f.box))for(const auto& p:f.parts)if(overlaps(p.box)&&(p.area||bcpdoor::hits(expanded,p.stroke)))return true;return false;
}
std::string captionFurnitureJson(const std::vector<CaptionFurniture>& furniture){
 std::ostringstream out;out<<std::setprecision(17)<<'[';for(std::size_t i=0;i<furniture.size();++i){if(i)out<<',';const auto& f=furniture[i];out<<"{\"root\":\""<<f.root<<"\",\"bounds\":"<<doorBoxJson(f.box)<<",\"parts\":[";for(std::size_t j=0;j<f.parts.size();++j){if(j)out<<',';const auto& p=f.parts[j];const auto& s=p.stroke;out<<"{\"area\":"<<(p.area?"true":"false")<<",\"bounds\":"<<doorBoxJson(p.box)<<",\"arc\":"<<(s.arc?"true":"false")<<",\"a\":["<<s.x1<<','<<s.y1<<"],\"b\":["<<s.x2<<','<<s.y2<<"],\"center\":["<<s.cx<<','<<s.cy<<"],\"radius\":"<<s.radius<<",\"angle\":"<<s.angle<<",\"sweep\":"<<s.sweep<<'}';}out<<"]}";}out<<']';return out.str();
}
