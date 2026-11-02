// Read-only public ObjectARX RX metadata. No private-field or OPM/COM fallback.
#include "rxmember.h"
#include "rxprop.h"
#include "rxvalue.h"
#include <set>
std::string nativeRxMembers(AcDbEntity& entity){
    auto* engine=AcRxMemberQueryEngine::theEngine();if(!engine)throw std::runtime_error("native RX query engine unavailable");
    std::unique_ptr<AcRxMemberIterator> iterator(engine->newMemberIterator(&entity));
    std::vector<std::string> members;bool limited=false;
    if(iterator)for(;!iterator->done();iterator->next()){
        if(members.size()>=512){limited=true;break;}auto* member=iterator->current();if(!member)throw std::runtime_error("RX returned a null member");
        const auto* property=AcRxProperty::cast(member);const auto* owner=AcRxClass::cast(member->owner());
        std::string value="null";int status=-9999,formatted=-9999;bool readOnly=true;
        if(property){readOnly=property->isReadOnly(&entity);AcRxValue result;status=property->getValue(&entity,result);if(status==Acad::eOk){wchar_t text[8192]{};formatted=result.toString(text,8192);text[8191]=0;value=quote(text);}}
        members.push_back("{\"name\":"+quote(member->name())+",\"memberRxClass\":"+quote(member->isA()->name())+",\"owner\":"+(owner?quote(owner->name()):std::string("null"))+",\"valueType\":"+quote(member->type().name())+",\"property\":"+plotBool(property!=nullptr)+",\"readOnly\":"+plotBool(readOnly)+",\"getStatus\":"+std::to_string(status)+",\"formatResult\":"+std::to_string(formatted)+",\"value\":"+value+"}");
    }
    return "{\"rxName\":"+quote(entity.isA()->name())+",\"iteratorReturned\":"+plotBool(iterator!=nullptr)+",\"memberLimitHit\":"+plotBool(limited)+",\"memberCount\":"+std::to_string(members.size())+",\"members\":"+axisTraceArray(members)+"}";
}
void nativeRxDiscovery(){
    const auto p=paths();std::string error="null",control="null";std::vector<std::string> sources;bool complete=false;
    try{quietRequireIsolatedWorker();requireNewFile(p.output+L"\\native-rx-discovery.json");
        for(const auto& path:{p.scheme,p.reference}){auto db=lifecycleRead(path);WorkingDatabaseScope work(*db);const auto ids=modelIds(*db);const auto before=familyCompactArray(ids);std::set<std::wstring> observed;std::vector<std::string> samples;
            for(int i=0;i<ids.length();++i){auto entity=open<AcDbEntity>(ids[i]);const std::wstring name=entity->isA()->name();if(name.rfind(L"TDb",0)!=0||!observed.insert(name).second)continue;samples.push_back("{\"handle\":"+quote(handleOf(ids[i]))+",\"metadata\":"+nativeRxMembers(*entity)+"}");}
            const auto after=familyCompactArray(ids);if(before!=after)throw std::runtime_error("RX discovery changed original public entity states");
            sources.push_back("{\"path\":"+quote(path)+",\"modelCount\":"+std::to_string(ids.length())+",\"originalBefore\":"+before+",\"originalAfter\":"+after+",\"samples\":"+axisTraceArray(samples)+"}");
        }
        AcDbLine line(AcGePoint3d::kOrigin,AcGePoint3d(1000,0,0));control=nativeRxMembers(line);complete=true;
    }catch(const std::exception& e){error=familyError(e);}catch(...){error=quote(L"unknown native RX discovery failure");}
    writeNewReport(p.output+L"\\native-rx-discovery.json","{\"contract\":\"stage1.native-rx-discovery.v1\",\"api\":\"AcRxMemberQueryEngine/AcRxProperty::getValue\",\"sources\":"+axisTraceArray(sources)+",\"nativeControl\":"+control+",\"setValueCalls\":0,\"COMUsed\":false,\"ComputerUseUsed\":false,\"TianzhengCommandsUsed\":false,\"sourceDrawingWritten\":false,\"phase2Started\":false,\"completed\":"+plotBool(complete)+",\"error\":"+error+"}");
}
