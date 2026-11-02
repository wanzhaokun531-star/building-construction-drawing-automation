// Read-only display/plot diagnostic for a separately saved stage2 intermediate.
#include "UniqueRawParts.h"
struct Stage2PreviewParts {
 AcDbVoidPtrArray values;const AcDbEntity* root;
 explicit Stage2PreviewParts(const AcDbEntity* p):root(p){}
 ~Stage2PreviewParts(){nativeparts::forEachDistinct(values,[&](void* raw){auto* e=AcDbEntity::cast(static_cast<AcRxObject*>(raw));if(e&&e!=root&&e->database()==nullptr&&e->objectId().isNull()&&e->ownerId().isNull())delete e;});}
};
void nativeStage2Preview()noexcept{
 try{
  quietRequireIsolatedWorker();const auto p=paths();const auto report=p.output+L"\\stage2-preview-native.json";requireNewFile(report);
  NativePlotContext context(p);NativePlotBackgroundScope background;
  std::string error="null",before="[]",after="[]",page="null",dimensions="[]",globalsBefore="null",globalsAfter="null",layoutBefore="null",layoutAfter="null";int initial=-1,final=-1;
  try{
   const double window[4]{9400,12000,37500,39500};context.prepare(window);const auto ids=modelIds(*context.database);initial=ids.length();
   before=plotGeometryJson(ids);globalsBefore=plotGlobalsJson();{auto l=open<AcDbLayout>(context.layout);layoutBefore=plotSettingsJson(*l);}
   std::ifstream input((p.output+L"\\annotation-index.tsv").c_str());std::string header;int expected{},expectedModel=269;if(!(input>>header>>expected))throw std::runtime_error("stage2 dimension index missing");if(header=="S2DIMPREVIEW2"){if(!(input>>expectedModel))throw std::runtime_error("stage2 preview model count missing");}else if(header!="S2DIMPREVIEW1"||expected!=36)throw std::runtime_error("stage2 dimension index version invalid");if(expected<0||expected>10000||expectedModel<213||expectedModel>100000||expected>expectedModel||initial!=expectedModel)throw std::runtime_error("stage2 preview counts differ from bound source");
   std::ostringstream ds;ds<<'[';std::string h;double wanted{};int count=0;
   while(input>>h>>wanted){if(!std::isfinite(wanted)||wanted<=0||h.find_first_not_of("0123456789ABCDEF")!=std::string::npos)throw std::runtime_error("stage2 dimension index row invalid");AcDbObjectId id;const std::wstring wh(h.begin(),h.end());check(context.database->getAcDbObjectId(id,false,AcDbHandle(wh.c_str())),"preview dimension handle");auto root=open<AcDbEntity>(id);if(wcscmp(root->isA()->name(),L"TDbDimension2"))throw std::runtime_error("preview dimension type");
    Stage2PreviewParts parts(root.get());check(root->explode(parts.values),"preview public dimension display");if(parts.values.length()!=1)throw std::runtime_error("preview dimension count");auto* d=AcDbAlignedDimension::cast(static_cast<AcRxObject*>(parts.values[0]));if(!d||d==root.get()||d->database()!=nullptr||!d->objectId().isNull()||!d->ownerId().isNull())throw std::runtime_error("preview dimension ownership");
    double value{};AcString raw,formatted;check(d->measurement(value),"preview dimension measurement");check(d->dimensionText(raw),"preview dimension text override");check(d->formatMeasurement(formatted,value,raw.isEmpty()?nullptr:raw.kACharPtr()),"preview formatted measurement");if(std::fabs(value-wanted)>1e-6)throw std::runtime_error("preview dimension measurement differs");
    if(count++)ds<<',';ds<<"{\"handle\":"<<quote(wh)<<",\"expected\":"<<number(wanted)<<",\"measurement\":"<<number(value)<<",\"override\":"<<quote(raw.kACharPtr())<<",\"formatted\":"<<quote(formatted.kACharPtr())<<'}';
   }
   if(!input.eof()||count!=expected)throw std::runtime_error("preview dimension rows incomplete");ds<<']';dimensions=ds.str();context.guard();background.activate();page=nativePlotPage(context,L"stage2-A3-1-100",420,297);context.guard();const auto saved=modelIds(*context.database);final=saved.length();after=plotGeometryJson(saved);{auto l=open<AcDbLayout>(context.layout);layoutAfter=plotSettingsJson(*l);}
  }catch(const std::exception& e){error=plotError(e);}catch(...){error=quote(L"unknown stage2 preview exception");}
  background.restore();if(background.restorationFailed())error=quote(L"background plot restoration failed");globalsAfter=plotGlobalsJson();
  std::ostringstream out;out<<"{\"contract\":\"bcp.stage2.native-preview.v1\",\"controlDWG\":"<<quote(context.control)<<",\"initialModelCount\":"<<initial<<",\"finalModelCount\":"<<final<<",\"originalBefore\":"<<before<<",\"originalAfter\":"<<after<<",\"dimensions\":"<<dimensions<<",\"globalsBefore\":"<<globalsBefore<<",\"globalsAfter\":"<<globalsAfter<<",\"layoutBefore\":"<<layoutBefore<<",\"layoutAfter\":"<<layoutAfter<<",\"backgroundOverride\":"<<background.json()<<",\"windowWCS\":[9400,12000,37500,39500],\"page\":"<<page<<",\"formalFrameComplete\":false,\"styleStandardsComplete\":false,\"error\":"<<error<<'}';writeNewReport(report,out.str());
 }catch(const std::exception& e){acutPrintf(L"\nSTAGE2_PREVIEW_FAILED %hs\n",e.what());}
}
