// Include after NativePlotProbe.inc and NativeFourSideAxisProbe.inc. Reuses the
// established AcPl pipeline and temporary plot settings, without changing DWG.
// This entry accepts only the complete four-side 215 fixture or the explicitly
// separate 214 single-template numbering fixture. No COM, activation or view edit.
void nativeFourAxisPreview(){
    try{
        quietRequireIsolatedWorker();const auto p=paths();const auto report=fullPath(p.output+L"\\axis-preview-native.json");requireNewFile(report);
        NativePlotContext context(p);NativePlotBackgroundScope background;
        std::string error="null",sourceKind="null",before="[]",after="[]",axes="[]",globalsBefore="null",globalsAfter="null",layoutBefore="null",layoutAfter="null",page="null";
        std::vector<std::string> axisBodies;int initial=-1,final=-1;double window[4]{},paper[2]{594,420};AcGePoint3d modelMin,modelMax;bool boundsKnown=false,fixtureVerified=false;
        try{
            auto* doc=acDocManager?acDocManager->curDocument():nullptr;auto* db=doc?doc->database():nullptr;fourAxisGuard(p,doc,db);
            const auto all=modelIds(*db);initial=all.length();if(initial!=214&&initial!=215)throw std::runtime_error("axis preview requires explicit complete215 or number-template214 fixture");
            sourceKind=quote(initial==215?L"full-four-side-215":L"single-template-number-214");before=familyCompactArray(all);AcDbObjectIdArray originals,axisIds;
            for(int i=0;i<all.length();++i){auto e=open<AcDbEntity>(all[i]);if(!_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet")){if(e->isAProxy())throw std::runtime_error("axis preview refuses proxy axis set");axisIds.append(all[i]);}else originals.append(all[i]);}
            if(originals.length()!=213||axisIds.length()!=initial-213)throw std::runtime_error("axis preview original/axis cohort differs");
            const auto originalSnapshot=familyCompactArray(originals);
            {AcDbDatabase frozen(false,true);check(frozen.readDwgFile(p.scheme.c_str(),AcDbDatabase::kForReadAndAllShare,false),"axis preview read frozen original oracle");check(frozen.closeInput(true),"axis preview close frozen oracle");WorkingDatabaseScope scope(frozen);if(modelCount(frozen)!=213||familyCompactArray(modelIds(frozen))!=originalSnapshot)throw std::runtime_error("axis preview original213 differs from frozen scheme");}
            FourSideMetrics metrics;if(initial==215)metrics=fourSideReadMetrics(p,*db,originals);
            bool seenV=false,seenH=false;
            for(int i=0;i<axisIds.length();++i){AcGePoint3dArray sp;{auto e=open<AcDbEntity>(axisIds[i]);check(e->getStretchPoints(sp),"axis preview native stretch orientation");}
                const auto ux=fourAxisUnique(sp,true),uy=fourAxisUnique(sp,false);const bool vertical=initial==215?(ux.size()==10&&uy.size()==4):(ux.size()==9&&uy.size()==4);const bool horizontal=initial==215?(ux.size()==4&&uy.size()==12):(ux.size()==4&&uy.size()==8);
                if(vertical==horizontal||(vertical&&seenV)||(horizontal&&seenH))throw std::runtime_error("axis preview actual station orientation/count differs from named fixture");seenV=seenV||vertical;seenH=seenH||horizontal;
                const auto observed=fourAxisObserve(axisIds[i],vertical);if(!observed.available()||!fourAxisBoundsValid(observed)||!fourAxisAuxiliarySign(observed))throw std::runtime_error("axis preview actual native controls/radius/bounds unavailable");
                if(initial==215&&!fourSideMatches(observed,metrics,vertical,vertical?metrics.xs:metrics.ys))throw std::runtime_error("axis preview complete four-side points do not match all true scheme candidates");
                axisBodies.push_back("{\"orientation\":"+quote(vertical?L"vertical":L"horizontal")+",\"stationCount\":"+std::to_string(observed.stations.size())+",\"nativeGeometry\":"+observed.entity+"}");
            }
            if(initial==215&&(!seenV||!seenH))throw std::runtime_error("axis preview must contain both complete native directions");axes=axisTraceArray(axisBodies);
            for(int i=0;i<all.length();++i){AcDbExtents ext;{auto e=open<AcDbEntity>(all[i]);check(e->getGeomExtents(ext),"axis preview full model extents");}const auto lo=ext.minPoint(),hi=ext.maxPoint();if(!std::isfinite(lo.x)||!std::isfinite(lo.y)||!std::isfinite(hi.x)||!std::isfinite(hi.y)||lo.x>hi.x||lo.y>hi.y)throw std::runtime_error("axis preview nonfinite/invalid model extents");if(!boundsKnown){modelMin=lo;modelMax=hi;boundsKnown=true;}else{modelMin.x=(std::min)(modelMin.x,lo.x);modelMin.y=(std::min)(modelMin.y,lo.y);modelMax.x=(std::max)(modelMax.x,hi.x);modelMax.y=(std::max)(modelMax.y,hi.y);}}
            if(!boundsKnown)throw std::runtime_error("axis preview has no model bounds");window[0]=modelMin.x-500;window[1]=modelMin.y-500;window[2]=modelMax.x+500;window[3]=modelMax.y+500;
            // Forty mm reserved overall for nominal margins; the independent
            // verifier additionally checks the actual validated printable area.
            if((window[2]-window[0])/100>paper[0]-40||(window[3]-window[1])/100>paper[1]-40){paper[0]=841;paper[1]=594;}
            if((window[2]-window[0])/100>paper[0]-40||(window[3]-window[1])/100>paper[1]-40){paper[0]=1189;paper[1]=841;}
            if((window[2]-window[0])/100>paper[0]-40||(window[3]-window[1])/100>paper[1]-40)throw std::runtime_error("axis preview full window exceeds A0 at exact1:100; Fit refused");
            context.prepare(window);fourAxisGuard(p,doc,db);globalsBefore=plotGlobalsJson();{auto layout=open<AcDbLayout>(context.layout);layoutBefore=plotSettingsJson(*layout);}fixtureVerified=true;
            quietRequireIsolatedWorker();context.guard();background.activate();page=nativePlotPage(context,L"axis-preview",paper[0],paper[1]);
            fourAxisGuard(p,doc,db);context.guard();const auto finalIds=modelIds(*db);final=finalIds.length();after=familyCompactArray(finalIds);if(before!=after||initial!=final)throw std::runtime_error("axis preview altered model objects");{auto layout=open<AcDbLayout>(context.layout);layoutAfter=plotSettingsJson(*layout);}if(layoutBefore!=layoutAfter)throw std::runtime_error("axis preview changed saved layout settings");
        }catch(const std::exception& e){error=plotError(e);}catch(...){error=quote(L"unknown native four-axis preview exception");}
        background.restore();if(background.restorationFailed())error=quote(L"axis preview BACKGROUNDPLOT restoration failed");globalsAfter=plotGlobalsJson();
        std::ostringstream out;out<<"{\"contract\":\"stage1.four-axis-preview-native.v1\",\"api\":\"ObjectARX native plot; no COM\",\"sourceKind\":"<<sourceKind<<",\"inputDWG\":"<<quote(context.control)<<",\"controlDWG\":"<<quote(context.control)<<",\"outputPDF\":"<<quote(fullPath(p.output+L"\\axis-preview.pdf"))<<",\"inputBinding\":\"independent verifier must bind explicit successful source native report and SHA256-equal source output DWG; no latest-run discovery\",\"initialModelCount\":"<<initial<<",\"finalModelCount\":"<<final<<",\"fixtureVerified\":"<<plotBool(fixtureVerified)<<",\"axisObjects\":"<<axes<<",\"modelBoundsMin\":["<<number(modelMin.x)<<','<<number(modelMin.y)<<"],\"modelBoundsMax\":["<<number(modelMax.x)<<','<<number(modelMax.y)<<"],\"marginModel\":500,\"windowWCS\":["<<number(window[0])<<','<<number(window[1])<<','<<number(window[2])<<','<<number(window[3])<<"],\"windowDCS\":"<<context.windowDCS<<",\"windowTransformStatuses\":"<<context.windowTransforms<<",\"viewCheck\":"<<context.viewEvidence<<",\"requestedPaperMM\":["<<number(paper[0])<<','<<number(paper[1])<<"],\"scale\":[1,100],\"originalBefore\":"<<before<<",\"originalAfter\":"<<after<<",\"globalsBefore\":"<<globalsBefore<<",\"globalsAfter\":"<<globalsAfter<<",\"backgroundOverride\":"<<background.json()<<",\"layoutBefore\":"<<layoutBefore<<",\"layoutAfter\":"<<layoutAfter<<",\"page\":"<<page<<",\"dwgSaved\":false,\"numberingSemanticVerified\":false,\"visualReview\":\"pending independent rendering and inspection\",\"error\":"<<error<<'}';writeNewReport(report,out.str());
    }catch(const std::exception& e){acutPrintf(L"\nSTAGE1_FOUR_AXIS_PREVIEW_FAILED: %hs\n",e.what());}
}
