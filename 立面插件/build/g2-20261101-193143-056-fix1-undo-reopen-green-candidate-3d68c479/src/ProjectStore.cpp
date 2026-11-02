#include "ProjectStore.h"
#include <acdocman.h>
#include <actrans.h>
#include <aced.h>
#include <acutads.h>
#include <acutmem.h>
#include <adscodes.h>
#include <dbdict.h>
#include <dbxrecrd.h>
#include <memory>
#include <set>
#include <stdexcept>
namespace bce {
namespace {
void check(Acad::ErrorStatus status,const char* operation){if(status!=Acad::eOk)throw std::runtime_error(std::string(operation)+": "+std::to_string(status));}
void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class T> Result<T> fail(const char* code,const std::string& message){return {std::nullopt,{{code,message,{},true}}};}
template<class T> class Open final {
public:
    explicit Open(AcDbObjectId id){check(acdbOpenObject(object_,id,AcDb::kForRead),"open project object");}
    ~Open()noexcept{if(object_)object_->close();}
    Open(const Open&)=delete;Open& operator=(const Open&)=delete;
    T* operator->()const noexcept{return object_;}T* get()const noexcept{return object_;}
private:T* object_{};
};
struct RbDelete{void operator()(resbuf* rb)const noexcept{if(rb)acutRelRb(rb);}};
using Rb=std::unique_ptr<resbuf,RbDelete>;
constexpr wchar_t envelope[]=L"BCE.PROJECT.HEX.1";
Rb chain(const std::string& bytes){
    Rb first;resbuf* tail{};
    const auto append=[&](const std::wstring& text){Rb node(acutNewRb(1));need(node!=nullptr,"acutNewRb failed");node->rbnext=nullptr;node->resval.rstring=nullptr;check(acutNewString(text.c_str(),node->resval.rstring),"SDK allocate Xrecord text");auto* raw=node.get();if(tail)tail->rbnext=node.release();else first=std::move(node);tail=raw;};
    append(envelope);constexpr wchar_t digits[]=L"0123456789ABCDEF";
    for(std::size_t i=0;i<bytes.size();i+=120){std::wstring chunk;const auto end=(std::min)(bytes.size(),i+120);chunk.reserve((end-i)*2);for(auto j=i;j<end;++j){const auto b=static_cast<unsigned char>(bytes[j]);chunk.push_back(digits[b>>4]);chunk.push_back(digits[b&15]);}append(chunk);}
    return first;
}
std::string fromChain(AcDbXrecord& record){
    resbuf* raw{};check(record.rbChain(&raw),"Xrecord rbChain");Rb rb(raw);
    need(raw&&raw->restype==1&&raw->resval.rstring,"project Xrecord header missing");
    need(std::wstring(raw->resval.rstring)==envelope,"foreign Xrecord envelope");
    std::string bytes;std::size_t chunks{};
    const auto nibble=[](wchar_t c)->unsigned{if(c>=L'0'&&c<=L'9')return static_cast<unsigned>(c-L'0');if(c>=L'A'&&c<=L'F')return static_cast<unsigned>(c-L'A'+10);throw std::runtime_error("invalid Xrecord hex character");};
    for(auto* node=raw->rbnext;node;node=node->rbnext){need(++chunks<=projectStoreMaxBytes/120+1,"Xrecord chunk count limit");need(node->restype==1&&node->resval.rstring,"unexpected Xrecord group");const auto* text=node->resval.rstring;const auto n=wcsnlen_s(text,241);need(n>0&&n<=240&&n%2==0,"invalid Xrecord chunk length");need(bytes.size()+n/2<=projectStoreMaxBytes,"Xrecord size limit");for(std::size_t i=0;i<n;i+=2)bytes.push_back(static_cast<char>((nibble(text[i])<<4)|nibble(text[i+1])));}
    need(!bytes.empty(),"empty project Xrecord");return bytes;
}
class Transaction final {
public:
    explicit Transaction(AcDbTransactionManager& manager):manager_(manager),transaction_(manager.startTransaction()){need(transaction_!=nullptr,"start project transaction");}
    ~Transaction()noexcept{if(active_&&manager_.numActiveTransactions()==1&&manager_.topTransaction()==transaction_)manager_.abortTransaction();}
    Transaction(const Transaction&)=delete;Transaction& operator=(const Transaction&)=delete;
    template<class T>T* open(AcDbObjectId id,AcDb::OpenMode mode){AcDbObject* object{};check(transaction_->getObject(object,id,mode),"transaction open project object");auto* typed=T::cast(object);need(typed!=nullptr,"foreign dictionary/object class");return typed;}
    void adopt(AcDbObject* object){const auto status=manager_.addNewlyCreatedDBRObject(object);if(status!=Acad::eOk){object->erase();object->close();check(status,"adopt new project object");}}
    void commit(){owned();check(manager_.endTransaction(),"commit project transaction");active_=false;}
    void abort(){owned();check(manager_.abortTransaction(),"abort project transaction");active_=false;}
private:
    void owned(){need(active_&&manager_.numActiveTransactions()==1&&manager_.topTransaction()==transaction_,"project transaction ownership changed");}
    AcDbTransactionManager& manager_;AcTransaction* transaction_{};bool active_{true};
};
struct Cancelled{};
void notify(StoreObserver* observer,StoreStage stage){if(observer&&!observer->proceed(stage))throw Cancelled{};}
std::string canonical(std::string h){const auto n=h.find_first_not_of('0');need(n!=std::string::npos,"invalid history handle");h.erase(0,n);for(auto& c:h)if(c>='a'&&c<='f')c=static_cast<char>(c-'a'+'A');return h;}
bool sameFingerprints(const std::vector<EntityFingerprint>& a,const std::vector<EntityFingerprint>& b){
    if(a.size()!=b.size())return false;
    for(const auto& f:a){const auto h=canonical(f.key.handle);const auto found=std::find_if(b.begin(),b.end(),[&](const EntityFingerprint& g){return canonical(g.key.handle)==h;});if(found==b.end()||found->geometry!=f.geometry||found->text!=f.text||found->resources!=f.resources||found->nativeClass!=f.nativeClass)return false;}return true;
}
// Standalone configuration edits may mark history stale, but may never delete,
// replace or invent generation ownership. Registration belongs to the future
// combined build transaction and its independently verified G1 receipt.
bool preservesHistory(const GenerationState& a,const GenerationState& b){
    if(a.views.size()!=b.views.size())return false;
    for(const auto& old:a.views){const auto found=std::find_if(b.views.begin(),b.views.end(),[&](const GeneratedViewState& g){return g.viewIdentity==old.viewIdentity;});if(found==b.views.end())return false;const auto& now=*found;
        if(old.moduleIdentity!=now.moduleIdentity||old.moduleVersion!=now.moduleVersion||old.generationId!=now.generationId||old.sourceSignature!=now.sourceSignature||old.configSignature!=now.configSignature||old.committedRevision!=now.committedRevision||old.commitEvidence!=now.commitEvidence||old.postCommitEvidence!=now.postCommitEvidence||(old.requiresReconfirmation&&!now.requiresReconfirmation)||!sameFingerprints(old.sourceEntities,now.sourceEntities)||!sameFingerprints(old.outputs,now.outputs))return false;
    }return true;
}
}
Result<std::string> readProjectStoreBytes(AcDbDatabase& db){
    try{Open<AcDbDictionary> nod(db.namedObjectsDictionaryId());if(!nod->has(projectStoreDictionary))return fail<std::string>("PROJECT_NOT_FOUND","No BCE project archive");AcDbObjectId dictionaryId;check(nod->getAt(projectStoreDictionary,dictionaryId),"project dictionary ID");Open<AcDbDictionary> dictionary(dictionaryId);need(dictionary->numEntries()==1&&dictionary->has(projectStoreRecord),"foreign/incomplete project dictionary");AcDbObjectId recordId;check(dictionary->getAt(projectStoreRecord,recordId),"project archive ID");Open<AcDbXrecord> record(recordId);return {fromChain(*record.get()),{}};}
    catch(const std::exception& e){return fail<std::string>("PROJECT_STORE_CONFLICT",e.what());}
}
Result<ProjectArchive> readProjectStore(AcDbDatabase& db){const auto raw=readProjectStoreBytes(db);if(!raw.canCommit())return {std::nullopt,raw.issues};return decodeProject(*raw.value);}
Result<StoreReceipt> writeProjectStore(NativeSceneContext& context,const StoreRequest& request,StoreObserver* observer){
    try{
        auto* db=context.currentDatabase();auto* doc=acDocManager->curDocument();
        if(!db||!doc||doc->database()!=db||request.expectedDocumentIdentity!=context.documentIdentity()||request.expectedRevision!=context.revision())return fail<StoreReceipt>(diagnostic::staleDocument,"Project storage context/revision changed");
        if(db->transactionManager()->numActiveTransactions()!=0||doc->transactionManager()->numActiveTransactions()!=0)return fail<StoreReceipt>("OUTER_TRANSACTION_ACTIVE","Standalone store refuses caller transaction");
        if(acDocManager->isApplicationContext()||doc->myLockMode()==AcAp::kNone||doc->myLockMode()==AcAp::kNotLocked||doc->myLockMode()==AcAp::kRead)return fail<StoreReceipt>("COMMAND_CONTEXT_REQUIRED","Current modal write command required");
        resbuf undo{};if(acedGetVar(L"UNDOCTL",&undo)!=RTNORM||undo.restype!=RTSHORT||!(undo.resval.rint&1)||(undo.resval.rint&8))return fail<StoreReceipt>("UNDO_BOUNDARY_UNAVAILABLE","Undo enabled with no caller Undo group required");
        if(!request.archive.bound||request.archive.project.config.databaseIdentity!=context.documentIdentity()+"/database"||(!request.archive.project.generation.views.empty()&&(request.archive.project.generation.documentIdentity!=context.documentIdentity()||request.archive.project.generation.databaseIdentity!=request.archive.project.config.databaseIdentity)))return fail<StoreReceipt>("PROJECT_UNBOUND","Archive must be explicitly bound to this current document");
        const auto encoded=encodeProject(request.archive.project);if(!encoded.canCommit())return {std::nullopt,encoded.issues};
        const auto old=readProjectStore(*db);bool exists=false;
        if(old.readable()){exists=true;if(old.value->project.persistentProjectId!=request.archive.project.persistentProjectId)return fail<StoreReceipt>("PROJECT_ID_CONFLICT","Existing persistent project identity differs");if(!preservesHistory(old.value->project.generation,request.archive.project.generation))return fail<StoreReceipt>("GENERATION_HISTORY_PROTECTED","Standalone configuration cannot replace or remove historical ownership");}
        else if(old.issues.size()!=1||old.issues[0].code!="PROJECT_NOT_FOUND")return {std::nullopt,old.issues};
        auto data=chain(*encoded.value);
        if(context.currentDatabase()!=db||context.revision()!=request.expectedRevision)return fail<StoreReceipt>(diagnostic::staleDocument,"Project changed during preflight");
        Transaction transaction(*db->transactionManager());
        try{
            notify(observer,StoreStage::beforeDictionary);
            auto* nod=transaction.open<AcDbDictionary>(db->namedObjectsDictionaryId(),AcDb::kForWrite);AcDbObjectId dictionaryId;AcDbDictionary* dictionary{};
            if(exists){check(nod->getAt(projectStoreDictionary,dictionaryId),"existing dictionary");dictionary=transaction.open<AcDbDictionary>(dictionaryId,AcDb::kForWrite);}
            else{need(!nod->has(projectStoreDictionary),"dictionary appeared after preflight");auto fresh=std::make_unique<AcDbDictionary>();fresh->setTreatElementsAsHard(true);check(nod->setAt(projectStoreDictionary,fresh.get(),dictionaryId),"create exclusive BCE dictionary");dictionary=fresh.release();transaction.adopt(dictionary);}
            notify(observer,StoreStage::dictionaryReady);notify(observer,StoreStage::beforeRecord);
            if(exists){AcDbObjectId recordId;check(dictionary->getAt(projectStoreRecord,recordId),"existing Xrecord");auto* record=transaction.open<AcDbXrecord>(recordId,AcDb::kForWrite);check(record->setFromRbChain(*data),"replace BCE archive chain");need(fromChain(*record)==*encoded.value,"replacement rbChain readback mismatch");}
            else{auto record=std::make_unique<AcDbXrecord>();record->setXlateReferences(false);check(record->setFromRbChain(*data),"new BCE archive chain");AcDbObjectId recordId;check(dictionary->setAt(projectStoreRecord,record.get(),recordId),"insert BCE archive");auto* owned=record.release();transaction.adopt(owned);need(fromChain(*owned)==*encoded.value,"new rbChain readback mismatch");}
            notify(observer,StoreStage::recordWritten);
            need(context.currentDatabase()==db,"current document changed during project commit");transaction.commit();
            return {StoreReceipt{true,request.archive.project.persistentProjectId},encoded.issues};
        }catch(const Cancelled&){transaction.abort();return fail<StoreReceipt>("PROJECT_STORE_CANCELLED","Cancelled; own project transaction aborted");}
        catch(const std::exception& e){transaction.abort();return fail<StoreReceipt>("PROJECT_STORE_FAILED",e.what());}
        catch(...){transaction.abort();return fail<StoreReceipt>("PROJECT_STORE_FAILED","Unknown failure; own transaction aborted");}
    }catch(const std::exception& e){return fail<StoreReceipt>("PROJECT_STORE_FAILED",e.what());}
    catch(...){return fail<StoreReceipt>("PROJECT_STORE_FAILED","Unknown project storage failure");}
}
}
