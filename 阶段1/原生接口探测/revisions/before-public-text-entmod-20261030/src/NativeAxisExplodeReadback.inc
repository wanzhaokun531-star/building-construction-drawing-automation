// Include after NativeObjectFamilyProbe.inc; no extra SDK libraries/headers.
// Public const explode() returns caller-owned, non-database-resident entities.
// It is NOT the EXPLODE command: no append/erase/save/private fields/COM.
// 2020 SDK: dbmain.h:2688; polysamp/transact.cpp:468-490 deletes returned parts.
struct AxisExplodeNode { AcDbEntity* entity{}; int id{},parent{},depth{}; };
struct AxisExplodePool {
    std::vector<std::unique_ptr<AcDbEntity>> owned;
    int returned{},deleted{},unresolved{},rejectedBorrowed{},duplicates{};
    bool safe{true};
    AxisExplodePool(){owned.reserve(4096);}
    AxisExplodePool(const AxisExplodePool&)=delete;
    AxisExplodePool& operator=(const AxisExplodePool&)=delete;
    bool contains(const void* value)const{for(const auto& e:owned)if(e.get()==value)return true;return false;}
    void release()noexcept{while(!owned.empty()){owned.pop_back();++deleted;}}
    ~AxisExplodePool(){release();}
};
bool axisExplodeTransient(const AcDbEntity& entity){return entity.objectId().isNull()&&entity.ownerId().isNull()&&entity.database()==nullptr;}
// Owns any unadopted return values even if explode() or serialization throws.
struct AxisExplodeReturns {
    AcDbVoidPtrArray values; AxisExplodePool& pool; const AcDbEntity* source;
    AxisExplodeReturns(AxisExplodePool& p,const AcDbEntity* s):pool(p),source(s){}
    AxisExplodeReturns(const AxisExplodeReturns&)=delete;
    AxisExplodeReturns& operator=(const AxisExplodeReturns&)=delete;
    ~AxisExplodeReturns()noexcept{
        for(int i=0;i<values.length();++i){void* raw=values[i];if(!raw||pool.contains(raw))continue;
            bool duplicate=false;for(int j=0;j<i;++j)if(values[j]==raw)duplicate=true;if(duplicate)continue;
            try{auto* entity=AcDbEntity::cast(static_cast<AcRxObject*>(raw));
                if(raw==source||!entity||!axisExplodeTransient(*entity)){pool.safe=false;++pool.unresolved;continue;}
                delete entity;++pool.returned;++pool.deleted;
            }catch(...){pool.safe=false;++pool.unresolved;}
        }
    }
};
std::string axisExplodeBounds(const AcDbEntity& entity){AcDbExtents e;const auto s=entity.getGeomExtents(e);return "{\"status\":"+std::to_string(s)+",\"min\":"+(s==Acad::eOk?point(e.minPoint()):"null")+",\"max\":"+(s==Acad::eOk?point(e.maxPoint()):"null")+"}";}
std::string axisExplodeVector(const AcGeVector3d& value){return "["+number(value.x)+","+number(value.y)+","+number(value.z)+"]";}
int axisExplodeDbmod(){resbuf value{};if(acedGetVar(L"DBMOD",&value)!=RTNORM)return -1;if(value.restype==RTSHORT)return value.resval.rint;if(value.restype==RTLONG)return value.resval.rlong;return -1;}

std::string axisExplodeReadAxis(AcDbObjectId id,bool& successful,bool& ownershipSafe){
    AxisExplodePool pool;std::vector<AxisExplodeNode> pending;pending.reserve(4096);
    std::vector<std::string> nodes,calls;std::string before="null",after="null",error="null";
    const auto handle=handleOf(id);int rootStatus=-9999;bool allLeaves=true,hasText=false,hasCircle=false;
    try{
        before=familyCompactEntity(id);auto root=open<AcDbEntity>(id);
        if(root->isAProxy()||_wcsicmp(root->isA()->name(),L"TDbAxisLabelSet"))throw std::runtime_error("explode root is not native TDbAxisLabelSet");
        pending.push_back({root.get(),0,-1,0});
        for(std::size_t cursor=0;cursor<pending.size();++cursor){
            const auto node=pending[cursor];const auto* entity=node.entity;std::string kind,detail;
            if(node.id!=0){
                if(auto* text=AcDbText::cast(entity)){AcString value;check(text->textString(value),"explode transient text string");kind="text";hasText=true;
                    detail=",\"text\":"+quote(value.kACharPtr())+",\"plainText\":"+quote(value.kACharPtr())+",\"position\":"+point(text->position())+",\"alignmentPoint\":"+point(text->alignmentPoint())+",\"height\":"+number(text->height())+",\"rotation\":"+number(text->rotation())+",\"bounds\":"+axisExplodeBounds(*text);
                }else if(auto* text=AcDbMText::cast(entity)){AcString value,plain;check(text->contents(value),"explode transient mtext contents");check(text->text(plain),"explode transient mtext plain text");kind="mtext";hasText=true;
                    detail=",\"text\":"+quote(plain.kACharPtr())+",\"plainText\":"+quote(plain.kACharPtr())+",\"rawContents\":"+quote(value.kACharPtr())+",\"position\":"+point(text->location())+",\"height\":"+number(text->textHeight())+",\"rotation\":"+number(text->rotation())+",\"bounds\":"+axisExplodeBounds(*text);
                }else if(auto* circle=AcDbCircle::cast(entity)){kind="circle";hasCircle=true;detail=",\"center\":"+point(circle->center())+",\"radius\":"+number(circle->radius())+",\"normal\":"+axisExplodeVector(circle->normal());
                }else if(auto* line=AcDbLine::cast(entity)){kind="line";detail=",\"start\":"+point(line->startPoint())+",\"end\":"+point(line->endPoint());}
            }
            if(kind.empty()){
                if(node.depth>=8)throw std::runtime_error("explode recursion depth limit reached; no inference made");
                AxisExplodeReturns parts(pool,entity);const auto status=entity->explode(parts.values);if(node.id==0)rootStatus=status;
                calls.push_back("{\"nodeId\":"+std::to_string(node.id)+",\"depth\":"+std::to_string(node.depth)+",\"status\":"+std::to_string(status)+",\"returnedCount\":"+std::to_string(parts.values.length())+"}");
                if(parts.values.length()>4096||pending.size()+static_cast<std::size_t>(parts.values.length())>4096)throw std::runtime_error("explode entity budget exceeded; validated transient parts released");
                for(int j=0;j<parts.values.length();++j){void* raw=parts.values[j];
                    if(!raw){pool.safe=false;++pool.unresolved;continue;}
                    if(raw==entity||pool.contains(raw)){pool.safe=false;++pool.duplicates;parts.values[j]=nullptr;continue;}
                    auto* child=AcDbEntity::cast(static_cast<AcRxObject*>(raw));
                    if(!child){pool.safe=false;++pool.unresolved;parts.values[j]=nullptr;continue;}
                    if(!axisExplodeTransient(*child)){pool.safe=false;++pool.rejectedBorrowed;parts.values[j]=nullptr;continue;}
                    pool.owned.emplace_back(child);++pool.returned;parts.values[j]=nullptr;
                    pending.push_back({child,static_cast<int>(pending.size()),node.id,node.depth+1});
                }
                if(!pool.safe)throw std::runtime_error("explode returned null/aliased/non-transient/uncertain ownership; traversal stopped safely");
                if(status!=Acad::eOk||parts.values.length()==0){kind="unsupported";allLeaves=false;if(node.id==0)throw std::runtime_error("native axis public explode not supported or empty");}
                else kind="compound";
            }
            if(node.id!=0)nodes.push_back("{\"nodeId\":"+std::to_string(node.id)+",\"parentNodeId\":"+std::to_string(node.parent)+",\"depth\":"+std::to_string(node.depth)+",\"type\":"+quote(entity->isA()->name())+",\"kind\":"+quote(std::wstring(kind.begin(),kind.end()))+",\"transient\":true,\"objectIdNull\":true,\"ownerIdNull\":true,\"databaseNull\":true"+detail+"}");
        }
        root.reset();after=familyCompactEntity(id);
    }catch(const std::exception& failure){error=familyError(failure);try{after=familyCompactEntity(id);}catch(...){}}
     catch(...){error=quote(L"unknown public explode readback exception");try{after=familyCompactEntity(id);}catch(...){}}
    pool.release();ownershipSafe=pool.safe&&pool.unresolved==0&&pool.rejectedBorrowed==0&&pool.duplicates==0&&pool.returned==pool.deleted;
    successful=error=="null"&&rootStatus==Acad::eOk&&ownershipSafe&&allLeaves&&hasText&&hasCircle&&before==after;
    return "{\"axisHandle\":"+quote(handle)+",\"before\":"+before+",\"after\":"+after+",\"rootExplodeStatus\":"+std::to_string(rootStatus)+",\"explodedNodeCount\":"+std::to_string(nodes.size())+",\"returnedTransientCount\":"+std::to_string(pool.returned)+",\"deletedTransientCount\":"+std::to_string(pool.deleted)+",\"ownershipSafe\":"+(ownershipSafe?"true":"false")+",\"unresolvedOwnershipCount\":"+std::to_string(pool.unresolved)+",\"rejectedBorrowedCount\":"+std::to_string(pool.rejectedBorrowed)+",\"duplicatePointerCount\":"+std::to_string(pool.duplicates)+",\"allLeavesRead\":"+(allLeaves?"true":"false")+",\"hasText\":"+(hasText?"true":"false")+",\"hasCircle\":"+(hasCircle?"true":"false")+",\"explodeCalls\":"+axisTraceArray(calls)+",\"nodes\":"+axisTraceArray(nodes)+",\"completed\":"+(successful?"true":"false")+",\"error\":"+error+"}";
}

void nativeAxisExplodeReadback(){
    try{
        quietRequireIsolatedWorker();const auto p=paths();const auto control=fullPath(p.output+L"\\control.dwg"),report=fullPath(p.output+L"\\axis-explode-readback-native.json");requireNewFile(report);
        std::string error="null",before="[]",after="[]";std::vector<std::string> axes;int initial=-1,final=-1,nonAxis=-1,dbmodBefore=-1,dbmodAfter=-1,txBefore=-1,txAfter=-1;bool complete=true,ownershipSafe=true,membership=false;
        try{
            auto* doc=acDocManager?acDocManager->curDocument():nullptr;auto* db=doc?doc->database():nullptr;
            if(!db||acDocManager->isApplicationContext()||acDocManager->mdiActiveDocument()!=doc||acdbHostApplicationServices()->workingDatabase()!=db||!doc->fileName()||_wcsicmp(fullPath(doc->fileName()).c_str(),control.c_str()))throw std::runtime_error("explode requires owned current/MDI/working control document");
            const ACHAR* file{};check(db->getFilename(file),"explode control database filename");if(!file||_wcsicmp(fullPath(file).c_str(),control.c_str()))throw std::runtime_error("explode database filename differs");
            txBefore=db->transactionManager()->numActiveTransactions();if(txBefore!=0)throw std::runtime_error("explode requires zero active transactions");
            const auto all=modelIds(*db);initial=all.length();if(initial!=215&&initial!=235)throw std::runtime_error("explode readback accepts only Seed215 or Singleton235 task copies");
            AcDbObjectIdArray targets;for(int i=0;i<all.length();++i){auto e=open<AcDbEntity>(all[i]);if(!_wcsicmp(e->isA()->name(),L"TDbAxisLabelSet")){if(e->isAProxy())throw std::runtime_error("explode refuses proxy axis root");targets.append(all[i]);}}
            nonAxis=initial-targets.length();if(nonAxis!=213||targets.length()!=initial-213)throw std::runtime_error("explode task copy does not retain213 protected originals plus native axes");
            before=familyCompactArray(all);dbmodBefore=axisExplodeDbmod();if(dbmodBefore<0)throw std::runtime_error("explode DBMOD observation unavailable");
            for(int i=0;i<targets.length();++i){bool ok=false,safe=false;axes.push_back(axisExplodeReadAxis(targets[i],ok,safe));complete=complete&&ok;ownershipSafe=ownershipSafe&&safe;if(!safe){complete=false;break;}}
            const auto now=modelIds(*db);final=now.length();membership=all.length()==now.length();if(membership)for(int i=0;i<all.length();++i)if(all[i]!=now[i])membership=false;
            after=familyCompactArray(now);dbmodAfter=axisExplodeDbmod();txAfter=db->transactionManager()->numActiveTransactions();
            complete=complete&&axes.size()==static_cast<std::size_t>(targets.length())&&ownershipSafe&&membership&&before==after&&dbmodBefore==dbmodAfter&&txAfter==0;
            if(!complete)error=quote(L"public explode readback unsupported, incomplete, ownership-unsafe or model changed; inspect retained native evidence");
        }catch(const std::exception& failure){error=familyError(failure);complete=false;}
        const auto result="{\"contract\":\"stage1.axis-explode-readback-native.v1\",\"api\":\"ObjectARX native public AcDbEntity::explode readback; no COM\",\"controlDWG\":"+quote(control)+",\"initialModelCount\":"+std::to_string(initial)+",\"finalModelCount\":"+std::to_string(final)+",\"axisCount\":"+std::to_string(axes.size())+",\"protectedNonAxisCount\":"+std::to_string(nonAxis)+",\"allModelBefore\":"+before+",\"allModelAfter\":"+after+",\"modelMembershipUnchanged\":"+(membership?"true":"false")+",\"dbmodBefore\":"+std::to_string(dbmodBefore)+",\"dbmodAfter\":"+std::to_string(dbmodAfter)+",\"activeTransactionsBefore\":"+std::to_string(txBefore)+",\"activeTransactionsAfter\":"+std::to_string(txAfter)+",\"axes\":"+axisTraceArray(axes)+",\"ownershipSafe\":"+(ownershipSafe?"true":"false")+",\"dwgSaved\":false,\"persistentAppendAttempted\":false,\"persistentEraseAttempted\":false,\"privateFieldsAccessed\":false,\"commandBridgeUsed\":false,\"comUsed\":false,\"completed\":"+(complete?"true":"false")+",\"semanticScope\":\"actual public transient text/circle/line content only; expected numbering requires independent comparison\",\"error\":"+error+"}";
        writeNewReport(report,result);
    }catch(const std::exception& failure){acutPrintf(L"\nAXIS_EXPLODE_READBACK_FAILED %hs\n",failure.what());}catch(...){acutPrintf(L"\nAXIS_EXPLODE_READBACK_FAILED unknown exception\n");}
}
