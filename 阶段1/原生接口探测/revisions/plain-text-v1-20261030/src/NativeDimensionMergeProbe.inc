// Stage1 only. Include after DimensionBridge and TextBridge helpers.
void nativeDimensionMergeProbe() {
    try {
        const auto p=paths();const auto control=fullPath(p.output+L"\\control.dwg"),output=fullPath(p.output+L"\\dimension-merge-generated.dwg"),report=fullPath(p.output+L"\\dimension-merge-native.json");
        requireNewFile(report);requireNewFile(output);
        std::string error="null",before="[]",after="[]",saved="[]",addBefore="[]",addAfter="[]",addSaved="[]",measureBefore="[]",measureAfter="null",measureSaved="null",wall="null",splitJson="null",rawSplitJson="null";
        int initial=-1,prepared=-1,final=-1,reopened=-1,command=-9999,save=-9999,read=-9999,closed=-9999,txBefore=-1,txAfter=-1;
        std::vector<std::wstring> originalHandles;auto trace=std::make_shared<AxisNativeTraceData>();AcDbIntArray transformStatuses;AcGePoint3dArray picks,users;
        try {
            if(!acDocManager || acDocManager->isApplicationContext())throw std::runtime_error("merge requires document context");
            auto* document=acDocManager->curDocument();auto* db=document?document->database():nullptr;
            const auto guard=[&]{
                if(!document||!db||acDocManager->isApplicationContext()||acDocManager->curDocument()!=document||acDocManager->mdiActiveDocument()!=document||acdbHostApplicationServices()->workingDatabase()!=db||!document->fileName()||_wcsicmp(fullPath(document->fileName()).c_str(),control.c_str()))throw std::runtime_error("merge owned document/MDI/working guard failed");
                const ACHAR* file{};check(db->getFilename(file),"merge native DB path");if(!file||_wcsicmp(fullPath(file).c_str(),control.c_str())||!below(control,p.output))throw std::runtime_error("merge owned database path differs");
                if(!db->transactionManager()||db->transactionManager()->numActiveTransactions()!=0)throw std::runtime_error("merge requires zero active transactions");
            };
            guard();const auto originals=modelIds(*db);initial=originals.length();if(initial!=213)throw std::runtime_error("merge must begin with full213 scheme");before=familyCompactArray(originals);
            for(int i=0;i<originals.length();++i)originalHandles.push_back(handleOf(originals[i]));
            AcGePoint3d left,right,split;double minY=std::numeric_limits<double>::infinity();
            {
                AcDbObjectId id;check(db->getAcDbObjectId(id,false,AcDbHandle(L"CD83")),"merge actual wall");auto e=open<AcDbEntity>(id);const auto* c=AcDbCurve::cast(e.get());
                if(!c||_wcsicmp(e->isA()->name(),L"TDbWall"))throw std::runtime_error("merge CD83 is not native wall");
                check(c->getStartPoint(left),"merge wall start");check(c->getEndPoint(right),"merge wall end");if(left.x>right.x)std::swap(left,right);
                if(std::fabs(left.y-right.y)>1e-6||std::fabs(right.x-left.x-3600)>1e-6)throw std::runtime_error("merge wall is not expected actual horizontal3600");
                wall="{\"sourceHandle\":\"CD83\",\"left\":"+point(left)+",\"right\":"+point(right)+"}";
            }
            {
                AcDbObjectId id;check(db->getAcDbObjectId(id,false,AcDbHandle(L"CD84")),"merge actual opening");auto e=open<AcDbEntity>(id);if(_wcsicmp(e->isA()->name(),L"TDbOpening"))throw std::runtime_error("merge CD84 is not native opening");
                AcGePoint3dArray grip;AcDbIntArray snap,geo;check(e->getGripPoints(grip,snap,geo),"merge actual opening points");bool found=false;
                for(int i=0;i<grip.length();++i)if(std::fabs(grip[i].y-left.y)<=1e-6&&grip[i].x>left.x+1e-6&&grip[i].x<right.x-1e-6&&(!found||grip[i].x>split.x)){split=grip[i];found=true;}
                if(!found||std::fabs(split.x-left.x-3000)>1e-6)throw std::runtime_error("merge opening actual right jamb not found");rawSplitJson=point(split);split=AcGePoint3d(split.x,left.y,left.z);splitJson=point(split);
            }
            for(int i=0;i<originals.length();++i){auto e=open<AcDbEntity>(originals[i]);if(!_wcsicmp(e->isA()->name(),L"TDbDimension2")){AcDbExtents b;check(e->getGeomExtents(b),"merge baseline dimension bounds");minY=(std::min)(minY,b.minPoint().y);}}
            if(!std::isfinite(minY))throw std::runtime_error("merge placement baseline absent");
            AcDbObjectId dimensions[2];
            {
                AcDbDatabase source(false,true);check(source.readDwgFile(p.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false),"merge read frozen reference");check(source.closeInput(true),"merge close reference input");
                AcDbObjectId templateId;check(source.getAcDbObjectId(templateId,false,AcDbHandle(L"19A0")),"merge find actual dimension template");
                {auto e=open<AcDbEntity>(templateId);if(_wcsicmp(e->isA()->name(),L"TDbDimension2")||e->isAProxy())throw std::runtime_error("merge reference dimension template differs");}
                for(int i=0;i<2;++i){AcDbObjectIdArray one;one.append(templateId);AcDbIdMapping mapping;check(mapping.setDestDb(db),"merge clone destination");check(source.wblockCloneObjects(one,modelId(*db),mapping,AcDb::kDrcMangleName,false),"merge clone one dimension");AcDbIdPair pair;pair.setKey(templateId);if(!mapping.compute(pair)||!pair.isCloned()||pair.value().isNull())throw std::runtime_error("merge actual template map absent");dimensions[i]=pair.value();}
            }
            std::ostringstream measurements;measurements<<'[';
            for(int i=0;i<2;++i){
                const auto first=i==0?left:split,last=i==0?split:right;
                if(!moveSchemeGripTo(*db,dimensions[i],2,first).succeeded||!moveSchemeGripTo(*db,dimensions[i],3,last).succeeded)throw std::runtime_error("merge real measurement-point placement failed");
                const auto anchor=currentSchemeGrip(dimensions[i],0);if(!moveSchemeGripTo(*db,dimensions[i],0,AcGePoint3d(anchor.x,minY-1000,anchor.z)).succeeded)throw std::runtime_error("merge dimension-line placement failed");
                const auto m=dimensionBridgeMeasurements(dimensions[i],left.y,left.z);if(i)measurements<<',';measurements<<m.json(left.y,left.z);
                if(m.points.length()!=2||m.segments.size()!=1||std::fabs(m.segments[0]-(i==0?3000:600))>1e-6)throw std::runtime_error("merge actual pre-command measurements differ");
                auto e=open<AcDbEntity>(dimensions[i]);const auto* c=AcDbCurve::cast(e.get());if(!c)throw std::runtime_error("merge native dimension curve missing");AcGePoint3d start,end;check(c->getStartPoint(start),"merge dimension start");check(c->getEndPoint(end),"merge dimension end");picks.append(AcGePoint3d((start.x+end.x)/2,start.y,start.z));
            }
            measurements<<']';measureBefore=measurements.str();prepared=modelCount(*db);addBefore=familyCompactArray(familyAdditions(*db,originals));if(prepared!=215)throw std::runtime_error("merge requires exactly2 added dimensions");
            guard();
            AcGePoint3d viewFirst,viewLast;
            if(textBridgeToUcs(AcGePoint3d(left.x-500,minY-2000,left.z),viewFirst)!=RTNORM||textBridgeToUcs(AcGePoint3d(right.x+500,minY,left.z),viewLast)!=RTNORM)throw std::runtime_error("merge diagnostic view conversion failed");
            ads_point v1{viewFirst.x,viewFirst.y,viewFirst.z},v2{viewLast.x,viewLast.y,viewLast.z};
            if(acedCommandS(RTSTR,L"_.ZOOM",RTSTR,L"_W",RT3DPOINT,v1,RT3DPOINT,v2,RTNONE)!=RTNORM)throw std::runtime_error("merge tight diagnostic view failed");
            for(int i=0;i<picks.length();++i){AcGePoint3d user;const int status=textBridgeToUcs(picks[i],user);transformStatuses.append(status);if(status!=RTNORM)throw std::runtime_error("merge WCS to UCS failed");users.append(user);}
            guard();txBefore=db->transactionManager()->numActiveTransactions();
            {
                AxisNativeTraceOwner owner(document,trace);if(!trace->editorAdded||!trace->inputAddReturned||trace->inputAddStatus!=Acad::eOk||trace->traceDropped)throw std::runtime_error("merge native observers unavailable");
                ads_point first{users[0].x,users[0].y,users[0].z},second{users[1].x,users[1].y,users[1].z};
                command=acedCommandS(RTSTR,L"TMergeDim",RT3DPOINT,first,RT3DPOINT,second,RTSTR,L"",RTNONE);
            }
            guard();txAfter=db->transactionManager()->numActiveTransactions();if(trace->retained)throw std::runtime_error("merge trace retained; unload guard remains authoritative");
            after=familyCompactArray(originals);final=modelCount(*db);const auto additions=familyAdditions(*db,originals);addAfter=familyCompactArray(additions);
            if(additions.length()==1)measureAfter=dimensionBridgeMeasurements(additions[0],left.y,left.z).json(left.y,left.z);
            requireNewFile(output);save=db->saveAs(output.c_str(),false,AcDb::kDHL_CURRENT);check(static_cast<Acad::ErrorStatus>(save),"merge save isolated output");
            AcDbDatabase reader(false,true);read=reader.readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false);check(static_cast<Acad::ErrorStatus>(read),"merge fresh independent output read");closed=reader.closeInput(true);check(static_cast<Acad::ErrorStatus>(closed),"merge close output input");
            {
                WorkingDatabaseScope work(reader);reopened=modelCount(reader);AcDbObjectIdArray old;
                for(const auto& h:originalHandles){AcDbObjectId id;check(reader.getAcDbObjectId(id,false,AcDbHandle(h.c_str())),"merge persisted original");old.append(id);}
                saved=familyCompactArray(old);const auto added=familyAdditions(reader,old);addSaved=familyCompactArray(added);if(added.length()==1)measureSaved=dimensionBridgeMeasurements(added[0],left.y,left.z).json(left.y,left.z);
            }
        }catch(const std::exception& e){error=dimensionBridgeError(e);}catch(...){error=quote(L"unknown dimension merge exception");}
        std::ostringstream out;out<<"{\"contract\":\"stage1.dimension-merge-native.v1\",\"api\":\"ObjectARX native; no COM\",\"controlDWG\":"<<quote(control)<<",\"outputDWG\":"<<quote(output)<<",\"error\":"<<error
            <<",\"initialModelCount\":"<<initial<<",\"beforeMergeModelCount\":"<<prepared<<",\"finalModelCount\":"<<final<<",\"reopenedModelCount\":"<<reopened<<",\"wallControl\":"<<wall<<",\"rawOpeningGrip\":"<<rawSplitJson<<",\"splitPoint\":"<<splitJson
            <<",\"originalBefore\":"<<before<<",\"originalAfter\":"<<after<<",\"originalSaved\":"<<saved<<",\"addedBefore\":"<<addBefore<<",\"addedAfter\":"<<addAfter<<",\"addedSaved\":"<<addSaved
            <<",\"beforeMeasurements\":"<<measureBefore<<",\"afterMeasurements\":"<<measureAfter<<",\"savedMeasurements\":"<<measureSaved<<",\"pointTransformStatuses\":"<<integers(transformStatuses)<<",\"picksWCS\":"<<points(picks)<<",\"picksUCS\":"<<points(users)
            <<",\"commandName\":\"TMergeDim\",\"commandStatus\":"<<command<<",\"saveStatus\":"<<save<<",\"reopenStatus\":"<<read<<",\"closeInputStatus\":"<<closed<<",\"activeTransactionsBeforeCommand\":"<<txBefore<<",\"activeTransactionsAfterCommand\":"<<txAfter
            <<",\"commandTrace\":"<<axisTraceArray(trace->commands)<<",\"inputTrace\":"<<axisTraceArray(trace->inputs)<<",\"trace\":"<<dimensionBridgeTraceJson(*trace)<<",\"scope\":\"two independent source-derived dimensions connected by documented command; no production perimeter/association/visual claim\"}";
        writeNewReport(report,out.str());acutPrintf(L"\nSTAGE1_DIMENSION_MERGE_COMPLETED\n");
    }catch(const std::exception& e){acutPrintf(L"\nSTAGE1_DIMENSION_MERGE_FAILED: %hs\n",e.what());}
}
