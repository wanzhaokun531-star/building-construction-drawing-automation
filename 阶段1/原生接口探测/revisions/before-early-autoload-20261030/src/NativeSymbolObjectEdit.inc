// Public TObjEdit + actual modal standard fields. No creation-cache assumptions.
struct SymbolObjectEditState {SymbolUiOperation* operation{};std::wstring handle;UINT_PTR timer{};bool inside{},buttonPosted{},cancelPosted{},timerStopped{},reportedWindows{};};
SymbolObjectEditState* activeSymbolObjectEdit{};
void CALLBACK symbolObjectEditTimer(HWND,UINT,UINT_PTR timer,DWORD)noexcept{
    auto* state=activeSymbolObjectEdit;if(!state||state->timer!=timer||state->inside||state->buttonPosted)return;auto* s=state->operation;if(!s)return;state->inside=true;
    try{
        const auto selected=axisContinuousObservedSelections(*s->trace);if(selected.size()!=1||selected[0]!=state->handle){state->inside=false;return;}
        s->editingSelectedOwnedTarget=true;
        if(!state->reportedWindows){std::vector<std::string> panels;for(const auto& panel:elevationDialogOwnedSnapshots())if(!panel.isMainFrame)panels.push_back(elevationDialogSnapshotJson(panel));s->events.push_back("{\"event\":\"actual-object-edit-window-inventory\",\"panels\":"+axisTraceArray(panels)+"}");state->reportedWindows=true;symbolUiProgress(*s,L"actual-object-edit-panels-recorded");}
        const auto panel=symbolUiFresh(*s);
        if(IsWindowEnabled(elevationDialogKnownMainFrame))throw std::runtime_error("object-edit panel is not the actual modal editor");
        s->events.push_back("{\"event\":\"actual-owned-object-edit-modal\",\"selectedHandle\":"+quote(state->handle)+",\"nativeTransactionDepth\":"+std::to_string(s->db->transactionManager()->numActiveTransactions())+"}");
        if(s->kind==L"slope"&&s->id.rfind(L"restore",0)!=0){const auto current=symbolUiReadFields(*s,panel);s->desired[L"lower"]=current.at(L"lower");}
        symbolUiCapture(*s,L"actual-owned-object-edit-modal");
        const auto fresh=symbolUiFresh(*s);HWND button{};int count=0;const bool okay=s->configurationVerified&&s->error=="null";
        for(const auto& c:fresh.controls)if(c.visible&&c.enabled&&c.className==L"Button"&&elevationDialogCaption(c.text,okay?L"确定":L"取消",okay?L"OK":L"Cancel")){button=c.hwnd;++count;}
        if(count!=1||!IsWindow(button))throw std::runtime_error("actual editor OK/Cancel role not unique");
        symbolUiProgress(*s,okay?L"before-actual-editor-OK":L"before-actual-editor-cancel");
        state->buttonPosted=PostMessageW(button,BM_CLICK,0,0)!=FALSE;state->cancelPosted=!okay&&state->buttonPosted;
        s->events.push_back("{\"event\":\"post-actual-native-editor-button\",\"caption\":"+quote(elevationDialogText(button))+",\"actualId\":"+std::to_string(GetDlgCtrlID(button))+",\"posted\":"+plotBool(state->buttonPosted)+"}");symbolUiProgress(*s,L"after-post-native-editor-button");
    }catch(const std::exception& e){if(s->captured)s->error=familyError(e);}catch(...){s->error=quote(L"unknown object edit modal callback failure");}
    state->inside=false;
}
std::unique_ptr<SymbolUiOperation> symbolEditObject(const Paths& p,AcApDocument* doc,AcDbDatabase* db,const std::wstring& kind,AcDbObjectId target,const SymbolUiValues& desired,const std::wstring& id){
    auto s=std::make_unique<SymbolUiOperation>();s->paths=p;s->doc=doc;s->db=db;s->kind=kind;s->command=L"TObjEdit";s->mode=L"edit";s->editTargetHandle=handleOf(target);s->desired=desired;s->id=id;s->pid=GetCurrentProcessId();s->thread=GetCurrentThreadId();
    auto state=std::make_unique<SymbolObjectEditState>();state->operation=s.get();state->handle=handleOf(target);AcDbObjectIdArray protectedIds;std::string protectedBefore;std::pair<int,std::wstring> beforeCommand;
    try{fourAxisGuard(p,doc,db);if(activeSymbolObjectEdit)throw std::runtime_error("object edit timer already active");const auto all=modelIds(*db);for(int i=0;i<all.length();++i)if(all[i]!=target)protectedIds.append(all[i]);protectedBefore=familyCompactArray(protectedIds);beforeCommand=elevationDialogCommandState();elevationDialogKnownMainFrame=adsw_acadMainWnd();
        ads_name name{};check(acdbGetAdsName(name,target),"exact object edit entity name");activeSymbolObjectEdit=state.get();state->timer=SetTimer(nullptr,0,100,symbolObjectEditTimer);if(!state->timer)throw std::runtime_error("object edit timer unavailable");
        {AxisNativeTraceOwner trace(doc,s->trace);s->commandStatus=acedCommandS(RTSTR,L"TObjEdit",RTENAME,name,RTNONE);}
        state->timerStopped=KillTimer(nullptr,state->timer)!=FALSE;if(state->timerStopped)state->timer=0;
        fourAxisGuard(p,doc,db);const auto selected=axisContinuousObservedSelections(*s->trace);s->panelClosed=s->panel&&(!IsWindow(s->panel)||!IsWindowVisible(s->panel));s->observerRemoved=s->trace->editorRemoved&&s->trace->inputRemoveReturned&&!s->trace->retained;
        s->safe=state->timerStopped&&s->observerRemoved&&s->panelClosed&&!textBridgeHasUnbalancedCommandTrace(*s->trace)&&elevationDialogCommandState()==beforeCommand&&familyCompactArray(protectedIds)==protectedBefore;
        s->completed=s->safe&&s->configurationVerified&&s->error=="null"&&state->buttonPosted&&!state->cancelPosted&&selected.size()==1&&selected[0]==state->handle&&s->commandStatus==RTNORM;
        if(!s->completed&&s->error=="null")s->error=quote(L"native object edit lifecycle, button, selection or protected-source check failed");
    }catch(const std::exception& e){s->error=familyError(e);}catch(...){s->error=quote(L"unknown native object-edit failure");}
    if(state->timer){state->timerStopped=KillTimer(nullptr,state->timer)!=FALSE;if(state->timerStopped)state->timer=0;}
    if(!state->timerStopped){symbolUiBridgeRetained=true;state->operation=nullptr;state.release();acrxDynamicLinker->lockApplication(nativeApplicationId);}else activeSymbolObjectEdit=nullptr;
    return s;
}
