// Include after Family, native input trace and NativeTextBridgeProbe helpers.
// No extra SDK headers/libraries. No direct zoom/activation/visibility/logging
// or dialog calls and no TCH private writes. TReAxisNo is documented as point +
// string input; the caller must still audit the actual host/window behavior.
// Caller prepares a quiet owned document/view and supplies an actual public grip.

struct AxisContinuousNumberRequest {
    std::wstring id;
    AcDbObjectId axis;
    std::wstring number;
    AcGePoint3d pickWcs;
};
struct AxisContinuousNumberResult {
    std::string json;
    bool commandCompleted{};
    bool safeToContinue{};
};

bool axisContinuousAllowedStart(const std::wstring& text) {
    if (text.empty()) return false;
    bool digits = text.front() >= L'1' && text.front() <= L'9';
    if (digits) { for (const auto c : text) if (c < L'0' || c > L'9') return false; return true; }
    return text.size() == 1 && text[0] >= L'A' && text[0] <= L'Y' && text[0] != L'I' && text[0] != L'O';
}

std::string axisContinuousTraceMeta(const AxisNativeTraceData& trace) {
    std::ostringstream out;
    out << "{\"editorAdded\":" << (trace.editorAdded ? "true" : "false") << ",\"editorRemoved\":" << (trace.editorRemoved ? "true" : "false")
        << ",\"inputAddStatus\":" << (trace.inputAddReturned ? std::to_string(trace.inputAddStatus) : "null")
        << ",\"inputRemoveStatus\":" << (trace.inputRemoveReturned ? std::to_string(trace.inputRemoveStatus) : "null")
        << ",\"retained\":" << (trace.retained ? "true" : "false") << ",\"traceDropped\":" << (trace.traceDropped ? "true" : "false") << '}';
    return out.str();
}

bool axisContinuousObservedCommand(int status, const AxisNativeTraceData& trace, const std::wstring& handle) {
    if (status != RTNORM || trace.retained || trace.traceDropped || !trace.editorAdded || !trace.editorRemoved
        || !trace.inputAddReturned || trace.inputAddStatus != Acad::eOk || !trace.inputRemoveReturned
        || (trace.inputRemoveStatus != Acad::eOk && trace.inputRemoveStatus != Acad::eInvalidKey)
        || textBridgeHasUnbalancedCommandTrace(trace)) return false;
    int starts{}, ends{}; bool selected{};
    const auto wanted = utf8(handle);
    for (auto event : trace.commands) {
        for (auto& c : event) if (c >= 'a' && c <= 'z') c = static_cast<char>(c-'a'+'A');
        if (event.find("\"EVENT\":\"COMMANDCANCELLED\"") != std::string::npos || event.find("\"EVENT\":\"COMMANDFAILED\"") != std::string::npos || event.find("\"EVENT\":\"UNKNOWNCOMMAND\"") != std::string::npos) return false;
        const auto at = event.find("\"COMMAND\":\""); if (at == std::string::npos) continue;
        const auto first = at + 11, end = event.find('"', first); if (end == std::string::npos) return false;
        auto command = event.substr(first, end-first); const auto non = command.find_first_not_of("_.");
        if (non == std::string::npos || command.substr(non) != "TREAXISNO") continue;
        if (event.find("\"EVENT\":\"COMMANDWILLSTART\"") != std::string::npos) ++starts;
        if (event.find("\"EVENT\":\"COMMANDENDED\"") != std::string::npos) ++ends;
    }
    for (const auto& event : trace.inputs) {
        for (const auto field : {std::string("\"selectedHandle\":\""), std::string("\"selectedHandles\":[")}) {
            auto start = event.find(field); if (start == std::string::npos) continue; start += field.size();
            if (field.back() == '"') {
                const auto end = event.find('"', start); if (end == std::string::npos || event.substr(start,end-start) != wanted) return false; selected = true;
            } else {
                const auto end = event.find(']',start); if (end == std::string::npos) return false;
                for (auto first=event.find('"',start); first != std::string::npos && first<end;) {
                    const auto last=event.find('"',first+1); if (last == std::string::npos || last>end || event.substr(first+1,last-first-1) != wanted) return false;
                    selected=true;first=event.find('"',last+1);
                }
            }
        }
    }
    return starts == 1 && ends == 1 && selected;
}

AxisContinuousNumberResult axisContinuousRenumber(const Paths& p, AcApDocument* document, AcDbDatabase& database,
                                                  const AxisContinuousNumberRequest& request) {
    const auto control=fullPath(p.output+L"\\control.dwg");
    std::string error{"null"}, before{"null"}, after{"null"}, dxfBefore{"null"}, dxfAfter{"null"}, commandStatus{"null"}, converted{"null"};
    int transactionsBefore=-1,transactionsAfter=-1;bool attempted{},pickMatched{},guardValid{};
    AcGePoint3d user;auto trace=std::make_shared<AxisNativeTraceData>();const auto handle=request.axis.isNull()?std::wstring():handleOf(request.axis);
    const auto guard=[&]{
        quietRequireIsolatedWorker();
        if(!acDocManager || !document || acDocManager->isApplicationContext() || acDocManager->curDocument()!=document
            || acDocManager->mdiActiveDocument()!=document || document->database()!=&database
            || acdbHostApplicationServices()->workingDatabase()!=&database || !document->fileName()
            || _wcsicmp(fullPath(document->fileName()).c_str(),control.c_str()) || !below(control,p.output))
            throw std::runtime_error("continuous number requires the owned current/MDI/working control document");
        const ACHAR* name{};check(database.getFilename(name),"continuous number database filename");
        if(!name || _wcsicmp(fullPath(name).c_str(),control.c_str()) || !database.transactionManager()
            || database.transactionManager()->numActiveTransactions()!=0)throw std::runtime_error("continuous number native filename/transaction guard failed");
        if(request.axis.isNull() || request.axis.database()!=&database)throw std::runtime_error("continuous number target is not owned by current control database");
    };
    int returned=-9999;bool completed{},safe{};
    try {
        guard();guardValid=true;
        if(!axisContinuousAllowedStart(request.number))throw std::runtime_error("continuous number start must be positive digits or A-Y excluding I/O/Z; overflow policy not guessed");
        {
            auto entity=open<AcDbEntity>(request.axis);if(_wcsicmp(entity->isA()->name(),L"TDbAxisLabelSet") || entity->isAProxy())throw std::runtime_error("continuous number target is not a native TDbAxisLabelSet");
            AcGePoint3dArray grips;AcDbIntArray snaps,geometry;check(entity->getGripPoints(grips,snaps,geometry),"continuous number actual public grips");
            for(int i=0;i<grips.length();++i)if(grips[i].distanceTo(request.pickWcs)<=1e-6)pickMatched=true;
            if(!pickMatched)throw std::runtime_error("continuous number pick does not match an actual public target grip");
        }
        before=familyCompactEntity(request.axis);dxfBefore=publicAxisDxf(request.axis);
        const auto transformed=textBridgeToUcs(request.pickWcs,user);converted=std::to_string(transformed);
        if(transformed!=RTNORM)throw std::runtime_error("continuous number WCS-to-UCS conversion failed");
        guard();transactionsBefore=database.transactionManager()->numActiveTransactions();
        {
            AxisNativeTraceOwner owner(document,trace);
            if(!trace->editorAdded || !trace->inputAddReturned || trace->inputAddStatus!=Acad::eOk || trace->traceDropped)throw std::runtime_error("continuous number trace unavailable; public command not attempted");
            ads_point pointIn{user.x,user.y,user.z};attempted=true;
            returned=acedCommandS(RTSTR,L"TReAxisNo",RT3DPOINT,pointIn,RTSTR,request.number.c_str(),RTSTR,L"",RTNONE);
            commandStatus=std::to_string(returned);
        }
        if(trace->retained || textBridgeHasUnbalancedCommandTrace(*trace))throw std::runtime_error("continuous number command trace unsafe; further host actions withheld");
        guard();transactionsAfter=database.transactionManager()->numActiveTransactions();
        after=familyCompactEntity(request.axis);dxfAfter=publicAxisDxf(request.axis);
        completed=axisContinuousObservedCommand(returned,*trace,handle);
        if(!completed)error=quote(L"public numbering command completion/actual target selection not verified");
        safe=true;
    }catch(const std::exception& failure){error=familyError(failure);}
     catch(...){error=quote(L"unknown native continuous number exception");}
    if(trace->retained || textBridgeHasUnbalancedCommandTrace(*trace))safe=false;
    else if(!safe && guardValid){try{guard();safe=true;}catch(...){safe=false;}}
    std::ostringstream out;
    out<<"{\"contract\":\"stage1.axis-continuous-number-operation.v1\",\"api\":\"ObjectARX native + public Tianzheng command; no COM\",\"id\":"<<quote(request.id)
       <<",\"axisHandle\":"<<quote(handle)<<",\"requestedNumber\":"<<quote(request.number)<<",\"commandName\":\"TReAxisNo\",\"commandAttempted\":"<<(attempted?"true":"false")
       <<",\"commandStatus\":"<<commandStatus<<",\"coordinateTransformStatus\":"<<converted<<",\"pickWcs\":"<<point(request.pickWcs)<<",\"pickUcs\":"<<point(user)
       <<",\"pickMatchesActualPublicGrip\":"<<(pickMatched?"true":"false")<<",\"activeTransactionsBefore\":"<<transactionsBefore<<",\"activeTransactionsAfter\":"<<transactionsAfter
       <<",\"before\":"<<before<<",\"after\":"<<after<<",\"publicDxfBefore\":"<<dxfBefore<<",\"publicDxfAfter\":"<<dxfAfter
       <<",\"tokens\":[\"TReAxisNo\",\"actual grip WCS converted to UCS\","<<quote(request.number)<<",\"\"],\"commandTrace\":"<<axisTraceArray(trace->commands)<<",\"inputTrace\":"<<axisTraceArray(trace->inputs)
       <<",\"trace\":"<<axisContinuousTraceMeta(*trace)<<",\"commandCompleted\":"<<(completed?"true":"false")<<",\"safeToContinue\":"<<(safe?"true":"false")
       <<",\"numberSemantics\":\"pending rendered circles or independently confirmed public text evidence\",\"error\":"<<error<<'}';
    return {out.str(),completed,safe};
}

AcGePoint3d axisContinuousFirstPick(AcDbObjectId id,bool horizontal) {
    AcGePoint3dArray grips,stretch;AcDbIntArray snaps,geometry;
    {auto e=open<AcDbEntity>(id);check(e->getGripPoints(grips,snaps,geometry),"continuous fixture actual grips");check(e->getStretchPoints(stretch),"continuous fixture actual stretch points");}
    if(grips.isEmpty()||stretch.isEmpty())throw std::runtime_error("continuous fixture public points absent");
    double station=std::numeric_limits<double>::infinity();for(int i=0;i<stretch.length();++i)station=(std::min)(station,horizontal?stretch[i].y:stretch[i].x);
    AcGePoint3d chosen;double outside=std::numeric_limits<double>::infinity();bool found{};
    for(int i=0;i<grips.length();++i){
        if(std::fabs((horizontal?grips[i].y:grips[i].x)-station)>1e-6)continue;
        bool shared{};for(int j=0;j<stretch.length();++j)if(grips[i].distanceTo(stretch[j])<=1e-6)shared=true;
        const double candidate=horizontal?grips[i].x:grips[i].y;
        if(shared&&candidate<outside){chosen=grips[i];outside=candidate;found=true;}
    }
    if(!found)throw std::runtime_error("continuous fixture first lower/left circle edge not present in actual grip/stretch arrays");return chosen;
}

void nativeAxisContinuousNumberFixture(const wchar_t* mode,bool horizontal,const wchar_t* start) {
    try {
        quietRequireIsolatedWorker();
        const auto p=paths();const auto control=fullPath(p.output+L"\\control.dwg"),output=fullPath(p.output+L"\\axis-number-"+mode+L"-generated.dwg"),report=fullPath(p.output+L"\\axis-continuous-number-native.json");
        requireNewFile(report);requireNewFile(output);std::string error{"null"},before{"[]"},after{"[]"},savedOriginal{"[]"},axisAfter{"null"},axisSaved{"null"},operation{"null"};
        int initial=-1,final=-1,reopened=-1,sourceCount=-1;std::string stored{"null"},loaded{"null"},closed{"null"};
        try {
            if(!acDocManager || acDocManager->isApplicationContext())throw std::runtime_error("continuous fixture needs owned document command context");
            auto* document=acDocManager->curDocument();auto* database=document?document->database():nullptr;
            if(!database||!document->fileName()||_wcsicmp(fullPath(document->fileName()).c_str(),control.c_str())||acDocManager->mdiActiveDocument()!=document||acdbHostApplicationServices()->workingDatabase()!=database)throw std::runtime_error("continuous fixture owned control identity guard failed");
            const auto ids=modelIds(*database);initial=ids.length();if(initial!=214)throw std::runtime_error("continuous fixture requires213 originals plus one native axis collection");
            AcDbObjectId axis;AcDbObjectIdArray originals;std::vector<std::wstring> handles;
            for(int i=0;i<ids.length();++i){auto e=open<AcDbEntity>(ids[i]);if(!_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet")){if(!axis.isNull())throw std::runtime_error("continuous fixture found multiple axis collections");axis=ids[i];}else{originals.append(ids[i]);handles.push_back(handleOf(ids[i]));}}
            sourceCount=originals.length();if(axis.isNull()||sourceCount!=213)throw std::runtime_error("continuous fixture native target/protected scheme coverage absent");
            before=familyCompactArray(originals);quietPrepareOwnedView();const auto pick=axisContinuousFirstPick(axis,horizontal);
            const AxisContinuousNumberRequest request{mode,axis,start,pick};const auto result=axisContinuousRenumber(p,document,*database,request);operation=result.json;
            if(!result.safeToContinue)throw std::runtime_error("continuous fixture cannot safely continue after command trace");
            after=familyCompactArray(originals);axisAfter=familyCompactEntity(axis);final=modelCount(*database);
            if(!result.commandCompleted)error=quote(L"numbering structure failed; isolated diagnostic state preserved");
            requireNewFile(output);const auto saved=database->saveAs(output.c_str(),false,AcDb::kDHL_CURRENT);stored=std::to_string(saved);check(saved,"save continuous fixture isolated output");
            AcDbDatabase reader(false,true);const auto read=reader.readDwgFile(output.c_str(),AcDbDatabase::kForReadAndAllShare,false);loaded=std::to_string(read);check(read,"new reader continuous fixture output");
            const auto close=reader.closeInput(true);closed=std::to_string(close);check(close,"close continuous fixture input");
            {WorkingDatabaseScope work(reader);reopened=modelCount(reader);AcDbObjectIdArray prior;
             for(const auto& handle:handles){AcDbObjectId id;check(reader.getAcDbObjectId(id,false,AcDbHandle(handle.c_str())),"continuous persisted original");prior.append(id);}
             savedOriginal=familyCompactArray(prior);AcDbObjectId copy;check(reader.getAcDbObjectId(copy,false,AcDbHandle(handleOf(axis).c_str())),"continuous persisted axis");axisSaved=familyCompactEntity(copy);}
        }catch(const std::exception& failure){error=familyError(failure);}catch(...){error=quote(L"unknown continuous fixture exception");}
        std::ostringstream out;out<<"{\"contract\":\"stage1.axis-continuous-number-native.v1\",\"api\":\"ObjectARX native + public Tianzheng command; no COM\",\"controlDWG\":"<<quote(control)<<",\"outputDWG\":"<<quote(output)<<",\"fixtureMode\":"<<quote(mode)
            <<",\"orientation\":"<<quote(horizontal?L"horizontal":L"vertical")<<",\"initialModelCount\":"<<initial<<",\"finalModelCount\":"<<final<<",\"reopenedModelCount\":"<<reopened<<",\"sourceCount\":"<<sourceCount
            <<",\"originalBefore\":"<<before<<",\"originalAfter\":"<<after<<",\"originalSaved\":"<<savedOriginal<<",\"operation\":"<<operation<<",\"axisAfter\":"<<axisAfter<<",\"axisSaved\":"<<axisSaved
            <<",\"saveStatus\":"<<stored<<",\"reopenStatus\":"<<loaded<<",\"closeInputStatus\":"<<closed<<",\"semanticVerification\":\"pending native PDF circle-number verification\",\"error\":"<<error<<'}';writeNewReport(report,out.str());
    }catch(const std::exception& failure){acutPrintf(L"\nSTAGE1_CONTINUOUS_NUMBER_FAILED: %hs\n",failure.what());}
}
void nativeAxisContinuousDigits(){nativeAxisContinuousNumberFixture(L"digits",false,L"1");}
void nativeAxisContinuousLetters(){nativeAxisContinuousNumberFixture(L"letters",true,L"A");}
void nativeAxisContinuousSkipIO(){nativeAxisContinuousNumberFixture(L"skip-io",true,L"G");}


struct AxisContinuousMergeResult {
    std::string json;
    bool commandCompleted{};
    bool safeToContinue{};
    AcDbObjectId mergedAxis;
};
struct AxisContinuousRbRelease { void operator()(resbuf* value) const noexcept { if(value)acutRelRb(value); } };

bool axisContinuousPointBag(const AcGePoint3dArray& a,const AcGePoint3dArray& b) {
    if(a.length()!=b.length())return false;std::vector<bool> used(b.length(),false);
    for(int i=0;i<a.length();++i){bool found{};for(int j=0;j<b.length();++j)if(!used[j]&&a[i].distanceTo(b[j])<=1e-6){used[j]=true;found=true;break;}if(!found)return false;}return true;
}
bool axisContinuousPickOnSingleStation(const AcGePoint3dArray& grip,const AcGePoint3dArray& stretch,const AcGePoint3d& pick,bool vertical) {
    for(int i=0;i<grip.length();++i)if(grip[i].distanceTo(pick)<=1e-6)return true;
    if(stretch.length()!=4)return false;const double station=vertical?stretch[0].x:stretch[0].y,z=stretch[0].z;
    if(std::fabs((vertical?pick.x:pick.y)-station)>1e-6||std::fabs(pick.z-z)>1e-6)return false;
    std::vector<double> along;
    for(int i=0;i<stretch.length();++i){if(std::fabs((vertical?stretch[i].x:stretch[i].y)-station)>1e-6||std::fabs(stretch[i].z-z)>1e-6)return false;along.push_back(vertical?stretch[i].y:stretch[i].x);}
    std::sort(along.begin(),along.end());const double value=vertical?pick.y:pick.x;
    // The four public stretch points give the two exterior leader segments.
    // The central gap across the building is deliberately excluded.
    return (value>=along[0]-1e-6&&value<=along[1]+1e-6)||(value>=along[2]-1e-6&&value<=along[3]+1e-6);
}

// A single station has six real circle/leader grips and two auxiliary UI
// controls. The latter are at the two near-circle stretch edges plus/minus R
// on the station coordinate. They are NOT circle circumference tangent points.
// Only the two joint states are allowed, never independent signs or arbitrary
// removal of unmatched grips. Circle/station geometry and bounds stay strict.
struct AxisContinuousAuxiliaryPolicy {
    bool valid{},vertical{};double station{},radius{};int sourceSign{};
    AcGePoint3dArray negativeState,positiveState;
    std::string json() const {
        return "{\"contract\":\"stage1.axis-auxiliary-control-states.v1\",\"declaredBeforeCommand\":true,\"valid\":"+std::string(valid?"true":"false")
            +",\"baseStation\":"+number(station)+",\"radius\":"+number(radius)+",\"sourceSign\":"+std::to_string(sourceSign)
            +",\"negativeState\":"+points(negativeState)+",\"positiveState\":"+points(positiveState)
            +",\"mixedSignsAllowed\":false,\"scope\":\"only two auxiliary controls; all station/circle grips, stretch points and bounds remain strict\"}";
    }
};
bool axisContinuousHasPoint(const AcGePoint3dArray& values,const AcGePoint3d& point) {
    for(int i=0;i<values.length();++i)if(values[i].distanceTo(point)<=1e-6)return true;return false;
}
AxisContinuousAuxiliaryPolicy axisContinuousSingleAuxiliaryPolicy(const AcGePoint3dArray& grip,const AcGePoint3dArray& stretch,const AcDbExtents& bounds,bool vertical) {
    AxisContinuousAuxiliaryPolicy p;p.vertical=vertical;if(grip.length()!=8||stretch.length()!=4)return p;p.station=vertical?stretch[0].x:stretch[0].y;
    const auto finite=[](const AcGePoint3d& value){return std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z);};
    if(!finite(bounds.minPoint())||!finite(bounds.maxPoint()))return p;
    for(int i=0;i<grip.length();++i)if(!finite(grip[i])||std::fabs(grip[i].z-stretch[0].z)>1e-6)return p;
    for(int i=0;i<stretch.length();++i){if(!finite(stretch[i])||std::fabs((vertical?stretch[i].x:stretch[i].y)-p.station)>1e-6||std::fabs(stretch[i].z-stretch[0].z)>1e-6||!axisContinuousHasPoint(grip,stretch[i]))return p;for(int j=0;j<i;++j)if(stretch[i].distanceTo(stretch[j])<=1e-6)return p;}
    AcGePoint3dArray centers,aux;
    for(int i=0;i<grip.length();++i){
        if(std::fabs((vertical?grip[i].x:grip[i].y)-p.station)<=1e-6){if(!axisContinuousHasPoint(stretch,grip[i]))centers.append(grip[i]);}
        else aux.append(grip[i]);
    }
    if(centers.length()!=2||aux.length()!=2)return p;
    auto low=centers[0],high=centers[1];if((vertical?low.y:low.x)>(vertical?high.y:high.x))std::swap(low,high);
    const auto lo=bounds.minPoint(),hi=bounds.maxPoint();const double a=vertical?low.y-lo.y:low.x-lo.x,b=vertical?hi.y-high.y:hi.x-high.x;
    if(!std::isfinite(a)||a<=1e-6||!std::isfinite(b)||std::fabs(a-b)>1e-6)return p;p.radius=(a+b)/2;
    if(std::fabs((vertical?lo.x:lo.y)-(p.station-p.radius))>1e-6||std::fabs((vertical?hi.x:hi.y)-(p.station+p.radius))>1e-6||std::fabs(low.z-high.z)>1e-6||std::fabs(lo.z-low.z)>1e-6||std::fabs(hi.z-low.z)>1e-6)return p;
    if(vertical){low.y+=p.radius;high.y-=p.radius;}else{low.x+=p.radius;high.x-=p.radius;}
    if(!axisContinuousHasPoint(stretch,low)||!axisContinuousHasPoint(stretch,high))return p;
    for(int i=0;i<2;++i){auto base=i==0?low:high,negative=base,positive=base;if(vertical){negative.x=p.station-p.radius;positive.x=p.station+p.radius;}else{negative.y=p.station-p.radius;positive.y=p.station+p.radius;}p.negativeState.append(negative);p.positiveState.append(positive);}
    if(axisContinuousPointBag(aux,p.negativeState))p.sourceSign=-1;else if(axisContinuousPointBag(aux,p.positiveState))p.sourceSign=1;else return p;
    p.valid=true;return p;
}
bool axisContinuousSubtractPointBag(const AcGePoint3dArray& actual,const AcGePoint3dArray& required,AcGePoint3dArray& remainder) {
    std::vector<bool> used(actual.length(),false);
    for(int i=0;i<required.length();++i){bool found{};for(int j=0;j<actual.length();++j)if(!used[j]&&required[i].distanceTo(actual[j])<=1e-6){used[j]=true;found=true;break;}if(!found)return false;}
    for(int i=0;i<actual.length();++i)if(!used[i])remainder.append(actual[i]);return true;
}

std::vector<std::wstring> axisContinuousObservedSelections(const AxisNativeTraceData& trace) {
    std::vector<std::wstring> values;
    const auto append=[&](const std::string& v){std::wstring w(v.begin(),v.end());if(values.empty()||values.back()!=w)values.push_back(w);};
    for(const auto& e:trace.inputs){
        const std::string single="\"selectedHandle\":\"",multi="\"selectedHandles\":[";auto at=e.find(single);
        if(at!=std::string::npos){at+=single.size();const auto end=e.find('"',at);if(end!=std::string::npos)append(e.substr(at,end-at));}
        at=e.find(multi);if(at!=std::string::npos){at+=multi.size();const auto end=e.find(']',at);if(end==std::string::npos)throw std::runtime_error("merge trace selection array incomplete");
            for(auto first=e.find('"',at);first!=std::string::npos&&first<end;){const auto last=e.find('"',first+1);if(last==std::string::npos||last>end)throw std::runtime_error("merge trace selection string incomplete");append(e.substr(first+1,last-first-1));first=e.find('"',last+1);}}
    }return values;
}

AxisContinuousMergeResult axisContinuousMerge(const Paths& p,AcApDocument* document,AcDbDatabase& database,
                                               const std::vector<AxisContinuousNumberRequest>& requests,bool vertical) {
    const auto control=fullPath(p.output+L"\\control.dwg");auto trace=std::make_shared<AxisNativeTraceData>();
    std::string error{"null"},before{"[]"},after{"[]"},protectedBefore{"[]"},protectedAfter{"[]"},commandStatus{"null"};
    std::vector<std::wstring> handles;AcGePoint3dArray picks,users,expectedStretch,expectedStationGrips,actualAuxiliary;AcDbIntArray transforms;
    int countBefore=-1,countAfter=-1,txBefore=-1,txAfter=-1,auxiliaryAfterSign{};bool attempted{},pointsMatched{},stationGripsMatched{},auxiliaryMatched{},boundsMatched{},completed{},safe{},guardValid{};AcDbObjectId merged;AxisContinuousAuxiliaryPolicy auxiliaryPolicy;
    AcGePoint3d expectedMin(std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()),expectedMax(-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity(),-std::numeric_limits<double>::infinity());
    const auto guard=[&]{
        quietRequireIsolatedWorker();
        if(!acDocManager||!document||acDocManager->isApplicationContext()||acDocManager->curDocument()!=document||acDocManager->mdiActiveDocument()!=document||document->database()!=&database||acdbHostApplicationServices()->workingDatabase()!=&database||!document->fileName()||_wcsicmp(fullPath(document->fileName()).c_str(),control.c_str())||!below(control,p.output))throw std::runtime_error("axis merge owned current/MDI/working control guard failed");
        const ACHAR* name{};check(database.getFilename(name),"axis merge filename");if(!name||_wcsicmp(fullPath(name).c_str(),control.c_str())||!database.transactionManager()||database.transactionManager()->numActiveTransactions()!=0)throw std::runtime_error("axis merge filename/zero-transaction guard failed");
    };
    try{
        guard();guardValid=true;if(requests.size()<2)throw std::runtime_error("axis merge requires at least two distinct same-direction single-station collections");
        const auto all=modelIds(database);countBefore=all.length();AcDbObjectIdArray targets,protectedIds;
        double prior=-std::numeric_limits<double>::infinity();std::ostringstream source;source<<'[';
        for(std::size_t i=0;i<requests.size();++i){
            const auto& req=requests[i];if(req.axis.isNull()||req.axis.database()!=&database)throw std::runtime_error("axis merge request is not owned by control database");
            for(int j=0;j<targets.length();++j)if(targets[j]==req.axis)throw std::runtime_error("axis merge request repeats a collection");targets.append(req.axis);handles.push_back(handleOf(req.axis));
            AcGePoint3dArray grip,stretch;AcDbIntArray snap,geo;AcDbExtents sourceBounds;
            {auto e=open<AcDbEntity>(req.axis);if(e->isAProxy()||_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet"))throw std::runtime_error("axis merge input is not a real TDbAxisLabelSet");check(e->getGripPoints(grip,snap,geo),"axis merge actual input grips");check(e->getStretchPoints(stretch),"axis merge actual input stretch");check(e->getGeomExtents(sourceBounds),"axis merge actual input bounds");}
            if(stretch.length()!=4||grip.length()!=8)throw std::runtime_error("axis merge inputs must each contain one complete two-sided station");
            const double station=vertical?stretch[0].x:stretch[0].y;if(!std::isfinite(station)||station<=prior+1e-6)throw std::runtime_error("axis merge input stations must be distinct and ascending");prior=station;
            const auto policy=axisContinuousSingleAuxiliaryPolicy(grip,stretch,sourceBounds,vertical);if(!policy.valid)throw std::runtime_error("axis merge input auxiliary controls/radius/bounds do not match declared joint states");
            if(i==0)auxiliaryPolicy=policy;else if(std::fabs(policy.radius-auxiliaryPolicy.radius)>1e-6)throw std::runtime_error("axis merge input circle radii differ");
            const auto lo=sourceBounds.minPoint(),hi=sourceBounds.maxPoint();expectedMin=AcGePoint3d((std::min)(expectedMin.x,lo.x),(std::min)(expectedMin.y,lo.y),(std::min)(expectedMin.z,lo.z));expectedMax=AcGePoint3d((std::max)(expectedMax.x,hi.x),(std::max)(expectedMax.y,hi.y),(std::max)(expectedMax.z,hi.z));
            bool pick{};int onStation=0;
            for(int j=0;j<grip.length();++j){if(grip[j].distanceTo(req.pickWcs)<=1e-6)pick=true;
                if(std::fabs((vertical?grip[j].x:grip[j].y)-station)<=1e-6){expectedStationGrips.append(grip[j]);++onStation;}
}
            pick=pick||axisContinuousPickOnSingleStation(grip,stretch,req.pickWcs,vertical);
            if(!pick||onStation!=6)throw std::runtime_error("axis merge input public grip/exterior-leader pick or six station grips do not match single-station contract");
            for(int j=0;j<stretch.length();++j){if(std::fabs((vertical?stretch[j].x:stretch[j].y)-station)>1e-6)throw std::runtime_error("axis merge input contains hidden extra station");expectedStretch.append(stretch[j]);}
            if(i)source<<',';source<<familyCompactEntity(req.axis);picks.append(req.pickWcs);AcGePoint3d user;const auto status=textBridgeToUcs(req.pickWcs,user);transforms.append(status);if(status!=RTNORM)throw std::runtime_error("axis merge WCS-to-UCS point conversion failed");users.append(user);
        }
        source<<']';before=source.str();
        for(int i=0;i<all.length();++i){bool target{};for(int j=0;j<targets.length();++j)if(all[i]==targets[j])target=true;if(!target)protectedIds.append(all[i]);}
        if(protectedIds.length()+targets.length()!=all.length())throw std::runtime_error("axis merge target must be actual ModelSpace member");protectedBefore=familyCompactArray(protectedIds);
        std::unique_ptr<resbuf,AxisContinuousRbRelease> chain;resbuf* tail{};
        const auto append=[&](resbuf* next){if(!next)throw std::runtime_error("axis merge complete-command chain allocation failed");if(!chain)chain.reset(next);else tail->rbnext=next;tail=next;while(tail->rbnext)tail=tail->rbnext;};
        append(acutBuildList(RTSTR,L"TLabelMerge",RTNONE));
        for(int i=0;i<users.length();++i){ads_point pt{users[i].x,users[i].y,users[i].z};append(acutBuildList(RT3DPOINT,pt,RTNONE));}
        append(acutBuildList(RTSTR,L"",RTNONE));guard();txBefore=database.transactionManager()->numActiveTransactions();int status=-9999;
        {AxisNativeTraceOwner owner(document,trace);if(!trace->editorAdded||!trace->inputAddReturned||trace->inputAddStatus!=Acad::eOk||trace->traceDropped)throw std::runtime_error("axis merge trace unavailable; command not attempted");attempted=true;status=acedCmdS(chain.get());commandStatus=std::to_string(status);}
        if(trace->retained||textBridgeHasUnbalancedCommandTrace(*trace))throw std::runtime_error("axis merge unsafe command/trace lifetime; further host actions withheld");
        guard();txAfter=database.transactionManager()->numActiveTransactions();const auto now=modelIds(database);countAfter=now.length();AcDbObjectIdArray resulting;
        for(int i=0;i<now.length();++i){bool protectedOne{};for(int j=0;j<protectedIds.length();++j)if(now[i]==protectedIds[j])protectedOne=true;if(!protectedOne)resulting.append(now[i]);}
        protectedAfter=familyCompactArray(protectedIds);after=familyCompactArray(resulting);
        if(resulting.length()==1){
            merged=resulting[0];AcGePoint3dArray grip,stretch;AcDbIntArray snap,geo;AcDbExtents actualBounds;bool boundsRead{};
            {auto e=open<AcDbEntity>(merged);if(!e->isAProxy()&&!_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet")){check(e->getGripPoints(grip,snap,geo),"axis merge actual output grips");check(e->getStretchPoints(stretch),"axis merge actual output stretch");check(e->getGeomExtents(actualBounds),"axis merge actual output bounds");boundsRead=true;}}
            stationGripsMatched=axisContinuousSubtractPointBag(grip,expectedStationGrips,actualAuxiliary);
            if(stationGripsMatched&&actualAuxiliary.length()==2){if(axisContinuousPointBag(actualAuxiliary,auxiliaryPolicy.negativeState))auxiliaryAfterSign=-1;else if(axisContinuousPointBag(actualAuxiliary,auxiliaryPolicy.positiveState))auxiliaryAfterSign=1;}
            auxiliaryMatched=auxiliaryPolicy.valid&&auxiliaryAfterSign!=0;
            boundsMatched=boundsRead&&actualBounds.minPoint().distanceTo(expectedMin)<=1e-6&&actualBounds.maxPoint().distanceTo(expectedMax)<=1e-6;
            pointsMatched=axisContinuousPointBag(expectedStretch,stretch)&&stationGripsMatched&&auxiliaryMatched&&boundsMatched;
        }
        int starts{},ends{};bool traceFailed{};
        for(auto event:trace->commands){for(auto& c:event)if(c>='a'&&c<='z')c=static_cast<char>(c-'a'+'A');if(event.find("\"EVENT\":\"COMMANDCANCELLED\"")!=std::string::npos||event.find("\"EVENT\":\"COMMANDFAILED\"")!=std::string::npos||event.find("\"EVENT\":\"UNKNOWNCOMMAND\"")!=std::string::npos)traceFailed=true;
            const auto at=event.find("\"COMMAND\":\"");if(at==std::string::npos)continue;const auto first=at+11,end=event.find('"',first);if(end==std::string::npos)continue;const auto name=event.substr(first,end-first);const auto non=name.find_first_not_of("_.");if(non==std::string::npos||name.substr(non)!="TLABELMERGE")continue;
            if(event.find("\"EVENT\":\"COMMANDWILLSTART\"")!=std::string::npos)++starts;if(event.find("\"EVENT\":\"COMMANDENDED\"")!=std::string::npos)++ends;}
        const auto selected=axisContinuousObservedSelections(*trace);bool selection=!selected.empty()&&selected.front()==handles.front();
        for(const auto& h:selected){bool found{};for(const auto& wanted:handles)if(h==wanted)found=true;selection=selection&&found;}
        for(const auto& h:handles){bool found{};for(const auto& seen:selected)if(h==seen)found=true;selection=selection&&found;}
        const bool detached=trace->editorAdded&&trace->editorRemoved&&trace->inputAddReturned&&trace->inputAddStatus==Acad::eOk&&trace->inputRemoveReturned&&(trace->inputRemoveStatus==Acad::eOk||trace->inputRemoveStatus==Acad::eInvalidKey)&&!trace->traceDropped;
        completed=status==RTNORM&&starts==1&&ends==1&&!traceFailed&&selection&&detached&&pointsMatched&&protectedBefore==protectedAfter&&countAfter==countBefore-static_cast<int>(requests.size())+1;
        if(!completed)error=quote(L"axis merge command selection/type/station-point union not verified; inspect actual trace/output without repair");safe=true;
    }catch(const std::exception& failure){error=familyError(failure);}catch(...){error=quote(L"unknown native axis merge exception");}
    if(trace->retained||textBridgeHasUnbalancedCommandTrace(*trace))safe=false;else if(!safe&&guardValid){try{guard();safe=true;}catch(...){safe=false;}}
    std::ostringstream out;out<<"{\"contract\":\"stage1.axis-merge-operation.v2\",\"api\":\"ObjectARX native + public Tianzheng command; no COM\",\"commandName\":\"TLabelMerge\",\"orientation\":"<<quote(vertical?L"vertical":L"horizontal")
        <<",\"requestedCollectionCount\":"<<requests.size()<<",\"targetHandles\":[";for(std::size_t i=0;i<handles.size();++i){if(i)out<<',';out<<quote(handles[i]);}
    out<<"],\"beforeSets\":"<<before<<",\"afterSets\":"<<after<<",\"protectedBefore\":"<<protectedBefore<<",\"protectedAfter\":"<<protectedAfter<<",\"beforeModelCount\":"<<countBefore<<",\"afterModelCount\":"<<countAfter
       <<",\"pickWcs\":"<<points(picks)<<",\"pickUcs\":"<<points(users)<<",\"pointTransformStatuses\":"<<integers(transforms)<<",\"activeTransactionsBefore\":"<<txBefore<<",\"activeTransactionsAfter\":"<<txAfter<<",\"commandAttempted\":"<<(attempted?"true":"false")
       <<",\"commandStatus\":"<<commandStatus<<",\"commandTrace\":"<<axisTraceArray(trace->commands)<<",\"inputTrace\":"<<axisTraceArray(trace->inputs)<<",\"trace\":"<<axisContinuousTraceMeta(*trace)<<",\"pointUnionMatched\":"<<(pointsMatched?"true":"false")
       <<",\"auxiliaryControlPolicy\":"<<auxiliaryPolicy.json()<<",\"actualAuxiliaryPoints\":"<<points(actualAuxiliary)<<",\"auxiliaryAfterSign\":"<<auxiliaryAfterSign<<",\"stationGripsMatched\":"<<(stationGripsMatched?"true":"false")<<",\"auxiliaryMatched\":"<<(auxiliaryMatched?"true":"false")<<",\"boundsMatched\":"<<(boundsMatched?"true":"false")<<",\"expectedMergedBounds\":{\"min\":"<<point(expectedMin)<<",\"max\":"<<point(expectedMax)<<'}'
       <<",\"mergedAxisHandle\":"<<(merged.isNull()?std::string("null"):quote(handleOf(merged)))<<",\"commandCompleted\":"<<(completed?"true":"false")<<",\"safeToContinue\":"<<(safe?"true":"false")<<",\"numberSemantics\":\"not checked by merge; renumber and render both sides separately\",\"error\":"<<error<<'}';
    return {out.str(),completed,safe,merged};
}
