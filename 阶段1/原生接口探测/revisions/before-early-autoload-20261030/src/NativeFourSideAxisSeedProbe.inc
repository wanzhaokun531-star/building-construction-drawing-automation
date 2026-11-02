// Include after NativeTextBridgeProbe.inc, Family, AxisTemplateDelete/Prune and
// native trace helpers. No new headers/libs. These helpers never activate or
// zoom a document, display a panel, alter a sysvar, or call entity COM.

void fourAxisGuard(const Paths& p, AcApDocument* document, AcDbDatabase* database) {
    quietRequireIsolatedWorker();
    const auto control=fullPath(p.output+L"\\control.dwg");
    if(!acDocManager||!document||!database||acDocManager->isApplicationContext()
        ||acDocManager->curDocument()!=document||acDocManager->mdiActiveDocument()!=document
        ||document->database()!=database||acdbHostApplicationServices()->workingDatabase()!=database
        ||!document->fileName()||_wcsicmp(fullPath(document->fileName()).c_str(),control.c_str()))
        throw std::runtime_error("four-axis owned current/MDI/working document guard failed");
    const ACHAR* filename{};check(database->getFilename(filename),"four-axis native database path");
    if(!filename||_wcsicmp(fullPath(filename).c_str(),control.c_str())||!below(control,p.output))
        throw std::runtime_error("four-axis owned database path differs");
    if(!database->transactionManager()||database->transactionManager()->numActiveTransactions()!=0)
        throw std::runtime_error("four-axis requires zero active transactions across native commands");
}

double fourAxisStation(const AcGePoint3d& p,bool vertical){return vertical?p.x:p.y;}
double fourAxisAcross(const AcGePoint3d& p,bool vertical){return vertical?p.y:p.x;}
std::vector<double> fourAxisUnique(const AcGePoint3dArray& points,bool vertical){
    std::vector<double> values,result;for(int i=0;i<points.length();++i)values.push_back(fourAxisStation(points[i],vertical));
    std::sort(values.begin(),values.end());for(const auto value:values)if(result.empty()||std::fabs(value-result.back())>1e-6)result.push_back(value);return result;
}
bool fourAxisHas(const std::vector<double>& values,double value){for(const auto v:values)if(std::fabs(v-value)<=1e-6)return true;return false;}
AcGePoint3dArray fourAxisWithout(const AcGePoint3dArray& points,const std::vector<double>& removed,bool vertical){
    AcGePoint3dArray result;for(int i=0;i<points.length();++i)if(!fourAxisHas(removed,fourAxisStation(points[i],vertical)))result.append(points[i]);return result;
}

struct FourAxisObservation {
    bool vertical{};AcGePoint3dArray grips,stretch,firstCenters,secondCenters;std::vector<double> stations;
    Acad::ErrorStatus gripStatus{Acad::eNotApplicable},stretchStatus{Acad::eNotApplicable},extentStatus{Acad::eNotApplicable};
    AcDbExtents bounds;double first{},second{},radius{};std::string entity{"null"};
    bool available()const{return gripStatus==Acad::eOk&&stretchStatus==Acad::eOk&&extentStatus==Acad::eOk&&!grips.isEmpty()&&!stretch.isEmpty()&&std::isfinite(radius)&&radius>0;}
};
FourAxisObservation fourAxisObserve(AcDbObjectId id,bool vertical){
    FourAxisObservation o;o.vertical=vertical;
    {auto e=open<AcDbEntity>(id);if(e->isAProxy()||_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet"))throw std::runtime_error("four-axis target is not native TDbAxisLabelSet");
     AcDbIntArray snap,geometry;o.gripStatus=e->getGripPoints(o.grips,snap,geometry);o.stretchStatus=e->getStretchPoints(o.stretch);o.extentStatus=e->getGeomExtents(o.bounds);}
    o.entity=familyCompactEntity(id);if(o.grips.isEmpty()||o.stretch.isEmpty()||o.extentStatus!=Acad::eOk)return o;
    o.stations=fourAxisUnique(o.stretch,vertical);o.first=o.second=fourAxisAcross(o.grips[0],vertical);
    for(int i=1;i<o.grips.length();++i){const double a=fourAxisAcross(o.grips[i],vertical);o.first=vertical?(std::max)(o.first,a):(std::min)(o.first,a);o.second=vertical?(std::min)(o.second,a):(std::max)(o.second,a);}
    for(int i=0;i<o.grips.length();++i){const auto& p=o.grips[i];if(std::fabs(fourAxisAcross(p,vertical)-o.first)<=1e-6)o.firstCenters.append(p);if(std::fabs(fourAxisAcross(p,vertical)-o.second)<=1e-6)o.secondCenters.append(p);}
    const double a=vertical?o.bounds.maxPoint().y-o.first:o.first-o.bounds.minPoint().x;
    const double b=vertical?o.second-o.bounds.minPoint().y:o.bounds.maxPoint().x-o.second;o.radius=(a+b)/2;
    if(std::fabs(a-b)>1e-6)throw std::runtime_error("four-axis opposite visible circle radii disagree with native bounds");
    return o;
}

// The two off-station UI controls are not circle tangent points. Native
// TDelLabel can canonicalize both from firstStation-R to firstStation+R
// when a collection becomes a singleton. Only these two joint states are
// allowed; the six real station controls and four stretch points stay exact.
AcGePoint3dArray fourAxisStationGrips(const FourAxisObservation& o){
    AcGePoint3dArray result;for(int i=0;i<o.grips.length();++i)if(fourAxisHas(o.stations,fourAxisStation(o.grips[i],o.vertical)))result.append(o.grips[i]);return result;
}
AcGePoint3dArray fourAxisAuxiliaryGrips(const FourAxisObservation& o){
    AcGePoint3dArray result;for(int i=0;i<o.grips.length();++i)if(!fourAxisHas(o.stations,fourAxisStation(o.grips[i],o.vertical)))result.append(o.grips[i]);return result;
}
int fourAxisAuxiliarySign(const FourAxisObservation& o){
    if(!o.available()||o.stations.empty())return 0;const auto actual=fourAxisAuxiliaryGrips(o);if(actual.length()!=2)return 0;
    AcGePoint3dArray edges;const double station=o.stations.front();double low=std::numeric_limits<double>::infinity(),high=-low;
    for(int i=0;i<o.stretch.length();++i)if(std::fabs(fourAxisStation(o.stretch[i],o.vertical)-station)<=1e-6){low=(std::min)(low,fourAxisAcross(o.stretch[i],o.vertical));high=(std::max)(high,fourAxisAcross(o.stretch[i],o.vertical));}
    for(int i=0;i<o.stretch.length();++i)if(std::fabs(fourAxisStation(o.stretch[i],o.vertical)-station)<=1e-6&&(std::fabs(fourAxisAcross(o.stretch[i],o.vertical)-low)<=1e-6||std::fabs(fourAxisAcross(o.stretch[i],o.vertical)-high)<=1e-6))edges.append(o.stretch[i]);
    if(edges.length()!=2||!std::isfinite(low)||!std::isfinite(high)||high-low<=1e-6)return 0;
    for(int sign=-1;sign<=1;sign+=2){AcGePoint3dArray expected;for(int i=0;i<2;++i){auto q=edges[i];if(o.vertical)q.x+=sign*o.radius;else q.y+=sign*o.radius;expected.append(q);}if(axisPrunePointBag(actual,expected))return sign;}return 0;
}
bool fourAxisBoundsValid(const FourAxisObservation& o){
    if(!o.available()||o.stations.empty())return false;const double lo=o.stations.front()-o.radius,hi=o.stations.back()+o.radius;
    const AcGePoint3d expectedMin=o.vertical?AcGePoint3d(lo,o.second-o.radius,0):AcGePoint3d(o.first-o.radius,lo,0);
    const AcGePoint3d expectedMax=o.vertical?AcGePoint3d(hi,o.first+o.radius,0):AcGePoint3d(o.second+o.radius,hi,0);
    return (o.bounds.minPoint()-expectedMin).length()<=1e-6&&(o.bounds.maxPoint()-expectedMax).length()<=1e-6;
}
std::string fourAxisAuxiliaryAudit(const FourAxisObservation& before,const FourAxisObservation& after){
    const int a=fourAxisAuxiliarySign(before),b=fourAxisAuxiliarySign(after);
    return "{\"policy\":\"only two joint auxiliary-control states at firstStation plus/minus unchanged radius and actual outer stretch edges\",\"beforeSign\":"+std::to_string(a)+",\"afterSign\":"+std::to_string(b)+",\"beforePoints\":"+points(fourAxisAuxiliaryGrips(before))+",\"afterPoints\":"+points(fourAxisAuxiliaryGrips(after))+",\"canonicalMirrorObserved\":"+(a&&b&&a!=b?"true":"false")+",\"afterBoundsValid\":"+(fourAxisBoundsValid(after)?"true":"false")+",\"numberDirectionVerified\":false}";
}

AcDbObjectId fourAxisCloneTemplate(const Paths& p,AcDbDatabase& destination,const wchar_t* sourceHandle,std::string& evidence){
    if(_wcsicmp(sourceHandle,L"199F")&&_wcsicmp(sourceHandle,L"19A3"))throw std::runtime_error("four-axis only bound reference templates199F/19A3 accepted");
    const int count=modelCount(destination);AcDbDatabase source(false,true);
    const auto read=source.readDwgFile(p.reference.c_str(),AcDbDatabase::kForReadAndAllShare,false);check(read,"read four-axis frozen reference");
    const auto closed=source.closeInput(true);check(closed,"close four-axis reference input");
    AcDbObjectId templateId;check(source.getAcDbObjectId(templateId,false,AcDbHandle(sourceHandle)),"find four-axis bound template");std::string before,after;
    {WorkingDatabaseScope scope(source);auto e=open<AcDbEntity>(templateId);if(e->isAProxy()||_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet"))throw std::runtime_error("four-axis frozen template native class differs");e.reset();before=familyCompactEntity(templateId);}
    AcDbObjectIdArray one;one.append(templateId);AcDbIdMapping mapping;check(mapping.setDestDb(&destination),"four-axis template destination");
    const auto cloned=source.wblockCloneObjects(one,modelId(destination),mapping,AcDb::kDrcMangleName,false);check(cloned,"clone one four-axis native template");
    AcDbIdPair pair;pair.setKey(templateId);if(!mapping.compute(pair)||!pair.isCloned()||pair.value().isNull()||pair.value().database()!=&destination)throw std::runtime_error("four-axis actual primary mapping missing");
    const auto resources=symbolResourceMapping(mapping);{WorkingDatabaseScope scope(source);after=familyCompactEntity(templateId);}
    if(modelCount(destination)!=count+1)throw std::runtime_error("four-axis primary clone imported unexpected model entities");
    evidence="{\"sourceHandle\":"+quote(sourceHandle)+",\"targetHandle\":"+quote(handleOf(pair.value()))+",\"sourceReadStatus\":"+std::to_string(read)+",\"sourceCloseInputStatus\":"+std::to_string(closed)+",\"cloneStatus\":"+std::to_string(cloned)+",\"sourceBefore\":"+before+",\"sourceAfterClone\":"+after+",\"resourceMapping\":"+resources+"}";
    return pair.value();
}
AcDbObjectId fourAxisCloneEntity(AcDbDatabase& database,AcDbObjectId seed){
    AcRxObject* raw{};{auto e=open<AcDbEntity>(seed);if(e->isAProxy()||_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet"))throw std::runtime_error("four-axis clone seed class differs");raw=e->clone();}
    if(!raw)throw std::runtime_error("four-axis native clone returned null");auto* e=AcDbEntity::cast(raw);
    if(e&&(!e->objectId().isNull()||e->database()!=nullptr))throw std::runtime_error("four-axis clone unexpectedly database-owned; no deletion/close guessed");
    std::unique_ptr<AcRxObject> object(raw);if(!e||e->isAProxy()||_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet"))throw std::runtime_error("four-axis clone returned different native class");
    AcDbObjectId id;{auto model=open<AcDbBlockTableRecord>(modelId(database),AcDb::kForWrite);check(model->appendAcDbEntity(id,e),"append four-axis transient native clone");object.release();check(e->close(),"close database-owned four-axis native clone");}return id;
}

struct FourAxisPruneResult {std::string json{"null"};bool completed{};bool safeToContinue{true};};
FourAxisPruneResult fourAxisPruneToFirst(const Paths& p,AcApDocument* document,AcDbDatabase& database,AcDbObjectId id,bool vertical,const AcDbObjectIdArray& protectedIds){
    FourAxisPruneResult result;std::string error{"null"};const auto protectedBefore=familyCompactArray(protectedIds);auto before=fourAxisObserve(id,vertical),after=before;
    const std::vector<double> expected=vertical?std::vector<double>{35081.381329981,38881.381329633,40281.381329981,41881.381329981,46081.381329981,47481.381329981,47521.381329981,49621.381329981,53081.381329981}:std::vector<double>{18092.097205169,19392.097205169,20192.097205169,23792.097205229,25592.097204953,27392.097204953,29892.097204953,31392.097204953};
    std::vector<double> remove;std::vector<std::string> attempts;double retained=std::numeric_limits<double>::quiet_NaN();
    try{
        fourAxisGuard(p,document,&database);
        if(!before.available()||!axisPruneSameXs(before.stations,expected)||std::fabs(before.radius-400)>1e-6||!fourAxisAuxiliarySign(before)||!fourAxisBoundsValid(before))throw std::runtime_error("four-axis bound template station/radius/auxiliary/bounds baseline differs");
        retained=expected.front();remove.assign(expected.begin()+1,expected.end());
        if(!fourAxisHas(fourAxisUnique(before.firstCenters,vertical),retained)||!fourAxisHas(fourAxisUnique(before.secondCenters,vertical),retained))throw std::runtime_error("four-axis retained first station is not visible on both opposite sides");
        // The complete deletion whitelist is fixed before the first command.
        // One long rectangle covers all unwanted controls, including hidden ones.
        for(int side=0;side<2;++side){
            auto current=fourAxisObserve(id,vertical);if(current.stations.size()==1)break;
            const double across=side==0?current.first:current.second;
            const double amin=remove.front()-450,amax=remove.back()+450;
            if(retained>=amin-400-1e-6&&retained<=amax+400+1e-6)throw std::runtime_error("four-axis declared deletion window may touch retained circle");
            AcGePoint3dArray world,user;AcDbIntArray transforms;
            world.append(vertical?AcGePoint3d(amin,across-450,0):AcGePoint3d(across-450,amin,0));
            world.append(vertical?AcGePoint3d(amax,across+450,0):AcGePoint3d(across+450,amax,0));
            for(const auto station:remove){if(!fourAxisHas(current.stations,station))continue;bool covered=false;for(int i=0;i<current.stretch.length();++i){const auto& q=current.stretch[i];if(std::fabs(fourAxisStation(q,vertical)-station)<=1e-6&&q.x>=world[0].x-1e-6&&q.x<=world[1].x+1e-6&&q.y>=world[0].y-1e-6&&q.y<=world[1].y+1e-6)covered=true;}if(!covered)throw std::runtime_error("four-axis delete rectangle does not cover every predeclared hidden/visible station control");}
            for(int i=0;i<world.length();++i){AcGePoint3d q;const int status=textBridgeToUcs(world[i],q);transforms.append(status);if(status!=RTNORM)throw std::runtime_error("four-axis delete window transform failed");user.append(q);}
            auto trace=std::make_shared<AxisNativeTraceData>();int command=-9999,txBefore=-1,txAfter=-1;bool attempted=false;
            try{
                fourAxisGuard(p,document,&database);txBefore=database.transactionManager()->numActiveTransactions();
                {AxisNativeTraceOwner owner(document,trace);if(!trace->editorAdded||!trace->inputAddReturned||trace->inputAddStatus!=Acad::eOk||trace->traceDropped)throw std::runtime_error("four-axis delete observers unavailable");
                 ads_point a{user[0].x,user[0].y,user[0].z},b{user[1].x,user[1].y,user[1].z};attempted=true;
                 command=acedCommandS(RTSTR,L"TDelLabel",RT3DPOINT,a,RT3DPOINT,b,RTSTR,L"",RTSTR,L"N",RTNONE);}
                fourAxisGuard(p,document,&database);txAfter=database.transactionManager()->numActiveTransactions();after=fourAxisObserve(id,vertical);
                if(trace->retained){result.safeToContinue=false;throw std::runtime_error("four-axis delete native observers retained");}
                if(command!=RTNORM)throw std::runtime_error("four-axis TDelLabel did not return RTNORM");
            }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown four-axis delete exception");result.safeToContinue=false;}
            if(trace->retained)result.safeToContinue=false;
            attempts.push_back("{\"side\":"+quote(side==0?L"first":L"second")+",\"commandName\":\"TDelLabel\",\"declaredDeleteStations\":"+axisTemplateDeleteNumbers(remove)+",\"worldWindow\":"+points(world)+",\"userWindow\":"+points(user)+",\"transformStatuses\":"+integers(transforms)+",\"commandAttempted\":"+(attempted?"true":"false")+",\"commandStatus\":"+std::to_string(command)+",\"activeTransactionsBeforeCommand\":"+std::to_string(txBefore)+",\"activeTransactionsAfterCommand\":"+std::to_string(txAfter)+",\"before\":"+current.entity+",\"after\":"+after.entity+",\"commandTrace\":"+axisTraceArray(trace->commands)+",\"inputTrace\":"+axisTraceArray(trace->inputs)+",\"trace\":"+axisTemplateDeleteTrace(*trace)+",\"error\":"+error+"}");
            if(error!="null")break;
            if(familyCompactArray(protectedIds)!=protectedBefore)throw std::runtime_error("four-axis delete altered a protected original/other collection");
            const auto retainedBefore=fourAxisWithout(fourAxisStationGrips(before),remove,vertical),retainedAfter=fourAxisWithout(fourAxisStationGrips(after),remove,vertical);
            if(!axisPrunePointBag(retainedBefore,retainedAfter)||!axisPrunePointBag(fourAxisWithout(before.stretch,remove,vertical),fourAxisWithout(after.stretch,remove,vertical)))throw std::runtime_error("four-axis delete changed a retained station control/stretch point");
            if(after.stations.size()==1){if(!fourAxisAuxiliarySign(before)||!fourAxisAuxiliarySign(after)||std::fabs(before.radius-after.radius)>1e-6||!fourAxisBoundsValid(after))throw std::runtime_error("four-axis singleton auxiliary controls outside the two declared joint states or circle bounds changed");}
            else if(!axisPrunePointBag(fourAxisAuxiliaryGrips(before),fourAxisAuxiliaryGrips(after)))throw std::runtime_error("four-axis multi-station delete changed auxiliary controls");
            if(after.stations.size()==1)break;
            // A second opposite-side attempt is allowed only when every still
            // stored station belongs to the original fixed unwanted whitelist.
            for(const auto station:after.stations)if(std::fabs(station-retained)>1e-6&&!fourAxisHas(remove,station))throw std::runtime_error("four-axis delete introduced an unexpected station");
        }
        after=fourAxisObserve(id,vertical);
        result.completed=error=="null"&&after.available()&&after.stations.size()==1&&std::fabs(after.stations[0]-retained)<=1e-6&&after.stretch.length()==4&&after.grips.length()==8
            &&after.firstCenters.length()==1&&after.secondCenters.length()==1
            &&axisPrunePointBag(fourAxisWithout(fourAxisStationGrips(before),remove,vertical),fourAxisStationGrips(after))&&axisPrunePointBag(fourAxisWithout(before.stretch,remove,vertical),after.stretch)
            &&fourAxisAuxiliarySign(before)!=0&&fourAxisAuxiliarySign(after)!=0&&std::fabs(before.radius-after.radius)<=1e-6&&fourAxisBoundsValid(after);
        if(!result.completed&&error=="null")error=quote(L"native template did not become exactly one two-sided station; no fallback object synthesized");
    }catch(const std::exception& e){if(error=="null")error=familyError(e);}catch(...){if(error=="null")error=quote(L"unknown four-axis seed exception");result.safeToContinue=false;}
    result.json="{\"orientation\":"+quote(vertical?L"vertical":L"horizontal")+",\"sourceHandle\":"+quote(vertical?L"199F":L"19A3")+",\"axisHandle\":"+quote(handleOf(id))+",\"retainedStation\":"+number(retained)+",\"declaredDeleteStations\":"+axisTemplateDeleteNumbers(remove)+",\"before\":"+before.entity+",\"after\":"+after.entity+",\"auxiliaryControlAudit\":"+fourAxisAuxiliaryAudit(before,after)+",\"attempts\":"+axisTraceArray(attempts)+",\"completed\":"+(result.completed?"true":"false")+",\"safeToContinue\":"+(result.safeToContinue?"true":"false")+",\"error\":"+error+"}";
    return result;
}

void nativeFourAxisSeed(){
    try{
        quietRequireIsolatedWorker();
        const auto p=paths();const auto control=fullPath(p.output+L"\\control.dwg"),output=fullPath(p.output+L"\\four-axis-seed-generated.dwg"),report=fullPath(p.output+L"\\four-axis-seed-native.json");
        requireNewFile(output);requireNewFile(report);std::string error="null",originalBefore="[]",originalAfter="[]",originalSaved="[]",added="[]",savedAdded="[]";
        std::vector<std::string> cloneEvidence,seedBodies;std::vector<std::wstring> originalHandles;std::wstring axisHandles[2];AcDbObjectId ids[2];FourAxisPruneResult results[2];
        int initial=-1,final=-1,reopened=-1,save=-9999,read=-9999,closed=-9999;bool firstDestroyed=false,completed=false,safe=true;
        try{
            auto* doc=acDocManager?acDocManager->curDocument():nullptr;auto* db=doc?doc->database():nullptr;fourAxisGuard(p,doc,db);const auto originals=modelIds(*db);initial=originals.length();
            if(initial!=213)throw std::runtime_error("four-axis seed must begin in complete frozen scheme213 copy");originalBefore=familyCompactArray(originals);for(int i=0;i<originals.length();++i)originalHandles.push_back(handleOf(originals[i]));
            for(int i=0;i<2;++i){std::string evidence;ids[i]=fourAxisCloneTemplate(p,*db,i==0?L"199F":L"19A3",evidence);cloneEvidence.push_back(evidence);axisHandles[i]=handleOf(ids[i]);}
            quietPrepareOwnedView();
            for(int i=0;i<2;++i){AcDbObjectIdArray protectedIds;const auto all=modelIds(*db);for(int j=0;j<all.length();++j)if(all[j]!=ids[i])protectedIds.append(all[j]);results[i]=fourAxisPruneToFirst(p,doc,*db,ids[i],i==0,protectedIds);safe=safe&&results[i].safeToContinue;if(!results[i].completed){error=quote(L"single-station preflight failed; four-side generation was not attempted");break;}if(!safe)break;}
            if(!safe)throw std::runtime_error("four-axis callback cleanup unresolved; no further native operation or save attempted");
            fourAxisGuard(p,doc,db);originalAfter=familyCompactArray(originals);final=modelCount(*db);added=familyCompactArray(familyAdditions(*db,originals));
            if(originalAfter!=originalBefore)throw std::runtime_error("four-axis seed protected originals changed");
            requireNewFile(output);save=db->saveAs(output.c_str(),false,AcDb::kDHL_CURRENT);check(static_cast<Acad::ErrorStatus>(save),"save isolated four-axis seed result");
            {auto reader=std::make_unique<AcDbDatabase>(false,true);check(reader->readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false),"first read four-axis seed result");check(reader->closeInput(true),"close first four-axis seed input");reader.reset();firstDestroyed=true;}
            AcDbDatabase reader(false,true);read=reader.readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false);check(static_cast<Acad::ErrorStatus>(read),"fresh reader four-axis seed result");closed=reader.closeInput(true);check(static_cast<Acad::ErrorStatus>(closed),"close final four-axis seed input");
            {WorkingDatabaseScope scope(reader);reopened=modelCount(reader);AcDbObjectIdArray old;for(const auto& handle:originalHandles){AcDbObjectId id;check(reader.getAcDbObjectId(id,false,AcDbHandle(handle.c_str())),"four-axis seed persisted original");old.append(id);}originalSaved=familyCompactArray(old);savedAdded=familyCompactArray(familyAdditions(reader,old));
             for(int i=0;i<2;++i){AcDbObjectId id;check(reader.getAcDbObjectId(id,false,AcDbHandle(axisHandles[i].c_str())),"find saved native seed");const auto persisted=fourAxisObserve(id,i==0);auto body=results[i].json;if(body=="null")body="{\"orientation\":"+quote(i==0?L"vertical":L"horizontal")+",\"completed\":false}";body.pop_back();body+=",\"saved\":"+persisted.entity+"}";seedBodies.push_back(body);}}
            completed=error=="null"&&safe&&results[0].completed&&results[1].completed&&final==215&&reopened==215;
        }catch(const std::exception& e){if(error=="null")error=familyError(e);}catch(...){error=quote(L"unknown four-axis seed preflight exception");safe=false;}
        if(seedBodies.empty())for(const auto& r:results)if(r.json!="null")seedBodies.push_back(r.json);
        writeNewReport(report,"{\"contract\":\"stage1.four-axis-seed-native.v2\",\"api\":\"ObjectARX native; no COM\",\"schemeInputPath\":"+quote(p.scheme)+",\"referenceInputPath\":"+quote(p.reference)+",\"controlDWG\":"+quote(control)+",\"outputDWG\":"+quote(output)+",\"initialModelCount\":"+std::to_string(initial)+",\"finalModelCount\":"+std::to_string(final)+",\"reopenedModelCount\":"+std::to_string(reopened)+",\"originalBefore\":"+originalBefore+",\"originalAfter\":"+originalAfter+",\"originalSaved\":"+originalSaved+",\"clones\":"+axisTraceArray(cloneEvidence)+",\"seeds\":"+axisTraceArray(seedBodies)+",\"added\":"+added+",\"savedAdded\":"+savedAdded+",\"saveStatus\":"+std::to_string(save)+",\"reopenStatus\":"+std::to_string(read)+",\"closeInputStatus\":"+std::to_string(closed)+",\"firstReaderDestroyed\":"+(firstDestroyed?"true":"false")+",\"completed\":"+(completed?"true":"false")+",\"safeToContinue\":"+(safe?"true":"false")+",\"windowActions\":[],\"numberingAttempted\":false,\"error\":"+error+",\"scope\":\"two native single-station carriers only; full grid, continuous labels, project axis selection and rendered appearance require separate verification\"}");
    }catch(const std::exception& e){acutPrintf(L"\nSTAGE1_FOUR_AXIS_SEED_FAILED: %hs\n",e.what());}
}
