// Included only by this isolated native probe, inside its private namespace.
constexpr bool mappingDiagnosticOnly = false;
std::string truth(bool value) { return value ? "true" : "false"; }
n8a::Point plain(const AcGePoint3d& p) { return {p.x,p.y,p.z}; }
n8a::Point plain(const AcGeVector3d& p) { return {p.x,p.y,p.z}; }
AcGePoint3d native_point(n8a::Point p) { return {p.x,p.y,p.z}; }
std::string textstyle_shape(AcDbObjectId id) {
    if(id.isNull())return "null";auto style=opened<AcDbTextStyleTableRecord>(id);AcString font,bigfont;
    check(style->fileName(font),"effective style font");check(style->bigFontFileName(bigfont),"effective style bigfont");
    return object({{"font",text(font.kACharPtr())},{"bigfont",text(bigfont.kACharPtr())},{"widthFactor",number(style->xScale())},{"oblique",number(style->obliquingAngle())},{"vertical",truth(style->isVertical()!=0)},{"textSize",number(style->textSize())},{"statusGetters", "0"}});
}
std::string style_record(AcDbObjectId id) {
    if(id.isNull())return "null";
    auto style=opened<AcDbTextStyleTableRecord>(id);AcString name,font,bigfont;
    check(style->getName(name),"style name");check(style->fileName(font),"style font");check(style->bigFontFileName(bigfont),"style bigfont");
    return object({{"id",handle(id)},{"name",text(name.kACharPtr())},{"font",text(font.kACharPtr())},{"bigfont",text(bigfont.kACharPtr())},
        {"textSize",number(style->textSize())},{"widthFactor",number(style->xScale())},{"oblique",number(style->obliquingAngle())},
        {"vertical",truth(style->isVertical()!=0)},{"shapeFile",truth(style->isShapeFile()!=0)}});
}
template<class Dim> std::string dim_values(const Dim& dim) {
    return object({{"dimlfac",number(dim.dimlfac())},{"dimdec",std::to_string(dim.dimdec())},{"dimscale",number(dim.dimscale())},
        {"dimtxt",number(dim.dimtxt())},{"dimasz",number(dim.dimasz())},{"dimexo",number(dim.dimexo())},{"dimexe",number(dim.dimexe())},
        {"dimgap",number(dim.dimgap())},{"dimtad",std::to_string(dim.dimtad())},{"dimjust",std::to_string(dim.dimjust())},{"dimtxsty",handle(dim.dimtxsty())}});
}
std::string dimstyle_record(AcDbObjectId id) {
    if(id.isNull())return "null";auto style=opened<AcDbDimStyleTableRecord>(id);AcString name;check(style->getName(name),"dimstyle name");
    return object({{"id",handle(id)},{"name",text(name.kACharPtr())},{"declaredPublicValues",dim_values(*style)},
        {"textStyle",style_record(style->dimtxsty())}});
}
std::string dimension_resources(const AcDbDimension& dimension) {
    auto base=opened<AcDbDimStyleTableRecord>(dimension.dimensionStyle());
    return object({{"baseDimStyle",dimstyle_record(dimension.dimensionStyle())},{"effectiveTextStyle",style_record(dimension.dimtxsty())},
        {"effectiveValues",dim_values(dimension)},{"effectiveDiffersFromBase",truth(dim_values(dimension)!=dim_values(*base))},
        {"coverage",quote(std::string("Only the explicitly listed public variables and resources; not all TCH fields or all DimStyle variables"))}});
}
std::string public_dimension_text(const AcDbDimension& dimension) {
    std::vector<std::string> texts;const auto blockId=dimension.dimBlockId();
    if(!blockId.isNull()) {
        auto block=opened<AcDbBlockTableRecord>(blockId);AcDbBlockTableRecordIterator* raw{};check(block->newIterator(raw),"public dim block iterator");
        std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);
        for(;!it->done();it->step()) {AcDbObjectId id;check(it->getEntityId(id),"public dim block child");auto child=opened<AcDbEntity>(id);
            if(const auto* t=AcDbText::cast(child.get())){AcString content;check(t->textString(content),"public AcDbText string");texts.push_back(object({{"class",text(t->isA()->name())},{"text",text(content.kACharPtr())},{"height",number(t->height())},{"widthFactor",number(t->widthFactor())},{"position",point(t->position())},{"style",style_record(t->textStyle())}}));}
            else if(const auto* multi=AcDbMText::cast(child.get())){AcString content;check(multi->contents(content),"public MText contents");texts.push_back(object({{"class",text(multi->isA()->name())},{"contents",text(content.kACharPtr())},{"height",number(multi->textHeight())},{"position",point(multi->location())},{"style",style_record(multi->textStyle())}}));}
        }
    }
    return object({{"dimBlockId",handle(blockId)},{"texts",array(texts)},{"status",quote(std::string(texts.empty()?"no_public_text_host_request_missing":"public_dimblock_text_observed_host_request_still_missing"))}});
}
std::string layer_record(AcDbObjectId id) {const auto layer=layer_observation(id);return object({{"id",handle(id)},{"name",layer.name},{"definition",layer.definition},{"linetypeId",handle(layer.linetype)}});}
std::string linetype_record(AcDbObjectId id) {
    auto type=opened<AcDbLinetypeTableRecord>(id);AcString name;check(type->getName(name),"linetype name");std::vector<std::string> dashes;
    for(int i=0;i<type->numDashes();++i)dashes.push_back(number(type->dashLengthAt(i)));
    return object({{"id",handle(id)},{"name",text(name.kACharPtr())},{"patternLength",number(type->patternLength())},{"dashLengths",array(dashes)}});
}
template<class F> std::string table_records(AcDbObjectId id,F f) {
    auto table=opened<AcDbSymbolTable>(id);AcDbSymbolTableIterator* raw{};check(table->newIterator(raw),"symbol table iterator");std::unique_ptr<AcDbSymbolTableIterator> it(raw);std::vector<std::string> rows;
    for(;!it->done();it->step()){AcDbObjectId record;check(it->getRecordId(record),"symbol record id");rows.push_back(f(record));}return array(rows);
}
std::vector<AcDbObjectId> model_roots(AcDbDatabase& db) {
    auto model=opened<AcDbBlockTableRecord>(acdbSymUtil()->blockModelSpaceId(&db));AcDbBlockTableRecordIterator* raw{};check(model->newIterator(raw),"model iterator");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);std::vector<AcDbObjectId> ids;
    for(;!it->done();it->step()){AcDbObjectId id;check(it->getEntityId(id),"model root id");ids.push_back(id);}return ids;
}
std::string symbol_identity(AcDbObjectId id){auto value=opened<AcDbSymbolTableRecord>(id);AcString name;check(value->getName(name),"protected symbol key");return object({{"id",handle(id)},{"ownerId",handle(value->ownerId())},{"class",text(value->isA()->name())},{"key",text(name.kACharPtr())}});}
std::string database_state(AcDbDatabase& db,bool protectedIdentityOnly=false) {
    Working working(&db);std::vector<std::string> roots;
    for(const auto id:model_roots(db)){auto root=opened<AcDbEntity>(id);Fields fields{{"id",handle(id)},{"class",text(root->isA()->name())},{"owner",handle(root->ownerId())},{"layer",handle(root->layerId())},{"linetype",handle(root->linetypeId())},{"colorIndex",std::to_string(root->colorIndex())},{"lineWeight",std::to_string(root->lineWeight())},{"visible",std::to_string(root->visibility())}};
        if(!protectedIdentityOnly){AcGePoint3dArray grips;AcDbIntArray osnap,geometry;const auto status=root->getGripPoints(grips,osnap,geometry);std::vector<std::string> points;if(status==Acad::eOk)for(int i=0;i<grips.length();++i)points.push_back(point(grips[i]));fields.emplace_back("gripStatus",std::to_string(status));fields.emplace_back("grips",array(points));}
        if(const auto* t=AcDbText::cast(root.get())){AcString content;check(t->textString(content),"state text");fields.emplace_back("text",text(content.kACharPtr()));fields.emplace_back("position",point(t->position()));fields.emplace_back("alignment",point(t->alignmentPoint()));fields.emplace_back("height",number(t->height()));fields.emplace_back("widthFactor",number(t->widthFactor()));fields.emplace_back("rotation",number(t->rotation()));fields.emplace_back("style",protectedIdentityOnly?symbol_identity(t->textStyle()):style_record(t->textStyle()));}
        roots.push_back(object(fields));}
    return object({{"readPolicy",quote(std::string(protectedIdentityOnly?"protected_common_identity_and_symbol_owner_keys_no_grips_explode_fonts":"owned_full_declared_public_observations"))},{"modelRoots",array(roots)},{"insunits",std::to_string(db.insunits())},
        {"layers",table_records(db.layerTableId(),protectedIdentityOnly?symbol_identity:layer_record)},
        {"textStyles",table_records(db.textStyleTableId(),protectedIdentityOnly?symbol_identity:style_record)},
        {"linetypes",table_records(db.linetypeTableId(),protectedIdentityOnly?symbol_identity:linetype_record)},
        {"dimStyles",table_records(db.dimStyleTableId(),protectedIdentityOnly?symbol_identity:dimstyle_record)}});
}
struct WriteMonitor final:AcDbDatabaseReactor {
    AcDbDatabase& db;unsigned opened{},modified{},appended{},erased{},unappended{},reappended{},header{};
    explicit WriteMonitor(AcDbDatabase& value):db(value){db.addReactor(this);}~WriteMonitor() noexcept {db.removeReactor(this);}
    void objectOpenedForModify(const AcDbDatabase*,const AcDbObject*)override{++opened;}
    void objectModified(const AcDbDatabase*,const AcDbObject*)override{++modified;}
    void objectAppended(const AcDbDatabase*,const AcDbObject*)override{++appended;}
    void objectErased(const AcDbDatabase*,const AcDbObject*,Adesk::Boolean)override{++erased;}
    void objectUnAppended(const AcDbDatabase*,const AcDbObject*)override{++unappended;}
    void objectReAppended(const AcDbDatabase*,const AcDbObject*)override{++reappended;}
    void headerSysVarWillChange(const AcDbDatabase*,const ACHAR*)override{++header;}
    bool clean()const{return opened+modified+appended+erased+unappended+reappended+header==0;}
    std::string json()const{return object({{"openedForModify",std::to_string(opened)},{"modified",std::to_string(modified)},{"appended",std::to_string(appended)},{"erased",std::to_string(erased)},{"unappended",std::to_string(unappended)},{"reappended",std::to_string(reappended)},{"headerWillChange",std::to_string(header)}});}
};
void report_file(const Config& config,const std::wstring& name,const std::string& json){auto file=create_new(config.output+L"\\"+name);write_flushed(file.get(),json);}
struct Fixture {
    std::string name;std::wstring seed;int segmentCount{};AcGePoint3d origin;AcGeVector3d direction;
    std::vector<double> stations;double lateral{},scaleDenominator{100},unitToMm{1};std::string unit{"mm"},unitSource{"explicit project fixture"},placementSource{"explicit per-chain fixture"};bool noncollinear{};
    std::vector<AcGePoint3d> anchors()const{std::vector<AcGePoint3d> points;for(auto coordinate:stations)points.push_back(origin+direction*coordinate);if(noncollinear&&points.size()>2)points[1]+=AcGeVector3d(-direction.y,direction.x,0)*37.25;return points;}
    std::string json()const{std::vector<std::string> coordinates,points,projected;for(auto coordinate:stations){coordinates.push_back(number(coordinate));projected.push_back(point(origin+direction*coordinate));}for(const auto& p:anchors())points.push_back(point(p));return object({{"name",quote(name)},{"seed",quote(seed)},{"segmentCount",std::to_string(segmentCount)},{"origin",point(origin)},{"direction",array({number(direction.x),number(direction.y),number(direction.z)})},{"coordinateInput",array(coordinates)},{"projectedPoints",array(projected)},{"sourcePoints",array(points)},{"lateralOffset",number(lateral)},{"baselinePoint",point(origin+AcGeVector3d(-direction.y,direction.x,0)*lateral)},{"unit",quote(unit)},{"unitSource",quote(unitSource)},{"scaleDenominator",number(scaleDenominator)},{"placementSource",quote(placementSource)},{"actualAnchorPolicy",quote(std::string("explicit fixture same-station source anchors; no projection replacement"))}});}
};
void validate_fixture(const Fixture& fixture) {
    require(!fixture.seed.empty(),"request_missing_seed");require(!fixture.placementSource.empty(),"request_missing_placement");require(!fixture.unitSource.empty(),"request_missing_unit_source");
    require(fixture.segmentCount>0&&fixture.stations.size()==static_cast<std::size_t>(fixture.segmentCount+1),"request_wrong_segment_count");
    require(std::abs(fixture.direction.length()-1)<=tolerance&&std::abs(fixture.direction.z)<=tolerance,"request_invalid_direction");
    require(fixture.unitToMm>0&&fixture.scaleDenominator>0,"request_invalid_unit_scale");
    const auto anchors=fixture.anchors();std::vector<double> dummy;
    for(std::size_t i=0;i<fixture.stations.size();++i){require(std::isfinite(fixture.stations[i]),"request_nonfinite_station");if(i)require(fixture.stations[i]>fixture.stations[i-1]+tolerance,"request_duplicate_or_descending_station");
        const auto projected=fixture.origin+fixture.direction*fixture.stations[i];const auto delta=anchors[i]-projected;require(std::abs(delta.dotProduct(fixture.direction))<=tolerance,"actual_anchor_station_projection_mismatch");dummy.push_back(fixture.stations[i]);}
}
std::vector<Fixture> fixtures(){return {
    {"two_mm100_horizontal",L"10837",2,{12000.375,-3400.625,0},{1,0,0},{0,512.375,3001.125},800.5},
    {"one_mm100_vertical_large",L"ACB0",1,{1000000.375,2000000.625,0},{0,1,0},{0,1876.625},-700.125},
    {"six_mm100_horizontal",L"AC83",6,{-45000.375,16000.625,0},{1,0,0},{0,135.375,630.125,1250.875,1999.25,3201.5,4450.625},900.375},
    {"twelve_mm100_vertical_large",L"CAF3",12,{2356789.125,-1456789.375,0},{0,1,0},{0,120.375,555.625,1110.875,1875.125,2720.375,3650.625,4700.875,5861.125,7130.375,8510.625,10000.875,12020.375},-1100.25},
    {"two_mm50_horizontal",L"10837",2,{14200.125,17200.375,0},{1,0,0},{0,710.625,3245.875},600.25,50},
    {"two_m50_horizontal",L"10837",2,{12.000375,-3.400625,0},{1,0,0},{0,.512375,3.001125},.40025,50,1000,"m"},
    {"two_mm100_noncollinear",L"10837",2,{22100.375,6400.625,0},{1,0,0},{0,512.375,3001.125},800.5,100,1,"mm","explicit project fixture","explicit per-chain fixture",true}
};}
n8a::Path topology(const Snapshot& snapshot,const AcGeVector3d& direction) {
    std::vector<n8a::Edge> edges;for(const auto& segment:snapshot.segments)edges.push_back({plain(segment.first),plain(segment.second)});
    auto result=n8a::path(edges,plain(direction));const AcGeVector3d normal(-direction.y,direction.x,0);
    const auto baseline=snapshot.segments.front().baseline;
    for(const auto& segment:snapshot.segments)require(std::abs((segment.baseline-baseline).dotProduct(normal))<=tolerance&&std::abs(segment.baseline.z-baseline.z)<=tolerance,"segment baselines are not one chain line");
    for(const auto node:result.nodes)require(std::abs(node.z-result.nodes.front().z)<=tolerance,"nonplanar node path");return result;
}
AcGeVector3d source_direction(const Snapshot& snapshot){require(snapshot.nodes.size()>=2,"source nodes missing");double greatest{};AcGeVector3d direction;
    for(const auto& a:snapshot.nodes)for(const auto& b:snapshot.nodes)if(a.distanceTo(b)>greatest){greatest=a.distanceTo(b);direction=b-a;}
    require(greatest>tolerance,"zero path span");direction.normalize();if(direction.x<-tolerance||(std::abs(direction.x)<=tolerance&&direction.y<0))direction.negate();return direction;}
std::vector<n8a::Point> plain_grips(const Snapshot& snapshot){std::vector<n8a::Point> points;for(const auto& p:snapshot.grips)points.push_back(plain(p));return points;}
void validate_topology_measurements(const Snapshot& snapshot,const n8a::Path& path,const AcGeVector3d& direction){for(std::size_t i=0;i<path.segments.size();++i){const auto expected=(native_point(path.nodes[i+1])-native_point(path.nodes[i])).dotProduct(direction);require(std::abs(snapshot.segments[path.segments[i]].measurement-expected)<=tolerance,"measurement_does_not_match_station_projection");}}
struct SceneLog {const Config& config;std::string name;unsigned sequence{};std::vector<std::string> actions;
    void attempt(const char* operation,Acad::ErrorStatus status,const Fields& details={}){auto fields=details;fields.emplace_back("operation",quote(std::string(operation)));fields.emplace_back("status",std::to_string(status));actions.push_back(object(fields));}
    void capture(const char* operation,Acad::ErrorStatus status,const Snapshot& value){++sequence;const auto filename=std::wstring(name.begin(),name.end())+L"-observation-"+std::to_wstring(sequence)+L".json";report_file(config,filename,object({{"operation",quote(std::string(operation))},{"status",std::to_string(status)},{"snapshot",value.json}}));actions.push_back(object({{"operation",quote(std::string(operation))},{"status",std::to_string(status)},{"observation",quote(filename)}}));}};
Acad::ErrorStatus move_node(AcDbObjectId root,std::size_t index,const AcGeVector3d& delta){auto object=opened<AcDbEntity>(root,AcDb::kForWrite);AcDbIntArray indices;indices.append(static_cast<int>(index));return object->moveGripPointsAt(indices,delta);}
Snapshot adapt(AcDbObjectId root,const Fixture& fixture,ObservationLog& observations,SceneLog& log) {
    auto current=observe(root,observations);const auto sourceDirection=source_direction(current);const auto originalPath=topology(current,sourceDirection);validate_topology_measurements(current,originalPath,sourceDirection);
    const double uniformScale=fixture.scaleDenominator/(100.0*fixture.unitToMm);
    const auto sourceOrigin=native_point(originalPath.nodes.front());const auto angle=std::atan2(fixture.direction.y,fixture.direction.x)-std::atan2(sourceDirection.y,sourceDirection.x);
    const auto matrix=AcGeMatrix3d::translation(fixture.origin.asVector())*AcGeMatrix3d::rotation(angle,AcGeVector3d::kZAxis)*AcGeMatrix3d::scaling(uniformScale)*AcGeMatrix3d::translation(-sourceOrigin.asVector());
    Acad::ErrorStatus status;{auto entity=opened<AcDbEntity>(root,AcDb::kForWrite);status=entity->transformBy(matrix);}log.attempt("transformBy",status,{{"uniformPaperScale",number(uniformScale)},{"rotationRadians",number(angle)}});check(status,"uniform seed paper/orientation transformBy");
    current=observe(root,observations);log.capture("uniformSeedPaperAndRigidPlacement",status,current);auto path=topology(current,fixture.direction);validate_topology_measurements(current,path,fixture.direction);
    require(path.nodes.size()==fixture.stations.size(),"transformed source/target node count mismatch");
    for(std::size_t i=0;i<path.nodes.size();++i)require(points_close(native_point(path.nodes[i]),matrix*native_point(originalPath.nodes[i])),"transform eOk but actual node differs");
    std::vector<double> sourceCoordinates;for(const auto p:path.nodes)sourceCoordinates.push_back((native_point(p)-fixture.origin).dotProduct(fixture.direction));
    const auto order=n8a::move_order(sourceCoordinates,fixture.stations);std::vector<AcGePoint3d> expected;for(const auto p:path.nodes)expected.push_back(native_point(p));const auto targets=fixture.anchors();
    // Include pure lateral changes even when the station coordinate itself is unchanged.
    auto completeOrder=order;for(std::size_t i=0;i<targets.size();++i)if(std::find(completeOrder.begin(),completeOrder.end(),i)==completeOrder.end()&&!points_close(expected[i],targets[i]))completeOrder.push_back(i);
    for(const auto node:completeOrder){current=observe(root,observations);path=topology(current,fixture.direction);const auto index=n8a::unique_grip(path.nodes[node],plain_grips(current));const auto delta=targets[node]-native_point(path.nodes[node]);const auto moveStatus=move_node(root,index,delta);log.attempt("moveGripPointsAtMeasurement",moveStatus,{{"pathNode",std::to_string(node)},{"actualMatchedGripIndex",std::to_string(index)},{"targetActualAnchor",point(targets[node])},{"offset",array({number(delta.x),number(delta.y),number(delta.z)})}});current=observe(root,observations);log.capture("moveMatchedMeasurementNode",moveStatus,current);check(moveStatus,"moveGripPointsAt measurement");const auto changed=topology(current,fixture.direction);expected[node]=targets[node];
        for(std::size_t i=0;i<expected.size();++i)require(points_close(native_point(changed.nodes[i]),expected[i]),"node move changed another actual measurement anchor");
        validate_topology_measurements(current,changed,fixture.direction);
    }
    current=observe(root,observations);path=topology(current,fixture.direction);const AcGeVector3d normal(-fixture.direction.y,fixture.direction.x,0);const auto targetBaseline=fixture.origin+normal*fixture.lateral;
    const double delta=(targetBaseline-current.segments.front().baseline).dotProduct(normal);
    std::vector<std::size_t> candidates;
    for(std::size_t i=0;i<current.grips.size();++i){const auto& p=current.grips[i];const bool measured=std::any_of(path.nodes.begin(),path.nodes.end(),[&](n8a::Point node){return points_close(p,native_point(node));});const double coordinate=(p-fixture.origin).dotProduct(fixture.direction);
        if(!measured&&std::abs((p-current.segments.front().baseline).dotProduct(normal))<=tolerance&&std::abs(p.z-targetBaseline.z)<=tolerance&&(std::abs(coordinate-fixture.stations.front())<=tolerance||std::abs(coordinate-fixture.stations.back())<=tolerance))candidates.push_back(i);}
    require(candidates.size()==2,"baseline semantics require two distinct endpoint grips on actual segment baseline");
    require(!points_close(current.grips[candidates[0]],current.grips[candidates[1]]),"ambiguous coincident baseline grips");
    const auto baselineStatus=move_node(root,candidates.front(),normal*delta);log.attempt("moveGripPointsAtBaseline",baselineStatus,{{"actualMatchedGripIndex",std::to_string(candidates.front())},{"normalOffset",number(delta)}});const auto baselineMoved=observe(root,observations);log.capture("moveMatchedBaselineOnly",baselineStatus,baselineMoved);check(baselineStatus,"moveGripPointsAt baseline");const auto finalPath=topology(baselineMoved,fixture.direction);
    for(std::size_t i=0;i<targets.size();++i)require(points_close(native_point(finalPath.nodes[i]),targets[i]),"baseline move changed actual source anchor");
    for(const auto& segment:baselineMoved.segments)require(std::abs((segment.baseline-targetBaseline).dotProduct(normal))<=tolerance,"baseline did not reach explicit placement");
    return baselineMoved;
}
struct FlatField {std::string path,raw;bool numeric{};double value{};};
// The scanner compares our own strictly generated JSON, preserving field paths.
// It never reads TCH serialization or treats private data as a business schema.
std::vector<FlatField> flatten(const std::string& json) {
    std::size_t at{};std::vector<FlatField> fields;
    const auto string_token=[&](){const auto begin=at;require(at<json.size()&&json[at++]=='"',"JSON string expected");bool escape=false;while(at<json.size()){const auto c=json[at++];if(c=='"'&&!escape)return json.substr(begin,at-begin);if(c=='\\'&&!escape)escape=true;else escape=false;}throw std::runtime_error("unterminated generated JSON string");};
    std::function<void(const std::string&)> parse;
    parse=[&](const std::string& path){require(at<json.size(),"truncated generated JSON");if(json[at]=='{'){++at;if(json[at]=='}'){++at;fields.push_back({path,"{}",false,0});return;}for(;;){auto key=string_token();require(json[at++]==':',"JSON colon expected");parse(path+"."+key.substr(1,key.size()-2));if(json[at]=='}'){++at;return;}require(json[at++]==',',"JSON object separator expected");}}
        else if(json[at]=='['){++at;if(json[at]==']'){++at;fields.push_back({path,"[]",false,0});return;}std::size_t index{};for(;;){parse(path+"["+std::to_string(index++)+"]");if(json[at]==']'){++at;return;}require(json[at++]==',',"JSON array separator expected");}}
        else if(json[at]=='"')fields.push_back({path,string_token(),false,0});
        else {const auto begin=at;while(at<json.size()&&json[at]!=','&&json[at]!=']'&&json[at]!='}')++at;const auto raw=json.substr(begin,at-begin);const bool numeric=!raw.empty()&&(raw[0]=='-'||(raw[0]>='0'&&raw[0]<='9'));char* end{};const double value=numeric?std::strtod(raw.c_str(),&end):0;require(!numeric||(end==raw.c_str()+raw.size()&&std::isfinite(value)),"invalid generated JSON number");fields.push_back({path,raw,numeric,value});}};
    parse("$");require(at==json.size(),"generated JSON trailing data");return fields;
}
struct FieldComparison {bool equal{};std::string json;};
FieldComparison compare_fields(const std::string& before,const std::string& after) {
    const auto first=flatten(before),second=flatten(after);bool equal=first.size()==second.size();std::vector<std::string> rows;
    const auto count=(std::max)(first.size(),second.size());
    for(std::size_t i=0;i<count;++i){const bool present=i<first.size()&&i<second.size();const bool same=present&&first[i].path==second[i].path&&first[i].numeric==second[i].numeric&&(first[i].numeric?std::abs(first[i].value-second[i].value)<=tolerance:first[i].raw==second[i].raw);equal=equal&&same;
        rows.push_back(object({{"beforePath",i<first.size()?quote(first[i].path):"null"},{"afterPath",i<second.size()?quote(second[i].path):"null"},{"before",i<first.size()?first[i].raw:"null"},{"after",i<second.size()?second[i].raw:"null"},{"equal",truth(same)}}));}
    return {equal,object({{"allDeclaredFieldsEqual",truth(equal)},{"numericToleranceModelUnits",number(tolerance)},{"beforeFieldCount",std::to_string(first.size())},{"afterFieldCount",std::to_string(second.size())},{"fields",array(rows)}})};
}
double formatted_number(const std::wstring& formatted) {const auto start=formatted.find_last_of(L';');const auto value=formatted.substr(start==std::wstring::npos?0:start+1);wchar_t* end{};const double parsed=std::wcstod(value.c_str(),&end);require(end==value.c_str()+value.size()&&end!=value.c_str()&&std::isfinite(parsed),"native_format_not_plain_numeric_after_controls");return parsed;}
void validate_measurements(const Snapshot& actual,const Fixture& fixture,const n8a::Path& path) {
    for(std::size_t i=0;i<path.segments.size();++i){const auto& part=actual.segments[path.segments[i]];const double expected=fixture.stations[i+1]-fixture.stations[i];require(std::abs(part.measurement-expected)<=tolerance,"native_measurement_mismatch");}
}
void validate_formats(const Snapshot& actual,const Fixture& fixture,const n8a::Path& path,double faultDelta=0) {
    for(std::size_t i=0;i<path.segments.size();++i){const auto& part=actual.segments[path.segments[i]];require(part.overrideText.empty(),"native_unexpected_dimension_text_override");require(std::abs(part.factor-fixture.unitToMm)<=tolerance,"native_display_factor_unsupported_no_public_root_dimlfac_method");const double expected=std::floor((fixture.stations[i+1]-fixture.stations[i])*fixture.unitToMm+.5)+faultDelta;require(std::abs(formatted_number(part.formatted)-expected)<=tolerance,"native_format_mismatch");}
}
std::string expect_reject(const std::string& name,const std::string& stage,const std::function<void()>& operation,const std::string& expected) {
    const auto message=n8a::expected_rejection(name,operation,expected);return object({{"name",quote(name)},{"reachedStage",quote(stage)},{"rejected", "true"},{"diagnostic",quote(message)},{"candidateReturned","false"},{"candidateSaved","false"}});
}
std::string formal_source_sha_rejection(const Config& config,Evidence& evidence);
AcDbObjectId clone_seed(AcDbDatabase& source,AcDbDatabase& candidate,AcDbObjectId seed,AcDbIdMapping& mapping) {
    require(&source!=&candidate && &candidate!=acDocManager->curDocument()->database(),"candidate must be independent owned database");AcDbObjectIdArray ids;ids.append(seed);
    check(source.wblockCloneObjects(ids,acdbSymUtil()->blockModelSpaceId(&candidate),mapping,AcDb::kDrcMangleName),"clone owned seed");AcDbIdPair pair;pair.setKey(seed);require(mapping.compute(pair)&&pair.isCloned()&&!pair.value().isNull()&&pair.value().database()==&candidate,"primary mapping invalid");return pair.value();
}
std::string reference_description(AcDbObjectId id,AcDbDatabase* expected) {
    if(id.isNull())return object({{"id","null"},{"class","null"},{"declaredDefinition","null"}});
    Working scope(id.database());auto value=opened<AcDbObject>(id);std::string definition="null";
    if(value->isA()==AcDbDimStyleTableRecord::desc())definition=dimstyle_record(id);
    else if(value->isA()==AcDbTextStyleTableRecord::desc())definition=style_record(id);
    else if(value->isA()==AcDbLayerTableRecord::desc())definition=layer_record(id);
    else if(value->isA()==AcDbLinetypeTableRecord::desc())definition=linetype_record(id);
    return object({{"id",handle(id)},{"class",text(value->isA()->name())},{"ownerId",handle(value->ownerId())},
        {"databaseNull",truth(value->database()==nullptr)},{"databaseMatchesExpected",truth(value->database()==expected)},
        {"declaredDefinition",definition}});
}
std::string sdk_map_records(AcDbIdMapping& mapping) {
    std::vector<std::string> pairs;AcDbIdMappingIter it(mapping);
    for(;!it.done();it.next()){AcDbIdPair pair;require(it.getMap(pair),"SDK map iterator failed");pairs.push_back(object({{"sourceId",handle(pair.key())},{"actualTargetId",handle(pair.value())},{"isCloned",truth(pair.isCloned())},{"isPrimary",truth(pair.isPrimary())},{"ownerTranslated",truth(pair.isOwnerXlated())}}));}
    return array(pairs);
}
struct ReferenceMapping {bool matches{};std::string json;};
struct SymbolProof {std::wstring key;std::string json;};
SymbolProof symbol_proof(AcDbObjectId id,AcDbDatabase& expected) {
    require(!id.isNull()&&id.database()==&expected,"derived_symbol_wrong_database_direction");Working scope(&expected);auto value=opened<AcDbSymbolTableRecord>(id);AcDbObjectId expectedOwner;
    if(value->isA()==AcDbDimStyleTableRecord::desc())expectedOwner=expected.dimStyleTableId();
    else if(value->isA()==AcDbTextStyleTableRecord::desc())expectedOwner=expected.textStyleTableId();
    else throw std::runtime_error("derived_symbol_unexpected_exact_class");
    require(value->ownerId()==expectedOwner,"derived_symbol_wrong_table_owner");AcString key;check(value->getName(key),"derived symbol key");auto table=opened<AcDbSymbolTable>(expectedOwner);AcDbObjectId found;const auto status=table->getAt(key.kACharPtr(),found);
    require(status==Acad::eOk&&found==id,"derived_symbol_table_key_lookup_mismatch");return {key.kACharPtr(),object({{"recordId",handle(id)},{"class",text(value->isA()->name())},{"ownerId",handle(value->ownerId())},{"expectedTableId",handle(expectedOwner)},{"tableClass",text(table->isA()->name())},{"key",text(key.kACharPtr())},{"getAtStatus",std::to_string(status)},{"getAtResult",handle(found)},{"correctDatabaseAndOwner", "true"}})};
}
template<class Dim> std::string dim_scalar_shape(const Dim& dim){return object({{"dimlfac",number(dim.dimlfac())},{"dimdec",std::to_string(dim.dimdec())},{"dimscale",number(dim.dimscale())},{"dimtxt",number(dim.dimtxt())},{"dimasz",number(dim.dimasz())},{"dimexo",number(dim.dimexo())},{"dimexe",number(dim.dimexe())},{"dimgap",number(dim.dimgap())},{"dimtad",std::to_string(dim.dimtad())},{"dimjust",std::to_string(dim.dimjust())}});}
std::string declared_symbol_shape(AcDbObjectId id){Working scope(id.database());auto value=opened<AcDbObject>(id);if(value->isA()==AcDbTextStyleTableRecord::desc()){auto* style=AcDbTextStyleTableRecord::cast(value.get());return object({{"textShape",textstyle_shape(id)},{"shapeFile",truth(style->isShapeFile()!=0)}});}if(value->isA()==AcDbDimStyleTableRecord::desc())return dim_scalar_shape(*AcDbDimStyleTableRecord::cast(value.get()));throw std::runtime_error("derived_symbol_unknown_definition");}
bool single_mangle_key(const std::wstring& source,const std::wstring& target){if(target.size()<4||target.front()!=L'$')return false;std::size_t at=1;while(at<target.size()&&target[at]>=L'0'&&target[at]<=L'9')++at;return at>1&&at<target.size()&&target[at]==L'$'&&target.substr(at+1)==source;}
struct DerivedBindings {
    AcDbDatabase& sourceDb;AcDbDatabase& targetDb;AcDbIdMapping& sdkMap;std::map<AcDbObjectId,AcDbObjectId> forward,reverse;
    DerivedBindings(AcDbDatabase& source,AcDbDatabase& target,AcDbIdMapping& mapping):sourceDb(source),targetDb(target),sdkMap(mapping){}
    ReferenceMapping bind(AcDbObjectId source,AcDbObjectId target,const std::string& role){
        AcDbIdPair pair;pair.setKey(source);const bool found=sdkMap.compute(pair),sdkMatches=found&&pair.value()==target;bool accepted=false;std::string diagnostic,sourceDefinition="null",targetDefinition="null",sourceBefore="null",targetBefore="null",sourceAfter="null",targetAfter="null",sourceShape="null",targetShape="null",nested="null";
        try{sourceDefinition=reference_description(source,&sourceDb);targetDefinition=reference_description(target,&targetDb);const auto first=symbol_proof(source,sourceDb),second=symbol_proof(target,targetDb);sourceBefore=first.json;targetBefore=second.json;auto sourceObject=opened<AcDbObject>(source);auto targetObject=opened<AcDbObject>(target);require(sourceObject->isA()==targetObject->isA(),"derived_symbol_exact_class_mismatch");
            if(found)require(sdkMatches,"derived_symbol_existing_sdk_pair_mismatch");else require(single_mangle_key(first.key,second.key),"derived_symbol_key_relation_mismatch");
            sourceShape=declared_symbol_shape(source);targetShape=declared_symbol_shape(target);require(sourceShape==targetShape,"derived_declared_definition_mismatch");
            const auto previous=forward.find(source);require(previous==forward.end()||previous->second==target,"derived_forward_binding_inconsistent");const auto inverse=reverse.find(target);require(inverse==reverse.end()||inverse->second==source,"derived_reverse_binding_inconsistent");
            if(auto* dim=AcDbDimStyleTableRecord::cast(sourceObject.get())){const auto child=bind(dim->dimtxsty(),AcDbDimStyleTableRecord::cast(targetObject.get())->dimtxsty(),role+".baseTextStyle");nested=child.json;require(child.matches,"derived_dimstyle_textstyle_binding_rejected");}
            sourceAfter=symbol_proof(source,sourceDb).json;targetAfter=symbol_proof(target,targetDb).json;require(sourceBefore==sourceAfter&&targetBefore==targetAfter,"derived_symbol_owner_key_changed_during_binding");forward[source]=target;reverse[target]=source;accepted=true;
        }catch(const std::exception& failure){diagnostic=failure.what();}
        return {accepted,object({{"role",quote(role)},{"bindingKind",quote(std::string("independent_public_explode_derived_role_binding_not_sdk_mapping"))},{"source",sourceDefinition},{"observedTarget",targetDefinition},{"sourceTableProofBefore",sourceBefore},{"targetTableProofBefore",targetBefore},{"sourceTableProofAfter",sourceAfter},{"targetTableProofAfter",targetAfter},{"sourceDeclaredShape",sourceShape},{"targetDeclaredShape",targetShape},{"nestedTextStyleBinding",nested},{"sdkComputeFound",truth(found)},{"sdkPairKey",handle(pair.key())},{"sdkPairValue",handle(pair.value())},{"sdkPairIsCloned",truth(pair.isCloned())},{"sdkPairIsPrimary",truth(pair.isPrimary())},{"sdkPairOwnerTranslated",truth(pair.isOwnerXlated())},{"observedTargetMatchesSdkPair",truth(sdkMatches)},{"singlePrefixRuleRequired",truth(!found)},{"acceptedDerivedBinding",truth(accepted)},{"diagnostic",quote(diagnostic)}})};
    }
};
struct SceneResult {bool supported{},setupPassed{},required{};std::string json;};
std::vector<std::string> binder_controls(const Config& config,ObservationLog& observations,Evidence& evidence) {
    auto scratch=std::make_unique<AcDbDatabase>(false,true);check(scratch->readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false),"binder control scratch read");check(scratch->closeInput(true),"binder control scratch close");WriteMonitor scratchMonitor(*scratch);AcDbObjectId seed;check(scratch->getAcDbObjectId(seed,false,AcDbHandle(L"10837")),"binder control seed");Snapshot source;
    {Working scope(scratch.get());source=observe(seed,observations);}auto candidate=std::make_unique<AcDbDatabase>(true,true);Working scope(candidate.get());AcDbIdMapping mapping;const auto root=clone_seed(*scratch,*candidate,seed,mapping);const auto target=observe(root,observations);const auto sourceText=source.segments.front().textStyleId,targetText=target.segments.front().textStyleId;
    DerivedBindings baseline(*scratch,*candidate,mapping);const auto initial=baseline.bind(sourceText,targetText,"control.initial");require(initial.matches,"binder_control_initial_binding_failed");std::vector<std::string> controls;const auto before=style_record(targetText);double oldWidth{};
    {auto style=opened<AcDbTextStyleTableRecord>(targetText,AcDb::kForWrite);oldWidth=style->xScale();check(style->setXScale(oldWidth+.03125),"control actual width mutation");}
    DerivedBindings changed(*scratch,*candidate,mapping);const auto mutation=changed.bind(sourceText,targetText,"control.actual_width_mutation");require(!mutation.matches&&mutation.json.find("derived_declared_definition_mismatch")!=std::string::npos,"binder_failed_to_reject_actual_style_scalar_mutation");const auto mutated=style_record(targetText);
    {auto style=opened<AcDbTextStyleTableRecord>(targetText,AcDb::kForWrite);check(style->setXScale(oldWidth),"control restore width");}require(style_record(targetText)==before,"binder control width restoration failed");
    controls.push_back(object({{"name",quote(std::string("actual_owned_textstyle_scalar_mutation"))},{"reachedStage",quote(std::string("actual_record_setXScale_then_direct_binder"))},{"before",before},{"mutated",mutated},{"restored",style_record(targetText)},{"binding",mutation.json},{"rejected","true"},{"candidateSaved","false"}}));
    auto wrong=std::make_unique<AcDbTextStyleTableRecord>();{auto actual=opened<AcDbTextStyleTableRecord>(targetText);check(wrong->copyFrom(actual.get()),"control same definition copyFrom");}check(wrong->setName(L"N8A_SAME_DEFINITION_WRONG_KEY"),"control wrong symbol key");AcDbObjectId wrongId;{auto table=opened<AcDbTextStyleTable>(candidate->textStyleTableId(),AcDb::kForWrite);check(table->add(wrongId,wrong.get()),"add actual wrong key control record");auto* resident=wrong.release();check(resident->close(),"close wrong key control record");}
    require(declared_symbol_shape(wrongId)==declared_symbol_shape(targetText),"wrong key control definitions must actually match");DerivedBindings wrongKey(*scratch,*candidate,mapping);const auto wrongResult=wrongKey.bind(sourceText,wrongId,"control.same_definition_wrong_key");require(!wrongResult.matches&&wrongResult.json.find("derived_symbol_key_relation_mismatch")!=std::string::npos,"binder_failed_to_reject_same_definition_wrong_key");controls.push_back(object({{"name",quote(std::string("same_definition_different_actual_symbol_key"))},{"binding",wrongResult.json},{"rejected","true"},{"candidateSaved","false"}}));
    DerivedBindings direction(*scratch,*candidate,mapping);const auto directionResult=direction.bind(targetText,sourceText,"control.wrong_direction");require(!directionResult.matches&&directionResult.json.find("derived_symbol_wrong_database_direction")!=std::string::npos,"binder_failed_to_reject_wrong_direction");controls.push_back(object({{"name",quote(std::string("wrong_source_target_database_direction"))},{"binding",directionResult.json},{"rejected","true"},{"candidateSaved","false"}}));
    DerivedBindings inverse(*scratch,*candidate,mapping);inverse.reverse[targetText]=source.rootLayerId;const auto inverseResult=inverse.bind(sourceText,targetText,"control.explicit_inverse_registry_fault");require(!inverseResult.matches&&inverseResult.json.find("derived_reverse_binding_inconsistent")!=std::string::npos,"binder_failed_to_reject_inverse_binding_fault");controls.push_back(object({{"name",quote(std::string("inverse_registry_fault_after_actual_records_match"))},{"binding",inverseResult.json},{"rejected","true"},{"candidateSaved","false"}}));
    report_file(config,L"binder-direct-rejection-controls.json",object({{"controls",array(controls)},{"scratchAllowedWrites",scratchMonitor.json()},{"candidateSaved","false"}}));evidence.step("directDerivedBindingControls","passed","actual scalar mutation, same-definition wrong key, wrong DB direction and inverse consistency rejected without saving");return controls;
}
SceneResult run_scene(AcDbDatabase& protectedReference,const Fixture& fixture,ObservationLog& observations,const Config& config,Evidence& evidence,const std::string& fault={}) {
    SceneResult result;result.required=fixture.name=="two_mm100_horizontal"&&fault.empty();std::string stage="request";SceneLog log{config,fixture.name+(fault.empty()?"":"_"+fault),0,{}};
    auto scratch=std::make_unique<AcDbDatabase>(false,true);check(scratch->readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false),"read owned source scratch");check(scratch->closeInput(true),"close owned scratch input");WriteMonitor scratchMonitor(*scratch);AcDbDatabase& source=*scratch;const auto scratchColdState=database_state(source,true);
    std::unique_ptr<AcDbDatabase> candidate;Snapshot sourceBefore;AcDbObjectId seed;std::vector<std::string> layerMaps;std::string actualJson="null",resourcesBefore="null",roundtrip="null",diagnostic,primaryResourceError,diagnosticApiOutcome;bool saved=false;std::wstring output;
    try{validate_fixture(fixture);stage="seed_resolution";check(source.getAcDbObjectId(seed,false,AcDbHandle(fixture.seed.c_str())),"resolve scene seed");AcDbObjectId protectedSeed;check(protectedReference.getAcDbObjectId(protectedSeed,false,AcDbHandle(fixture.seed.c_str())),"resolve protected seed identity");{auto identity=opened<AcDbEntity>(protectedSeed);require(!identity->isAProxy()&&std::wstring(identity->isA()->name())==L"TDbDimension2"&&identity->ownerId()==acdbSymUtil()->blockModelSpaceId(&protectedReference),"protected seed common identity mismatch");}{Working scope(&source);sourceBefore=observe(seed,observations);}require(sourceBefore.segments.size()==static_cast<std::size_t>(fixture.segmentCount),"seed_actual_segment_count_mismatch");
        candidate=std::make_unique<AcDbDatabase>(true,true);Working scope(candidate.get());stage="clone_setup";AcDbIdMapping mapping;const auto root=clone_seed(source,*candidate,seed,mapping);auto before=observe(root,observations);
        report_file(config,std::wstring(log.name.begin(),log.name.end())+L"-clone-public-baseline.json",object({{"source",sourceBefore.json},{"candidate",before.json},{"actualRootSdkCloneMap",sdk_map_records(mapping)}}));
        require(before.comparisonJson==sourceBefore.comparisonJson,"clone_public_geometry_measurement_format_paper_mismatch");layerMaps.push_back(validate_layer_map(sourceBefore.rootLayerId,before.rootLayerId,mapping));bool referencesMatch=true;
        DerivedBindings bindings(source,*candidate,mapping);
        for(std::size_t i=0;i<before.segments.size();++i){layerMaps.push_back(validate_layer_map(sourceBefore.segments[i].layerId,before.segments[i].layerId,mapping));
            for(const auto& role:{std::string("dimensionStyle"),std::string("effectiveTextStyle")}){stage="clone_derived_binding_"+std::to_string(i)+"_"+role;const auto checkMap=bindings.bind(role=="dimensionStyle"?sourceBefore.segments[i].dimStyleId:sourceBefore.segments[i].textStyleId,role=="dimensionStyle"?before.segments[i].dimStyleId:before.segments[i].textStyleId,"segment["+std::to_string(i)+"]."+role);layerMaps.push_back(checkMap.json);referencesMatch=referencesMatch&&checkMap.matches;
                report_file(config,std::wstring(log.name.begin(),log.name.end())+L"-resource-reference-"+std::to_wstring(i)+L"-"+std::wstring(role.begin(),role.end())+L".json",checkMap.json);}}
        if(!referencesMatch)primaryResourceError="public_resource_reference_mapping_mismatch";
        require(model_roots(*candidate).size()==1,"candidate must contain exactly one requested direct model root");
        if(mappingDiagnosticOnly){stage="diagnostic_only_native_two_node_path";try{const auto actual=adapt(root,fixture,observations,log);actualJson=actual.json;const auto path=topology(actual,fixture.direction);validate_measurements(actual,fixture,path);validate_formats(actual,fixture,path);diagnosticApiOutcome="actual_stations_measurements_baseline_and_format_matched_no_save_no_qualification";}catch(const std::exception& failure){diagnosticApiOutcome=failure.what();}throw std::runtime_error(primaryResourceError.empty()?"diagnostic_only_no_candidate_qualification":primaryResourceError);}
        require(referencesMatch,"public_resource_reference_mapping_mismatch");result.setupPassed=true;
        stage="native_adaptation";const auto actual=adapt(root,fixture,observations,log);actualJson=actual.json;const auto path=topology(actual,fixture.direction);const auto anchors=fixture.anchors();for(std::size_t i=0;i<path.nodes.size();++i)require(points_close(native_point(path.nodes[i]),anchors[i]),"actual_anchor_mismatch");
        stage="measurement_acceptance";auto expected=fixture;if(fault=="measurement")expected.stations.back()+=1;validate_measurements(actual,expected,path);
        stage="format_acceptance";validate_formats(actual,fixture,path,fault=="format"?1:0);
        stage="paper_acceptance";const auto sourcePath=topology(sourceBefore,source_direction(sourceBefore));const double scale=fixture.scaleDenominator/(100*fixture.unitToMm);for(std::size_t i=0;i<path.segments.size();++i){require(std::abs(actual.segments[path.segments[i]].textHeight-sourceBefore.segments[sourcePath.segments[i]].textHeight*scale)<=tolerance,"uniform_transform_paper_height_unsupported");require(actual.segments[path.segments[i]].textStyleShape==sourceBefore.segments[sourcePath.segments[i]].textStyleShape,"font_or_width_factor_changed");}
        require(fault.empty(),"fault_request_must_not_publish_candidate");stage="save_and_reopen";resourcesBefore=database_state(*candidate);const auto preSave=object({{"root",actual.json},{"databaseAndReadResources",resourcesBefore}});report_file(config,std::wstring(fixture.name.begin(),fixture.name.end())+L"-before-save.json",preSave);
        output=config.output+L"\\"+std::wstring(fixture.name.begin(),fixture.name.end())+L"-owned-candidate.dwg";require(GetFileAttributesW(output.c_str())==INVALID_FILE_ATTRIBUTES,"candidate file already exists");const auto handleBefore=root.handle();check(candidate->saveAs(output.c_str(),false,AcDb::kDHL_CURRENT),"save owned test DWG");saved=true;
        auto reopened=std::make_unique<AcDbDatabase>(false,true);check(reopened->readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false),"reopen owned test DWG");check(reopened->closeInput(true),"close reopened stream");{Working reopenedScope(reopened.get());AcDbObjectId reopenedRoot;check(reopened->getAcDbObjectId(reopenedRoot,false,handleBefore),"resolve reopened root");const auto readback=observe(reopenedRoot,observations);const auto reopenedState=database_state(*reopened);const auto after=object({{"root",readback.json},{"databaseAndReadResources",reopenedState}});const auto comparison=compare_fields(preSave,after);roundtrip=comparison.json;report_file(config,std::wstring(fixture.name.begin(),fixture.name.end())+L"-after-reopen.json",after);report_file(config,std::wstring(fixture.name.begin(),fixture.name.end())+L"-roundtrip-comparison.json",roundtrip);require(comparison.equal,"roundtrip_declared_public_field_mismatch");}
        result.supported=true;stage="accepted_public_probe";
    }catch(const std::exception& failure){diagnostic=failure.what();}
    // Candidate and any scoped working DB are gone before testing the source.
    std::string sourcePostError;candidate.reset();try{if(!seed.isNull()){Working scope(&source);const auto after=observe(seed,observations);require(sourceBefore.json.empty()||sourceBefore.json==after.json,"source_seed_changed_after_scene");}}catch(const std::exception& failure){sourcePostError=failure.what();if(diagnostic.empty())diagnostic=sourcePostError;result.supported=false;}
    report_file(config,std::wstring(log.name.begin(),log.name.end())+L"-source-scratch.json",object({{"role",quote(std::string("owned_mutable_reference_work_scratch_not_protected_source"))},{"inputFileSHA256",quote(config.referenceSHA256)},{"readStatus","0"},{"closeInputStatus","0"},{"coldIdentityState",scratchColdState},{"afterIdentityState",database_state(source,true)},{"allowedScratchWrites",scratchMonitor.json()},{"protectedReferenceWasNotExploded", "true"},{"willDestroyOwnedScratch","true"}}));
    result.json=object({{"fixture",fixture.json()},{"support",quote(std::string(result.supported?"supported_public_observations":result.setupPassed?"unsupported":"setup_failed"))},{"setupPassed",truth(result.setupPassed)},{"requiredMinimum",truth(result.required)},
        {"stage",quote(stage)},{"diagnostic",quote(diagnostic)},{"sourcePostCheckError",quote(sourcePostError)},{"primaryResourceError",quote(primaryResourceError)},{"diagnosticOnly",truth(mappingDiagnosticOnly)},{"diagnosticApiOutcome",quote(diagnosticApiOutcome)},{"cloneLayerMappings",array(layerMaps)},{"actual",actualJson},{"operations",array(log.actions)},
        {"allDeclaredRoundtripFields",roundtrip},{"candidateSaved",truth(saved)},{"candidateFile",output.empty()?"null":quote(output)},{"candidateSHA256",saved?quote(sha256(output)):"null"},{"candidateReturned","false"},
        {"nativeTextHostRequest","null"},{"glyphComplete","false"},{"capabilityUpgradeAllowed","false"},{"faultInjection",quote(fault)}});
    report_file(config,std::wstring(log.name.begin(),log.name.end())+L"-scene.json",result.json);evidence.step(log.name,result.supported?"passed":result.setupPassed?"unsupported":"failed",stage+": "+diagnostic);return result;
}
std::string ambient_globals(){std::vector<std::string> values;for(const auto name:{L"DBMOD",L"DIMLFAC",L"DIMSCALE",L"DIMTXT",L"DIMDEC",L"INSUNITS",L"LTSCALE",L"CLAYER",L"TEXTSTYLE",L"DIMSTYLE"}){resbuf value{};require(acedGetVar(name,&value)==RTNORM,"ambient getvar failed");std::string json;if(value.restype==RTREAL)json=number(value.resval.rreal);else if(value.restype==RTSHORT)json=std::to_string(value.resval.rint);else if(value.restype==RTLONG)json=std::to_string(value.resval.rlong);else if(value.restype==RTSTR){json=text(value.resval.rstring);acutDelString(value.resval.rstring);}else throw std::runtime_error("unexpected ambient sysvar type");values.push_back(object({{"name",text(name)},{"restype",std::to_string(value.restype)},{"value",json}}));}return array(values);}
void create_unsaved_control(AcDbDatabase& ambient){require(model_roots(ambient).empty(),"ambient must be the controller-owned unnamed empty fixture");resbuf titled{};require(acedGetVar(L"DWGTITLED",&titled)==RTNORM&&titled.restype==RTSHORT&&titled.resval.rint==0,"ambient fixture must be unnamed");auto model=opened<AcDbBlockTableRecord>(acdbSymUtil()->blockModelSpaceId(&ambient),AcDb::kForWrite);auto textObject=std::make_unique<AcDbText>();textObject->setDatabaseDefaults(&ambient);check(textObject->setTextString(L"N8A_UNSAVED_CONTROL_014cca66"),"control contents");check(textObject->setPosition({37.125,-91.375,0}),"control position");check(textObject->setHeight(2.75),"control height");check(textObject->setWidthFactor(.8),"control width");AcDbObjectId id;check(model->appendAcDbEntity(id,textObject.get()),"append owned ambient control");auto* resident=textObject.release();check(resident->close(),"close ambient control");}
struct ProtectionAudit final {
    const Config& config;Evidence& evidence;AcDbDatabase& scheme;AcDbDatabase& source;AcDbDatabase& ambient;WriteMonitor& schemeMonitor;WriteMonitor& sourceMonitor;WriteMonitor& ambientMonitor;
    std::string schemeBefore,sourceBefore,ambientBefore,globalsBefore;bool baselineComplete{},attempted{},preserved{};
    AcDbDatabase* working{acdbHostApplicationServices()->workingDatabase()};AcApDocument* current{acDocManager->curDocument()};AcApDocument* active{acDocManager->mdiActiveDocument()};
    ProtectionAudit(const Config& c,Evidence& e,AcDbDatabase& a,AcDbDatabase& b,AcDbDatabase& d,WriteMonitor& am,WriteMonitor& bm,WriteMonitor& dm):config(c),evidence(e),scheme(a),source(b),ambient(d),schemeMonitor(am),sourceMonitor(bm),ambientMonitor(dm){}
    ~ProtectionAudit() noexcept {finish();}
    void baseline(){schemeBefore=database_state(scheme,true);sourceBefore=database_state(source,true);ambientBefore=database_state(ambient);globalsBefore=ambient_globals();report_file(config,L"source-and-ambient-before.json",object({{"scheme",schemeBefore},{"reference",sourceBefore},{"ambient",ambientBefore},{"globals",globalsBefore}}));baselineComplete=true;}
    bool finish() noexcept {
        if(attempted)return preserved;attempted=true;
        try{std::vector<std::string> errors;const auto read=[&](const char* role,const std::function<std::string()>& operation){try{return operation();}catch(const std::exception& failure){errors.push_back(quote(std::string(role)+": "+failure.what()));return std::string("null");}};
            const auto schemeAfter=read("scheme public state",[&]{return database_state(scheme,true);});const auto sourceAfter=read("reference public state",[&]{return database_state(source,true);});const auto ambientAfter=read("ambient public state",[&]{return database_state(ambient);});const auto globalsAfter=read("ambient globals",[&]{return ambient_globals();});const auto schemeSHA=read("scheme disk SHA",[&]{return quote(sha256(config.scheme));});const auto sourceSHA=read("reference disk SHA",[&]{return quote(sha256(config.reference));});
            const bool pointers=working==acdbHostApplicationServices()->workingDatabase()&&current==acDocManager->curDocument()&&active==acDocManager->mdiActiveDocument();
            preserved=baselineComplete&&errors.empty()&&schemeBefore==schemeAfter&&sourceBefore==sourceAfter&&ambientBefore==ambientAfter&&globalsBefore==globalsAfter&&schemeMonitor.clean()&&sourceMonitor.clean()&&ambientMonitor.clean()&&schemeSHA==quote(config.schemeSHA256)&&sourceSHA==quote(config.referenceSHA256)&&pointers;
            report_file(config,L"source-and-ambient-after.json",object({{"baselineComplete",truth(baselineComplete)},{"postObservationComplete",truth(errors.empty())},{"postObservationErrors",array(errors)},{"scheme",schemeAfter},{"reference",sourceAfter},{"ambient",ambientAfter},{"globals",globalsAfter},{"schemeSHA256",schemeSHA},{"referenceSHA256",sourceSHA},{"currentActiveWorkingPreserved",truth(pointers)},{"schemeWrites",schemeMonitor.json()},{"referenceWrites",sourceMonitor.json()},{"ambientWrites",ambientMonitor.json()},{"allPreserved",truth(preserved)}}));
            evidence.step("sourceAmbientFinally",preserved?"passed":"failed",errors.empty()?"complete declared post observations and write counters recorded":"post observation incomplete; primary error is retained");
        }catch(const std::exception& failure){preserved=false;try{evidence.error(std::string("SECONDARY_PROTECTION_AUDIT_FAILURE: ")+failure.what());}catch(...) {}}
        catch(...){preserved=false;try{evidence.error("SECONDARY_PROTECTION_AUDIT_FAILURE: unknown exception");}catch(...) {}}
        return preserved;
    }
};
bool probe(const Config& config,Evidence& evidence) {
    ObservationLog observations{config,evidence,0};auto* ambient=acDocManager->curDocument()->database();require(ambient!=nullptr,"ambient fixture missing");create_unsaved_control(*ambient);
    resbuf dirty{};require(acedGetVar(L"DBMOD",&dirty)==RTNORM&&((dirty.restype==RTSHORT&&dirty.resval.rint!=0)||(dirty.restype==RTLONG&&dirty.resval.rlong!=0)),"ambient_control_must_be_an_actual_unsaved_edit");
    auto scheme=std::make_unique<AcDbDatabase>(false,true),reference=std::make_unique<AcDbDatabase>(false,true);
    check(scheme->readDwgFile(config.scheme.c_str(),AcDbDatabase::kForReadAndAllShare,false),"read scheme SideDB");check(scheme->closeInput(true),"close scheme input");check(reference->readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false),"read reference SideDB");check(reference->closeInput(true),"close reference input");
    WriteMonitor schemeMonitor(*scheme),sourceMonitor(*reference),ambientMonitor(*ambient);ProtectionAudit audit(config,evidence,*scheme,*reference,*ambient,schemeMonitor,sourceMonitor,ambientMonitor);audit.baseline();
    std::vector<std::string> sceneRecords,faults;bool minimum=false,allSetups=true;const auto matrix=fixtures();
    const auto sceneLimit=mappingDiagnosticOnly?std::size_t(1):matrix.size();
    for(std::size_t scene=0;scene<sceneLimit;++scene){const auto& fixture=matrix[scene];evidence.checkpoint("before-scene-"+fixture.name);const auto result=run_scene(*reference,fixture,observations,config,evidence);sceneRecords.push_back(result.json);if(result.required)minimum=result.supported;allSetups=allSetups&&result.setupPassed;}
    if(!mappingDiagnosticOnly){
    auto bad=matrix.front();bad.seed.clear();faults.push_back(expect_reject("missing_seed","request",[&]{validate_fixture(bad);},"request_missing_seed"));
    bad=matrix.front();bad.stations[1]=bad.stations[0];faults.push_back(expect_reject("duplicate_stations","request",[&]{validate_fixture(bad);},"request_duplicate_or_descending_station"));
    bad=matrix.front();bad.stations[1]=bad.stations.back()+1;faults.push_back(expect_reject("descending_stations","request",[&]{validate_fixture(bad);},"request_duplicate_or_descending_station"));
    bad=matrix.front();bad.placementSource.clear();faults.push_back(expect_reject("missing_placement","request",[&]{validate_fixture(bad);},"request_missing_placement"));
    bad=matrix.front();bad.unitSource.clear();faults.push_back(expect_reject("missing_unit_source","request",[&]{validate_fixture(bad);},"request_missing_unit_source"));
    faults.push_back(formal_source_sha_rejection(config,evidence));
    {auto faultScratch=std::make_unique<AcDbDatabase>(false,true);check(faultScratch->readDwgFile(config.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false),"fault scratch read");check(faultScratch->closeInput(true),"fault scratch close");WriteMonitor faultScratchMonitor(*faultScratch);Working scope(faultScratch.get());AcDbObjectId id;check(faultScratch->getAcDbObjectId(id,false,AcDbHandle(L"10837")),"fault seed");const auto actual=observe(id,observations);faults.push_back(expect_reject("wrong_seed_segment_count","native_snapshot_actual_count",[&]{require(actual.segments.size()==3,"seed_actual_segment_count_mismatch");},"seed_actual_segment_count_mismatch"));const auto path=topology(actual,source_direction(actual));auto grips=plain_grips(actual);grips.push_back(path.nodes.front());faults.push_back(expect_reject("ambiguous_node_match","actual_native_grips_then_explicit_duplicate_fault",[&]{n8a::unique_grip(path.nodes.front(),grips);},"node grip is not unique"));AcDbObjectId missing;faults.push_back(expect_reject("absent_seed_handle","native_seed_resolution",[&]{const auto status=faultScratch->getAcDbObjectId(missing,false,AcDbHandle(L"FFFFFFFFFFFF"));require(status==Acad::eOk,"missing_seed_resolution_rejected");},"missing_seed_resolution_rejected"));report_file(config,L"fault-source-scratch.json",object({{"allowedScratchWrites",faultScratchMonitor.json()},{"protectedSourceWasNotObserved","true"}}));}
    const auto directBinderControls=binder_controls(config,observations,evidence);faults.insert(faults.end(),directBinderControls.begin(),directBinderControls.end());
    for(const auto& fault:{std::string("measurement"),std::string("format")}){const auto result=run_scene(*reference,matrix.front(),observations,config,evidence,fault);require(result.setupPassed&&!result.supported,"post_adapt_fault_did_not_reach_native_setup");const auto wanted=fault=="measurement"?"native_measurement_mismatch":"native_format_mismatch";require(result.json.find(wanted)!=std::string::npos,"post_adapt_fault_wrong_stage");faults.push_back(result.json);}
    }
    const bool protectedState=audit.finish();
    report_file(config,L"dimension-adapt.json",object({{"contract",quote(std::string("bce.dimension-adapt.v1"))},{"phase",quote(std::string(mappingDiagnosticOnly?"native-mapping-and-node-diagnostic":"native-green-candidate"))},{"diagnosticOnly",truth(mappingDiagnosticOnly)},{"caseName",quote(config.caseName)},{"moduleSHA256",quote(config.moduleSHA256)},{"minimumMm100AdaptationPassed",truth(minimum)},{"allSceneNativeSetupsPassed",truth(allSetups)},{"sourceAndAmbientProtected",truth(protectedState)},{"scenes",array(sceneRecords)},{"faults",array(faults)},{"support",quote(std::string("partial"))},{"glyphComplete","false"},{"capabilityUpgradeAllowed","false"},{"productionResourceCompleteness","false"},{"limitations",quote(std::string("Only declared public observation/resource fields compared; no private TCH data, no natural host text request, no current drawing generation qualification"))}}));
    require(!mappingDiagnosticOnly,"diagnostic_only_not_an_accepted_candidate");
    require(protectedState,"source_or_ambient_public_state_or_write_events_changed");require(allSetups,"one_or_more_native_scene_setup_failed");require(minimum,"required_nontrivial_mm100_adaptation_not_supported");return true;
}
