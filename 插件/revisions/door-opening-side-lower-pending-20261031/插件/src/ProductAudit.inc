// Validation-only, read-only evidence from the actually opened final DWG.
void productAudit() noexcept {
 fs::path receipt;
 try {
  maskAuditCommand();auto* doc=curDoc();require(doc&&acDocManager->mdiActiveDocument()==doc,"audit requires current document");
  const fs::path file=doc->fileName();require(file.filename()==L"control.dwg"&&file.parent_path().filename().wstring().rfind(L"stage2-preview-",0)==0,"audit is outside owned preview");receipt=file.parent_path();auto& db=*doc->database();Working working(&db);
  std::ostringstream out;out<<std::setprecision(17)<<"{\"contract\":\"bcp.product-opened-axes.v1\",\"axes\":[";bool first=true;int carriers=0;
  for(const auto& item:modelTypes(db)) {
   auto entity=open<AcDbEntity>(find(db,ascii(item.first)));auto* ref=AcDbBlockReference::cast(entity.get());if(!ref)continue;
   const auto name=symbolName(ref->blockTableRecord());if(name.rfind(L"BCP_AXIS_",0)!=0)continue;const bool vertical=name.rfind(L"BCP_AXIS_V_",0)==0;
   auto extension=open<AcDbDictionary>(ref->extensionDictionary());AcDbObjectId filters,filter;status(extension->getAt(L"ACAD_FILTER",filters),"audit axis filter collection");auto list=open<AcDbDictionary>(filters);status(list->getAt(L"SPATIAL",filter),"audit axis spatial filter");auto spatial=open<AcDbSpatialFilter>(filter);
   AcGePoint2dArray points;AcGeVector3d n;double z{},front{},back{};Adesk::Boolean on=false;status(spatial->getDefinition(points,n,z,front,back,on),"audit axis clip bounds");require(on&&points.length()==2&&n.isEqualTo(AcGeVector3d::kZAxis)&&z==0,"opened axis clip shape differs");
   auto block=open<AcDbBlockTableRecord>(ref->blockTableRecord());AcDbBlockTableRecordIterator* raw{};status(block->newIterator(raw),"audit native axis contents");std::unique_ptr<AcDbBlockTableRecordIterator> it(raw);AcDbObjectId child;int count=0;
   for(;!it->done();it->step()){AcDbObjectId id;status(it->getEntityId(id),"audit native axis child");auto e=open<AcDbEntity>(id);require(std::wstring(e->isA()->name())==L"TDbAxisLabelSet"&&!e->isAProxy(),"axis container is not genuine TCH");child=id;++count;}require(count==1,"unexpected axis container contents");const auto ink=axisInk(child,vertical);
   if(!first)out<<',';first=false;++carriers;out<<"{\"name\":"<<jsonUtf8(name)<<",\"nativeHandle\":\""<<axisHandle(child)<<"\",\"container\":\""<<axisHandle(ref->objectId())<<"\",\"inverted\":"<<(spatial->isInverted()?"true":"false")<<",\"min\":["<<points[0].x<<','<<points[0].y<<"],\"max\":["<<points[1].x<<','<<points[1].y<<"],\"circles\":[";
   for(std::size_t i=0;i<ink.circles.size();++i){if(i)out<<',';out<<"{\"centre\":"<<wholePoints({ink.circles[i].centre})<<",\"radius\":"<<ink.circles[i].radius<<'}';}out<<"]}";
  }
  out<<"],\"groups\":[";first=true;int groups=0,lines=0;auto dict=open<AcDbDictionary>(db.groupDictionaryId());std::unique_ptr<AcDbDictionaryIterator> it(dict->newIterator());
  for(;!it->done();it->next()){const std::wstring name=it->name();if(name.rfind(L"BCP_GRID_",0)!=0)continue;auto group=open<AcDbGroup>(it->objectId());AcDbObjectIdArray members;group->allEntityIds(members);require(!members.isEmpty(),"empty logical axis group");if(!first)out<<',';first=false;++groups;out<<"{\"name\":"<<jsonUtf8(name)<<",\"lines\":[";
   for(int i=0;i<members.length();++i){auto line=open<AcDbLine>(members[i]);require(line->isA()==AcDbLine::desc(),"logical axis member is not native line");if(i)out<<',';++lines;out<<"{\"handle\":\""<<axisHandle(members[i])<<"\",\"curve\":"<<wholePoints({line->startPoint(),line->endPoint()})<<",\"linetype\":"<<jsonUtf8(symbolName(line->linetypeId()))<<"}";}out<<"]}";
  }
  require(carriers==18&&groups==20&&lines==22,"opened axis coverage differs");out<<"],\"carriers\":"<<carriers<<",\"logicalGroups\":"<<groups<<",\"axisLines\":"<<lines<<",\"readOnly\":true,\"error\":null}";writeNew(receipt/L"product-axis-document-audit.json",out.str());
 }catch(const std::exception& e){try{if(!receipt.empty())writeNew(receipt/L"product-axis-document-audit.error",e.what());}catch(...){}}
}
