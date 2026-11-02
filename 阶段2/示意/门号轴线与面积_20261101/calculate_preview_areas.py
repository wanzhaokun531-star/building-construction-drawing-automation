"""One-off DWG-preview geometry analysis. Does not edit plugin code or DWGs.

Uses the actual native-read wall axes/left-right widths. All wall spans are solid
for room-area closure, including door/window spans; room recesses are not added.
The coordinate arrangement computes rectangle-union subtraction exactly after
coalescing existing <= 1e-6 mm numerical jitter. No geometry libraries needed.
"""
from __future__ import annotations

import hashlib
import json
import math
from collections import defaultdict, deque
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
JOB = ROOT / "阶段1/runs/stage2-product-20261101-143628"
OUT = Path(__file__).resolve().parent
EPS = 1e-6
ROOM_NAMES = {"卧室", "厨房", "卫", "卫生间", "公卫", "楼梯", "客厅", "餐厅", "玄关", "烤火房", "储物间", "书房", "堂屋", "棋牌室", "起居厅", "杂物间", "水吧"}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def inside(point, polygon):
    x, y = point
    hit = False
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        if (a[1] > y) != (b[1] > y) and x < (b[0] - a[0]) * (y - a[1]) / (b[1] - a[1]) + a[0]:
            hit = not hit
    return hit


def signed_area(poly):
    # Local origin avoids cancellation for far-translated floor coordinates.
    ox, oy = poly[0]
    return sum((a[0]-ox)*(b[1]-oy)-(b[0]-ox)*(a[1]-oy) for a, b in zip(poly, poly[1:]+poly[:1])) / 2


def coalesce(values):
    groups = []
    for value in sorted(set(values)):
        if groups and value - groups[-1][0] <= EPS:
            groups[-1].append(value)
        else:
            groups.append([value])
    coords = [sum(g) / len(g) for g in groups]
    mapping = {v: i for i, g in enumerate(groups) for v in g}
    correction = max(abs(v - coords[mapping[v]]) for v in mapping)
    return coords, mapping, correction


def simplify(poly):
    changed = True
    while changed and len(poly) > 3:
        changed = False
        for i, b in enumerate(poly):
            a, c = poly[i-1], poly[(i+1) % len(poly)]
            if (abs(a[0]-b[0]) <= EPS and abs(b[0]-c[0]) <= EPS) or (abs(a[1]-b[1]) <= EPS and abs(b[1]-c[1]) <= EPS):
                poly.pop(i)
                changed = True
                break
    return poly


def trace_boundary(cells, xs, ys):
    edges = set()
    for i, j in cells:
        for neighbour, edge in (
            ((i, j-1), ((i, j), (i+1, j))),
            ((i+1, j), ((i+1, j), (i+1, j+1))),
            ((i, j+1), ((i+1, j+1), (i, j+1))),
            ((i-1, j), ((i, j+1), (i, j))),
        ):
            if neighbour not in cells:
                edges.add(edge)
    starts = defaultdict(list)
    for a, b in edges:
        starts[a].append(b)
    assert all(len(v) == 1 for v in starts.values()), "ambiguous point-touch boundary"
    loops = []
    while edges:
        initial = min(edges)
        current = initial
        loop = []
        while True:
            assert current in edges, "boundary loop failed"
            edges.remove(current)
            loop.append([xs[current[0][0]], ys[current[0][1]]])
            nxt = (current[1], starts[current[1]][0])
            if nxt == initial:
                break
            current = nxt
        loops.append(simplify(loop))
    return sorted(loops, key=lambda p: -signed_area(p))


def distance_to_segment(point, a, b):
    dx, dy = b[0]-a[0], b[1]-a[1]
    t = max(0, min(1, ((point[0]-a[0])*dx+(point[1]-a[1])*dy)/(dx*dx+dy*dy)))
    return math.hypot(point[0]-a[0]-t*dx, point[1]-a[1]-t*dy)


def calculate(floor):
    records = sorted(JOB.glob(f"插件生成施工图_{'一' if floor == 1 else '二'}层_生成记录*"))
    assert len(records) == 1
    foundation = records[0] / "过程/foundation"
    cache = foundation / "current-plan.tsv"
    apath = foundation / "analysis.json"
    analysis = json.loads(apath.read_text(encoding="utf-8"))
    contours = analysis["wallOuterBodyContours"]
    walls, labels = [], []
    for line in cache.read_text(encoding="utf-8").splitlines()[1:]:
        row = line.split()
        if row[0] == "W":
            x1, y1, x2, y2, left, right = map(float, row[2:8])
            dx, dy = x2-x1, y2-y1
            assert abs(dx) <= EPS or abs(dy) <= EPS, "nonrectilinear wall"
            # Preserve exact longitudinal bounds, use mean cross-coordinate to
            # remove only existing sub-epsilon axial noise before arrangement.
            if abs(dy) <= EPS:
                y = (y1+y2)/2
                sign = 1 if dx > 0 else -1
                r = [min(x1,x2), min(y+sign*left,y-sign*right), max(x1,x2), max(y+sign*left,y-sign*right)]
            else:
                x = (x1+x2)/2
                sign = 1 if dy > 0 else -1
                r = [min(x-sign*left,x+sign*right), min(y1,y2), max(x-sign*left,x+sign*right), max(y1,y2)]
            walls.append({"handle": row[1], "rect": r, "rawRect": r[:], "start": [x1,y1], "end": [x2,y2], "horizontal":abs(dy)<=EPS,"leftMm": left, "rightMm": right})
        elif row[0] == "R":
            bounds = list(map(float, row[2:6]))
            labels.append({"handle": row[1], "bounds": bounds, "text": bytes.fromhex(row[6]).decode("utf-8"), "centre": [(bounds[0]+bounds[2])/2,(bounds[1]+bounds[3])/2]})
    assert len(walls) == 34
    native_preview = ROOT / ("阶段1/runs/stage2-preview-dynamic-20261101-143804/stage2-preview-native.json" if floor == 1 else "阶段1/runs/stage2-preview-dynamic-20261101-143807/stage2-preview-native.json")
    native = json.loads(native_preview.read_text(encoding='utf-8-sig'))
    native_walls = {e['handle']:e for e in native['originalBefore'] if e['type']=='TDbWall'}
    assert set(native_walls)=={w['handle'] for w in walls},'native saved wall coverage differs'
    wall_bounds_error=0.0
    for wall in walls:
        e=native_walls[wall['handle']]
        actual=[*e['min'][:2],*e['max'][:2]]
        error=max(abs(a-b) for a,b in zip(actual,wall['rawRect']))
        assert error<=EPS,'wall-body width/rectangle differs from actual saved native extents'
        wall_bounds_error=max(wall_bounds_error,error)
        wall['nativeSavedBodyBounds']=actual
    # Raw rectangles ending at centreline intersections leave a spurious
    # half-thickness square outside every convex exterior corner. Extend only
    # genuinely meeting perpendicular endpoints through the other wall body.
    # This is orthogonal square/miter joining, not a blanket wall-end buffer.
    junction_extensions=[]
    for wall in walls:
        for endpoint in (wall['start'],wall['end']):
            for other in walls:
                if wall['horizontal']==other['horizontal']:
                    continue
                x,y=endpoint
                a,b=other['start'],other['end']
                meets=(abs(x-(a[0]+b[0])/2)<=EPS and min(a[1],b[1])-EPS<=y<=max(a[1],b[1])+EPS) if wall['horizontal'] else (abs(y-(a[1]+b[1])/2)<=EPS and min(a[0],b[0])-EPS<=x<=max(a[0],b[0])+EPS)
                if not meets:
                    continue
                old=wall['rect'][:]
                indices=(0,2) if wall['horizontal'] else (1,3)
                for low,high in [indices]:
                    wall['rect'][low]=min(wall['rect'][low],other['rawRect'][low])
                    wall['rect'][high]=max(wall['rect'][high],other['rawRect'][high])
                if old!=wall['rect']:
                    junction_extensions.append({'wall':wall['handle'],'meetingWall':other['handle'],'point':endpoint,'before':old,'after':wall['rect'][:]})
    xs, xm, xc = coalesce([p[0] for p in sum(contours, [])] + [w['rect'][i] for w in walls for i in (0,2)])
    ys, ym, yc = coalesce([p[1] for p in sum(contours, [])] + [w['rect'][i] for w in walls for i in (1,3)])
    gross, occupied = set(), set()
    for i in range(len(xs)-1):
        for j in range(len(ys)-1):
            middle = ((xs[i]+xs[i+1])/2, (ys[j]+ys[j+1])/2)
            if any(inside(middle,p) for p in contours):
                gross.add((i,j))
    for wall in walls:
        r=wall['rect']
        for i in range(xm[r[0]],xm[r[2]]):
            for j in range(ym[r[1]],ym[r[3]]):
                occupied.add((i,j))
    available = gross - occupied
    cell_area = lambda cell: (xs[cell[0]+1]-xs[cell[0]])*(ys[cell[1]+1]-ys[cell[1]])
    components = []
    todo = set(available)
    while todo:
        start = min(todo)
        todo.remove(start)
        component, queue = {start}, deque([start])
        while queue:
            i,j = queue.popleft()
            for neighbour in ((i-1,j),(i+1,j),(i,j-1),(i,j+1)):
                if neighbour in todo:
                    todo.remove(neighbour)
                    component.add(neighbour)
                    queue.append(neighbour)
        components.append(component)
    components.sort(key=lambda c: (min(xs[i] for i,j in c), -max(ys[j+1] for i,j in c)))
    regions = []
    assigned = set()
    for index,cells in enumerate(components,1):
        loops = trace_boundary(cells,xs,ys)
        area = sum(cell_area(c) for c in cells)
        assert abs(sum(signed_area(p) for p in loops)-area) < 0.01
        contains = lambda point: inside(point,loops[0]) and not any(inside(point,h) for h in loops[1:])
        names = [r for r in labels if r['text'] in ROOM_NAMES and contains(r['centre'])]
        assigned.update(r['handle'] for r in names)
        preferred = [[(r['bounds'][0]+r['bounds'][2])/2,r['bounds'][1]-450] for r in names]
        boundary_edges=[(a,b) for p in loops for a,b in zip(p,p[1:]+p[:1])]
        clearance=lambda p:min(distance_to_segment(p,a,b) for a,b in boundary_edges)
        centres=[[(xs[i]+xs[i+1])/2,(ys[j]+ys[j+1])/2] for i,j in cells]
        candidates=[p for p in preferred if contains(p) and clearance(p)>250]
        annotation=candidates[0] if candidates else max(centres,key=clearance)
        regions.append({"id":f"F{floor}-R{index:02d}","areaMm2":area,"areaM2":area/1e6,"vertices":loops[0],"holes":loops[1:],"labels":[{"handle":r['handle'],"text":r['text'],"bounds":r['bounds']} for r in names],"status":"shared-open-space" if len(names)>1 else "named-room" if names else "name-required","recommendedAnnotationPoint":annotation,"annotationPlacementScope":"geometric interior only; furniture/text collision QA still required","boundaryClosed":True})
    gross_area=sum(cell_area(c) for c in gross)
    wall_area=sum(cell_area(c) for c in gross & occupied)
    net_area=sum(r['areaMm2'] for r in regions)
    assert abs(gross_area-wall_area-net_area)<0.01
    contour_areas=[abs(signed_area(p)) for p in contours]
    assert abs(sum(contour_areas)-gross_area)<0.01
    dwg=JOB/f"插件生成施工图_{'一' if floor==1 else '二'}层.dwg"
    result={"contract":"bcp.one-off-room-area-preview.v1","floor":floor,"units":"mm","sourceDWG":str(dwg),"sourceDWGSHA256":sha(dwg),"cache":str(cache),"cacheSHA256":sha(cache),"analysis":str(apath),"analysisSHA256":sha(apath),"pluginCodeChanged":False,"scope":"One-off preview calculation; not implemented plugin area feature or legal construction-area certification.","method":"Actual wall start/end plus left/right widths -> continuous wall-body rectangles with meeting perpendicular endpoints extended through joint body -> union, then subtract within actual closed exterior-wall body contours; complete doorway spans virtually closed at wall-face boundary. Axis-aligned coordinate-cell arrangement.","numericalEpsilonMm":EPS,"maxCoordinateCoalescingMm":max(xc,yc),"finishLayersDeducted":False,"exteriorContours":contours,"exteriorContourAreasM2":[a/1e6 for a in contour_areas],"totalExteriorContourAreaM2":gross_area/1e6,"netRegionAreaTotalM2":net_area/1e6,"wallBodyAreaM2":wall_area/1e6,"netPlusWallEqualsExterior":True,"wallCount":len(walls),"walls":walls,"junctionExtensions":junction_extensions,"regions":regions,"unassignedRoomLabels":[r for r in labels if r['text'] in ROOM_NAMES and r['handle'] not in assigned],"excludedOutsideOrNonRoomLabels":[r for r in labels if r['text'] not in ROOM_NAMES],"boundaries":["Wall body faces, with no finish/insulation layer deduction.","Actual perpendicular wall endpoints use square/miter wall-body joining; unconnected free endpoints are not buffered.","Connected open functional spaces are one net region; no furniture/axis-based artificial subdivision.","Unnamed enclosed regions remain name-required.","Exterior total includes the actual closed external wall contours only; exterior porch and second-floor open terrace are not included in these wall contours.","Stair enclosure/opening and floor-void treatment is not inferred from wall geometry; the net floor result is a geometric preview.","Existing original area text is not reused as calculation input."]}
    result['nativeSavedWallBoundsVerification']={'path':str(native_preview),'SHA256':sha(native_preview),'wallsChecked':len(walls),'maxErrorMm':wall_bounds_error,'passed':True,'scope':'Each unjoined wall-body raw rectangle matches saved TDbWall public geometry extents. Perpendicular square/miter junction fill is a geometric preview convention; public leaf/junction edges were not separately captured in this record.'}
    target=OUT/f"room-areas-floor-{floor}.json"
    target.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
    return result


if __name__ == '__main__':
    for f in (1,2):
        r=calculate(f)
        print(f"floor {f}: exterior={r['totalExteriorContourAreaM2']:.6f} m2, net={r['netRegionAreaTotalM2']:.6f} m2, regions={len(r['regions'])}, coordinate coalescing={r['maxCoordinateCoalescingMm']:.3g} mm")
        for region in r['regions']:
            print(region['id'], '/'.join(t['text'] for t in region['labels']) or '待确认',f"{region['areaM2']:.6f}",region['status'])
