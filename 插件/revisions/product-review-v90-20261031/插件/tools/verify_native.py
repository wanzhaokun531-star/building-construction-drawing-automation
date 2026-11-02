"""Independently check the reopened DWG geometry against native measured records."""
from pathlib import Path
import json,math,sys,hashlib
ROOT=Path(__file__).resolve().parents[2];APP=ROOT/'插件';S=ROOT/'阶段2'
def read(p):return json.loads(Path(p).read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
def need(value,message):
 if not value:raise ValueError(message)
def near(a,b):return len(a)==len(b) and all(abs(x-y)<1e-6 for x,y in zip(a,b))
def points(a,b):return len(a)==len(b) and all(near(x,y) for x,y in zip(a,b))
def box(e):return [*e['min'][:2],*e['max'][:2]]
def overlaps(a,b,gap):return a[0]<b[2]+gap-1e-6 and a[2]>b[0]-gap+1e-6 and a[1]<b[3]+gap-1e-6 and a[3]>b[1]-gap+1e-6
def verify_geometry(a,w,c,p):
 need(all(r['error'] is None for r in (a,w,c,p)),'native stage failed')
 need(not c['COMUsed'] and not c['ComputerUseUsed'] and not c['TianzhengBusinessCommandsUsed'] and w['command']=='TMergeDim' and w['commandCount']==82,'unapproved route')
 need(a['apronRootCount']==0 and a['nativeAxisSets']==18 and a['logicalFullAxes']==20 and a['nativeFullAxisLines']==22 and len(a['captions'])==27,'wrong foundation coverage')
 need(p['initialModelCount']==p['finalModelCount']==308 and p['originalBefore']==p['originalAfter'] and p['globalsBefore']==p['globalsAfter'] and p['layoutBefore']==p['layoutAfter'],'opened result changed during audit')
 idx={e['handle']:e for e in p['originalBefore']};need(len(idx)==308,'duplicate or missing model roots')
 targets={x['handle'] for x in a['captions'] if x['text'].startswith('M')};need(len(targets)==14,'missing M captions')
 before={e['handle']:e for e in w['protectedBefore']};need(len(before)==284,'protected foundation scope differs')
 for h,e in before.items():
  actual=idx[h];need(actual['type']==e['type'] and actual['layer']==e['layer'],'protected class/layer changed '+h)
  if h in targets:continue
  need(actual['extentStatus']==e['extentStatus'] and actual['grip']['status']==e['gripStatus'],'protected geometry status changed '+h)
  if e['extentStatus']==0:need(points([actual['min'],actual['max']],e['bounds']),'protected bounds changed '+h)
  need(points(actual['grip']['points'],e['grips']),'protected grips changed '+h)
 saved={x['handle']:x for x in w['saved']};need(len(saved)==24 and sum(x['segments'] for x in saved.values())==106,'wrong whole-chain coverage')
 need(len(w['live'])==24 and sum(x['sharedNodeChecks'] for x in w['live'])==82 and all(x['singleGripWholeBaseline'] and x['singleSharedNodeLinked'] and x['exercisePerformed'] for x in w['live']),'whole-chain editing was not exercised')
 for h,row in saved.items():
  e=idx[h];need(e['type']=='TDbDimension2' and not e['proxy'] and points(e['grip']['points'],row['grips']) and len(row['grips'])==2*row['segments']+6,'saved native chain changed '+h)
  gp=e['grip']['points'];need(all(abs(v-math.dist(gp[i+2],gp[i+3]))<1e-6 for i,v in enumerate(row['values'])),'dimension measurement changed '+h)
 need(sum(e['type']=='TDbDimension2' for e in idx.values())==32,'dimensions wrapped or substituted')
 audit={x['handle']:x for x in c['dimensionAudit']};need(set(audit)==set(saved),'missing dimension glyph audit');dim_boxes=[]
 for h,row in audit.items():
  n=saved[h]['segments'];gp=idx[h]['grip']['points'];need(row['segments']==n and len(row['texts'])==n and points(row['baseline'],gp[:2]),'dimension font/baseline audit differs')
  p0,p1=gp[:2];dim_boxes.append([min(p0[0],p1[0])-10,min(p0[1],p1[1])-10,max(p0[0],p1[0])+10,max(p0[1],p1[1])+10])
  for i,t in enumerate(row['texts']):
   pos=t['position'][0];b=t['fontBounds'];need(near(pos,gp[n+3+i]) and abs(t['value']-math.dist(gp[2+i],gp[3+i]))<1e-6,'font audit differs from actual native chain')
   need(t['height']>0 and b[0]<b[2] and b[1]<b[3] and b[0]<=pos[0]<=b[2] and b[1]<=pos[1]<=b[3],'invalid native dimension glyph');dim_boxes.append(b)
 openings=[e for e in idx.values() if e['type'] in ('TDbOpening','TDbGroupOpening')];fixed=[e for e in idx.values() if e['type'] in ('TDbText','TDbSymbElevation','AcDbText','AcDbMText') and e['handle'] not in targets]
 rows={x['handle']:x for x in c['targets']};need(set(rows)==targets and len(c['targets'])==14,'missing door-caption relationship')
 for h,row in rows.items():
  e=idx[h];old=before[h];need(e['type']=='AcDbText' and not e['proxy'] and row['styleUnchanged'],'door caption class/style changed')
  need(all(abs((e['max'][i]-e['min'][i])-(old['bounds'][1][i]-old['bounds'][0][i]))<1e-6 for i in (0,1)),'door caption size changed')
  pos=e['stretch']['points'][0];aa,bb=row['anchorA'][0],row['anchorB'][0];along=int(abs(aa[0]-bb[0])<1e-6);normal=1-along
  anchors=idx[row['opening']]['grip']['points'][1:3];need((points([aa,bb],anchors) or points([bb,aa],anchors)),'caption bound to wrong opening')
  need(near(pos,row['after'][0]) and row['side']*(pos[normal]-aa[normal])>0 and min(aa[along],bb[along])-1e-6<=pos[along]<=max(aa[along],bb[along])+1e-6,'caption side/projection changed')
  oldside=1 if old['grips'][0][normal]>(aa[normal]+bb[normal])/2 else -1;need(row['side']==oldside,'caption switched default side')
  need(not any(overlaps(box(e),box(o),80) for o in openings),'door number overlaps door/window graphics')
  need(not any(overlaps(box(e),box(o),60) for o in fixed) and not any(overlaps(box(e),box(idx[k]),60) for k in targets if k!=h),'door number overlaps fixed text')
  need(not any(overlaps(box(e),b,60) for b in dim_boxes),'door number overlaps dimension text or baseline')
 return dict(modelRoots=308,wholeNativeChains=24,measuredSegments=106,sharedNodeEdits=82,doorCaptions=14,openingCaptions=27,apronAdded=0)
def verify_axes(a,actual):
 need(actual['error'] is None and actual['readOnly'] and actual['carriers']==18 and actual['logicalGroups']==20 and actual['axisLines']==22,'opened axis audit failed')
 expected={x['handle']:x for x in a['axes']};rows={x['nativeHandle']:x for x in actual['axes']};need(set(rows)==set(expected),'native axis carrier differs');ends={'top':set(),'bottom':set(),'left':set(),'right':set()}
 for h,row in rows.items():
  e=expected[h];need(row['inverted']==e['clipInverted'] and near(row['min'],e['clipMin']) and near(row['max'],e['clipMax']),'opened axis clip changed')
  for i,circle in enumerate(row['circles']):
   need(near(circle['centre'][0],e['circles'][i]['centre']) and abs(circle['radius']-400)<1e-6,'axis circle moved')
   x,y=circle['centre'][0][:2];inside=row['min'][0]<=x<=row['max'][0] and row['min'][1]<=y<=row['max'][1];visible=not inside if row['inverted'] else inside
   need(visible==e['showPositive' if i else 'showNegative'],'axis end visibility differs')
   if visible:ends[('top' if i else 'bottom') if e['vertical'] else ('right' if i else 'left')].add(e['label'])
 need(ends==dict(top=set('135789'),bottom=set('124689'),left=set('ADEFGJL'),right=set('BFGKL')),'wrong facade axis numbers')
 expected_lines={p['handle']:[p['start'],p['end']] for row in a['fullAxisLines'] for p in row['pieces']};lines={p['handle']:p for row in actual['groups'] for p in row['lines']};need(set(lines)==set(expected_lines) and all(points(lines[h]['curve'],v) and lines[h]['linetype']=='DOTE' for h,v in expected_lines.items()),'logical axis stroke geometry changed')
 return dict(nativeAxisCarriers=18,logicalGroups=20,axisLines=22,visibleEnds=sum(map(len,ends.values())))
def main(version):
 run=read(APP/f'evidence/native-product-{version}.json');result=run['result'];b=run['binding'];preview=read(APP/f'evidence/preview-product-{version}.json');record=Path(result['records']);job=Path(preview['job']);p=read(job/'stage2-preview-native.json')
 need(run['error'] is None and result['completed'] and result['preparationDocumentClosed'] and result['intermediateDocumentClosed'] and result['sourceMemoryObserved'],'incomplete product flow')
 need(read(Path(b['job'])/'product-result.json')==result,'embedded result differs from actual native completion')
 need(Path(result['output']).resolve()==(Path(b['job'])/'插件生成施工图.dwg').resolve(),'result is outside the exact tested output')
 need(record.parent.resolve()==Path(b['job']).resolve() and record.name.startswith('插件生成施工图_生成记录_'),'native records belong to another job')
 need(result['output']==b['output']==preview['source'] and sha(result['output'])==result['outputSHA256']==preview['sourceSHA256']==sha(job/'control.dwg'),'DWG source/output binding differs')
 need(sha(job/'stage2-preview-native.json')==preview['nativeReportSHA256'] and sha(Path(b['job'])/'product-result.json')==run['resultSHA256'],'result evidence changed')
 need(sha(b['input'])==sha(result['source'])==result['sourceSHA256']==b['inputSHA256'],'input changed')
 for name,digest in b['sourceFiles'].items():need(sha(S/'src'/name)==digest,'engine source changed '+name)
 for name,digest in b['productSources'].items():need(sha(APP/name)==digest,'product source changed '+name)
 need(sha(APP/'bin/BuildingPlanPlugin2020.arx')==b['moduleSHA256'],'candidate binary changed')
 a=read(record/'过程/foundation/annotations.json');w=read(record/'过程/join/whole-dimension-chains.json');c=read(record/'过程/caption/door-caption-clearance.json')
 need(sha(record/'过程/join/control.bak')==w['inputSHA256'] and sha(record/'过程/join/chain.tsv')==w['contractSHA256'],'prepared drawing or node contract changed')
 need(sha(record/'过程/join/方案施工图_整体连续天正尺寸.dwg')==w['outputSHA256']==c['inputSHA256'] and sha(record/'过程/caption/doors.tsv')==c['contractSHA256'] and sha(record/'过程/caption/方案施工图_门编号避让.dwg')==c['outputSHA256'],'native intermediate output changed')
 summary=verify_geometry(a,w,c,p);summary.update(verify_axes(a,read(job/'product-axis-document-audit.json')))
 report=dict(contract='bcp.product-independent-qa.v1',version=version,summary=summary,DWG=result['output'],DWGSHA256=sha(result['output']),moduleSHA256=b['moduleSHA256'],integrationSHA256=sha(APP/f'evidence/native-product-{version}.json'),previewSHA256=sha(APP/f'evidence/preview-product-{version}.json'),nativeReports={str(f.relative_to(record)):sha(f) for f in record.rglob('*.json')},error=None)
 out=APP/f'evidence/qa-product-{version}.json'
 if out.exists():need(read(out)==report,'saved QA record differs from actual recomputation')
 else:out.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 print(json.dumps(summary,ensure_ascii=False))
if __name__=='__main__':main(sys.argv[1])
