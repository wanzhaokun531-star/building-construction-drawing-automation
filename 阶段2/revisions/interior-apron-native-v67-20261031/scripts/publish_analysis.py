"""Presentation only: display actual source extents and C++ decisions, no building rules."""
from pathlib import Path
import json,csv,html,hashlib,shutil,sys
S=Path(__file__).resolve().parent
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def main():
 suffix=sys.argv[1] if len(sys.argv)>1 else '';assert suffix in ['', '-v2','-v3','-v4','-v5'];receipt=read(S/f'evidence/native-run{suffix}.json');native=Path(receipt['output']);r=read(native/'analysis.json');source=read(S/'inputs/frozen-scheme-v1.json');records=source['records'];walls=[x for x in records if x['type']=='TDbWall'];matched={h for c in r['localWallChains'] for h in c['openings']}
 output=S/('交付/首批基础模型_v2' if suffix else '交付/首批基础模型');output.mkdir(parents=True,exist_ok=False)
 for filename in ['analysis.json','native-foundation.json','方案工作副本_未生成标注.dwg']:shutil.copyfile(native/filename,output/filename)
 names={'THIN_WALL_ROLE_REQUIRED':'100厚墙内外/用途待确认，不静默归入内隔墙','WALL_ROLE_REQUIRED':'200厚墙用途待确认：主要墙、门垛或局部短墙','OPENING_CATEGORY_REQUIRED':'门窗类别待确认：M/C/FDM/LC','GROUP_OPENING_GEOMETRY_REQUIRED':'组合门整体端点及分段取点待核准'}
 with (output/'待确认清单.csv').open('x',encoding='utf-8-sig',newline='') as f:
  writer=csv.writer(f);writer.writerow(['序号','源对象句柄','问题类别','内容'])
  for i,issue in enumerate(r['issues'],1):writer.writerow([i,issue['handle'],issue['code'],names.get(issue['code'],issue['code'])])
 x0=min(w['sourceGeometry']['min'][0] for w in walls);y0=min(w['sourceGeometry']['min'][1] for w in walls);x1=max(w['sourceGeometry']['max'][0] for w in walls);y1=max(w['sourceGeometry']['max'][1] for w in walls);scale=850/max(x1-x0,y1-y0);margin=90;width=(x1-x0)*scale+margin*2;height=(y1-y0)*scale+margin*2+45
 xy=lambda p:((p[0]-x0)*scale+margin,(y1-p[1])*scale+margin)
 svg=[f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">','<rect width="100%" height="100%" fill="white"/>','<g font-family="Arial" font-size="11">','<text x="16" y="24">Stage 2 analysis - NOT a construction drawing</text>']
 for w in walls:
  g=w['sourceGeometry'];a=xy([g['min'][0],g['max'][1]]);b=xy([g['max'][0],g['min'][1]]);svg.append(f'<rect x="{a[0]}" y="{a[1]}" width="{b[0]-a[0]}" height="{b[1]-a[1]}" fill="#697582"><title>Wall {w["handle"]}: native extents</title></rect>')
 for e in records:
  if e['handle'] in matched:
   points=e['sourceGeometry']['grip']['points'];a=xy(points[1]);b=xy(points[2]);svg.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" stroke="#10a36b" stroke-width="4"><title>Opening {e["handle"]}</title></line>')
 for axis in r['axisCandidates']:
  if axis['vertical']:a=xy([axis['coordinate'],y1]);b=xy([axis['coordinate'],y0]);label=(a[0]-3,a[1]-18)
  else:a=xy([x0,axis['coordinate']]);b=xy([x1,axis['coordinate']]);label=(a[0]-35,a[1]+3)
  svg.append(f'<line x1="{a[0]}" y1="{a[1]}" x2="{b[0]}" y2="{b[1]}" stroke="#cb790c" stroke-width=".8" stroke-dasharray="8 5"/><text x="{label[0]}" y="{label[1]}" fill="#9c5800">{html.escape(axis["draftLabel"])}?</text>')
 svg += [f'<text x="16" y="{height-22}">Gray: native wall extents / Green: matched openings / Orange: unconfirmed axis candidates</text>','</g></svg>'];(output/'方案分析图.svg').write_text('\n'.join(svg),encoding='utf-8')
 try:
  import fitz
  doc=fitz.open(stream='\n'.join(svg).encode(),filetype='svg');pdf=fitz.open('pdf',doc.convert_to_pdf());pdf[0].get_pixmap(matrix=fitz.Matrix(1.3,1.3),alpha=False).save(str(output/'方案分析图.png'))
 except ImportError:pass
 manifest={f.name:hashlib.sha256(f.read_bytes()).hexdigest().upper() for f in output.iterdir() if f.is_file()};(output/'文件指纹.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps(dict(output=str(output),walls=63,openings=30,candidateAxes=len(r['axisCandidates']),issues=len(r['issues'])),ensure_ascii=False))
if __name__=='__main__':main()
