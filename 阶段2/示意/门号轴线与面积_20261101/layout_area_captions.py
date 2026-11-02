"""One-off area-text layout; reads existing native witnesses, edits no DWG."""
from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path.insert(0, str(ROOT / '插件/tools'))
from verify_native import overlaps, stroke_hits_expanded_box
from calculate_preview_areas import inside


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def text_width(text,height):
    # Placement envelope only; exact SHX/native glyph bounds must be read after
    # creation. Deliberately includes 0.15 h extra around the full string.
    return height*(sum(1.0 if ord(c)>127 else .32 if c=='.' else .58 for c in text)+.15)


def rectangle_inside(bounds, region):
    x1,y1,x2,y2=bounds
    outer,holes=region['vertices'],region['holes']
    good=lambda p:inside(p,outer) and not any(inside(p,h) for h in holes)
    # For rectilinear boundaries, checking all corners and intersections with
    # polygon edges is sufficient, including concave notches and holes.
    if not all(good(p) for p in [(x1,y1),(x1,y2),(x2,y1),(x2,y2)]):
        return False
    for poly in [outer]+holes:
        for a,b in zip(poly,poly[1:]+poly[:1]):
            if stroke_hits_expanded_box({'arc':False,'a':a,'b':b},bounds,0):
                return False
    return True


def build(floor,edit_contract):
    areas=read(HERE/f'room-areas-floor-{floor}.json')
    preview=Path(areas['nativeSavedWallBoundsVerification']['path'])
    native=read(preview)
    source={e['handle']:e for e in native['originalBefore']}
    record=next((ROOT/'阶段1/runs/stage2-product-20261101-143628').glob(f"插件生成施工图_{'一' if floor==1 else '二'}层_生成记录*"))
    clearance_path=record/'过程/caption/door-caption-clearance.json'
    clearance=read(clearance_path)
    edits=next(f for f in edit_contract['floors'] if f['floor']==floor)
    moved={m['handle']:m['newBounds'] for m in edits['moves']}
    boxes=[]
    strokes=[]
    furniture_roots=[]
    # Never assume furniture-free because the earlier layer-specific witness
    # is empty: this actual drawing has furniture on A-立面看线.
    for e in source.values():
        b=[*e['min'][:2],*e['max'][:2]]
        typ=e['type']
        if typ in ('TDbText','AcDbText','AcDbMText','TDbDrawingName','TDbSymbElevation'):
            boxes.append({'handle':e['handle'],'kind':'text','bounds':moved.get(e['handle'],b),'gap':60})
        elif typ in ('TDbOpening','TDbGroupOpening'):
            boxes.append({'handle':e['handle'],'kind':'opening','bounds':b,'gap':80})
        elif typ=='TDbMultiStair':
            boxes.append({'handle':e['handle'],'kind':'stair','bounds':b,'gap':40})
        elif e['layer']=='A-立面看线':
            if typ=='AcDbBlockReference':
                boxes.append({'handle':e['handle'],'kind':'furniture-or-component-conservative-root-bounds','bounds':b,'gap':40})
                furniture_roots.append(e['handle'])
            elif typ=='AcDbPolyline':
                pts=e['stretch']['points']
                end=e['curve']['endParam']
                segments=list(zip(pts,pts[1:]))
                if abs(end-len(pts))<1e-6:
                    segments.append((pts[-1],pts[0]))
                straight=sum(math.dist(a,z) for a,z in segments)
                if abs(straight-e['curve']['length'])<=1e-4:
                    for a,z in segments:
                        if math.dist(a,z)>1e-6:
                            strokes.append({'handle':e['handle'],'kind':'polyline-stroke','a':a[:2],'b':z[:2],'arc':False,'gap':40})
                else:
                    boxes.append({'handle':e['handle'],'kind':'unresolved-curved-polyline-conservative-bounds','bounds':b,'gap':40})
            elif typ=='AcDbLine':
                strokes.append({'handle':e['handle'],'kind':'line','a':e['curve']['start'][:2],'b':e['curve']['end'][:2],'arc':False,'gap':40})
            elif typ=='AcDbCircle':
                boxes.append({'handle':e['handle'],'kind':'circle-conservative-bounds','bounds':b,'gap':40})
    for wall in areas['walls']:
        boxes.append({'handle':wall['handle'],'kind':'joined-wall-body','bounds':wall['rect'],'gap':40})
    for dim in clearance['dimensionAudit']:
        a,b=dim['baseline']
        strokes.append({'handle':dim['handle'],'kind':'dimension-baseline','a':a[:2],'b':b[:2],'arc':False,'gap':30})
        for txt in dim['texts']:
            boxes.append({'handle':dim['handle'],'kind':'actual-dimension-glyph','bounds':txt['fontBounds'],'gap':60})
    results=[]
    for region in areas['regions']:
        height=240
        shared=len(region['labels'])>1
        text=('连通净' if shared else '净')+f"{region['areaM2']:.2f}㎡"
        width=text_width(text,height)
        poly=region['vertices']
        minx,miny,maxx,maxy=min(p[0] for p in poly),min(p[1] for p in poly),max(p[0] for p in poly),max(p[1] for p in poly)
        centres=[[(label['bounds'][0]+label['bounds'][2])/2,label['bounds'][1]-450] for label in region['labels']]
        if not centres:
            centres=[region['recommendedAnnotationPoint']]
        candidates=[]
        for origin in centres:
            for dy in (0,-200,200,-400,400,-600,600,-900,900):
                for dx in (0,-200,200,-400,400,-600,600,-900,900):
                    candidates.append([origin[0]+dx,origin[1]+dy])
        for j in range(max(1,math.ceil((maxy-miny)/160))):
            for i in range(max(1,math.ceil((maxx-minx)/160))):
                candidates.append([minx+80+i*160,miny+80+j*160])
        seen=set()
        evaluated=[]
        for pos in candidates:
            key=tuple(round(v,5) for v in pos)
            if key in seen:
                continue
            seen.add(key)
            bounds=[pos[0]-width/2,pos[1]-height/2,pos[0]+width/2,pos[1]+height/2]
            if not rectangle_inside(bounds,region):
                continue
            contacts=[{'handle':b['handle'],'kind':b['kind']} for b in boxes if overlaps(bounds,b['bounds'],b['gap'])]
            contacts.extend({'handle':s['handle'],'kind':s['kind']} for s in strokes if stroke_hits_expanded_box(s,bounds,s['gap']))
            score=min(math.dist(pos,c) for c in centres)
            evaluated.append((len(contacts),score,pos,bounds,contacts))
        if evaluated:
            count,score,position,bounds,contacts=min(evaluated,key=lambda t:(t[0],t[1]))
            status='candidate-clear' if not contacts else 'review-required'
        else:
            position=region['recommendedAnnotationPoint']
            bounds=[position[0]-width/2,position[1]-height/2,position[0]+width/2,position[1]+height/2]
            contacts=[{'kind':'text-envelope-does-not-fit-net-region-at-240mm'}]
            count,score,status=len(contacts),None,'review-required'
        results.append({'floor':floor,'region':region['id'],'text':text,'areaM2':region['areaM2'],'position':[position[0],position[1],0],'height':height,'rotation':0,'horizontalMode':'center','verticalMode':'middle','templateCaptionHandle':next(e['handle'] for e in source.values() if e['type']=='AcDbText'),'sourceRoomLabels':region['labels'],'assumedWidth':width,'assumedBounds':bounds,'status':status,'candidateCount':len(evaluated),'remainingContacts':contacts,'displacementFromPreferredMm':score})
        boxes.append({'handle':region['id'],'kind':'new-area-text','bounds':bounds,'gap':60})
    return {'floor':floor,'sourceDWG':areas['sourceDWG'],'sourceDWGSHA256':areas['sourceDWGSHA256'],'areaDataSHA256':sha(HERE/f'room-areas-floor-{floor}.json'),'previewJSON':str(preview),'previewSHA256':sha(preview),'doorCaptionWitness':str(clearance_path),'doorCaptionWitnessSHA256':sha(clearance_path),'furnitureAuditCapturedRoots':len(clearance['interiorLowerPlacement']['furnitureAudit']),'additionalFurnitureBoundsRootCount':len(furniture_roots),'additionalFurnitureBoundsRoots':furniture_roots,'captionCount':len(results),'clearCandidates':sum(r['status']=='candidate-clear' for r in results),'captions':results}


if __name__=='__main__':
    cp=HERE/'门号与轴衔接编辑合同_v1.json'
    contract=read(cp)
    data={'contract':'bcp.one-off-area-caption-layout.v1','pluginSourceChanged':False,'units':'mm','scope':'One-off area-caption placement candidates only. Actual native glyph extents, save/reopen and visual QA pending.','doorEditContractSHA256':sha(cp),'assumptions':['One horizontal centered text per connected net region; no guessed room names.','Text height 240 mm, same native text style as existing opening caption template.','Assumed glyph width: CJK/square-metre 1h, ASCII digits .58h, dot .32h, plus .15h envelope; actual SHX/text extents must be checked after creation.','Prefer below original room name; fallback to geometric blank candidates.','Door edits in the current v1 contract are applied to collision boxes. Any later door move requires post-check.','A-立面看线 block roots conservatively occupy whole public bounds; straight polyline/line strokes use actual saved vertices, not filled interiors.','Net region areas and contours are read unchanged from room-areas-floor-N.json.'],'floors':[build(1,contract),build(2,contract)]}
    out=HERE/'area-caption-layout.json'
    out.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
    for f in data['floors']:
        print('floor',f['floor'],'clear',f['clearCandidates'],'/',f['captionCount'])
        for c in f['captions']:
            print(c['region'],c['text'],c['status'],c['position'],c['remainingContacts'])
