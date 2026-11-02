std::string captureGroupEndpoints(AcDbDatabase& db,bcp::Plan& plan,std::string& supplement){
 Working work(&db);std::ostringstream records,json;records<<std::setprecision(17)<<"S2NATIVEENDPOINTS1 "<<plan.sourceSha<<'\n';json<<std::setprecision(17)<<"{\"method\":\"AcDbEntity::getOsnapPoints(kOsModeEnd)\",\"sourceSHA256\":\""<<plan.sourceSha<<"\",\"groups\":[";bool comma=false;
 for(auto& opening:plan.openings){if(opening.observedKind!="G")continue;auto root=open<AcDbEntity>(find(db,opening.handle));require(std::wstring(root->isA()->name())==L"TDbGroupOpening","endpoint source is not native group");AcGePoint3dArray grips;AcDbIntArray modes,ids;status(root->getGripPoints(grips,modes,ids),"group endpoint pick anchor");require(grips.length()>0,"group pick anchor absent");AcGePoint3dArray points;AcDbIntArray geometryIds;const auto result=root->getOsnapPoints(AcDb::kOsModeEnd,0,grips[0],grips[0],AcGeMatrix3d::kIdentity,points,geometryIds);
  if(comma)json<<',';comma=true;json<<"{\"handle\":\""<<opening.handle<<"\",\"status\":"<<result<<",\"points\":[";
  for(int i=0;i<points.length();++i){if(i)json<<',';json<<'['<<points[i].x<<','<<points[i].y<<','<<points[i].z<<']';if(result==Acad::eOk){opening.endpointCandidates.push_back({points[i].x,points[i].y});records<<opening.handle<<' '<<points[i].x<<' '<<points[i].y<<'\n';}}
  json<<"],\"uniqueWidthAndHostResolved\":"<<(bcp::resolveGroupEndpoints(opening,plan.walls)?"true":"false")<<'}';
 }
 json<<"],\"readOnly\":true}";supplement=records.str();return json.str();
}
