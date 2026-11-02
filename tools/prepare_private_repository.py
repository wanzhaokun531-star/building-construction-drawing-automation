"""Prepare a clean private-repository handoff; does not contact GitHub."""
from pathlib import Path
import os,json,hashlib,re,shutil,datetime,collections,tomllib
ROOT=Path(__file__).resolve().parents[1];STAMP=datetime.datetime.now().strftime('%Y%m%d-%H%M%S');ARCHIVE=ROOT.parent/'项目清理归档'/f'{ROOT.name}_{STAMP}'
assert not ARCHIVE.exists();ARCHIVE.mkdir(parents=True);assert ROOT.resolve() not in ARCHIVE.resolve().parents
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest().upper()
# Preserve current source and delivered samples; archive is reversible, not deletion.
protected={p.relative_to(ROOT).as_posix():sha(p) for part in ['插件/src','阶段2/src','立面插件/src','阶段0/输入快照'] for p in (ROOT/part).rglob('*') if p.is_file()}
protected['阶段2/示意/原生轴号与可拉伸轴线_20261102/方案施工图_二层_原生轴号与可拉伸轴线确认样张_v12.dwg']=sha(ROOT/'阶段2/示意/原生轴号与可拉伸轴线_20261102/方案施工图_二层_原生轴号与可拉伸轴线确认样张_v12.dwg')
# Current build scripts depend on these exact frozen source/receipt chains.
build=ROOT/'立面插件/build';selected={'m1-20261101-221054-935-review-fix1-three-contracts-green-f81070ee','i1-20261101-210949-709-fix1-critical-output-paged-issues-green-772efe6e'};queue=list(selected)
while queue:
 n=queue.pop();p=build/n/'receipt.json'
 if not p.exists():continue
 for m in re.finditer(r'立面插件(?:\\\\|/|\\)build(?:\\\\|/|\\)([^/\\"\s]+)',p.read_text('utf-8-sig',errors='replace')):
  q=m.group(1)
  if q not in selected and (build/q).is_dir():selected.add(q);queue.append(q)
# Copy the essential final sample readbacks out of transient host jobs before moving them.
evidence=ROOT/'docs/归档前关键证据_v164';evidence.mkdir(parents=True,exist_ok=True)
folder=ROOT/'阶段2/示意/原生轴号与可拉伸轴线_20261102';manifest=json.loads((folder/'manifest-v164.json').read_text('utf-8'));qa=json.loads((folder/'qa-v164.json').read_text('utf-8'))
items={'用户修改图原生几何.json':Path(manifest['sourcePreview']),'用户修改图轴网审计.json':Path(manifest['sourceAudit']),'最终样张原生几何.json':Path(qa['captures']['final']['job'])/'stage2-preview-native.json'}
items.update({Path(e['nativeReport']).name:Path(e['nativeReport']) for a in manifest['axes'] for e in a.get('leadEdits',[])})
for name,p in items.items():shutil.copyfile(p,evidence/name)
# Preserve the user's modified drawing as a portable baseline in this repository.
baseline=ROOT/'阶段2/inputs/用户修改二层基线_20261102.dwg';shutil.copyfile('D:/修改.dwg',baseline);assert sha(baseline)==manifest['sourceSHA256']
# Model/project Codex settings and relevant local skills; never copy account/session stores.
codex=ROOT/'.codex';(codex/'backups').mkdir(parents=True);global_config=Path('C:/Users/PC/.codex/config.toml');original=global_config.read_text('utf-8');parsed=tomllib.loads(original)
portable='''# 项目配置：保留本机模型和推理偏好；本机路径见 backups 中的示例。\n'''+''.join(f'{k} = {json.dumps(parsed[k],ensure_ascii=False)}\n' for k in ['model','model_reasoning_effort','service_tier'] if k in parsed)+'\n[windows]\nsandbox = "elevated"\n'
(codex/'config.toml').write_text(portable,encoding='utf-8')
example=original.replace(next((line for line in original.splitlines() if line.startswith('SKY_CUA_NATIVE_PIPE_DIRECTORY =')),''),'# SKY_CUA_NATIVE_PIPE_DIRECTORY 为每次运行生成，未归档。')
example=re.sub(r"(?ms)^\[projects\..*?(?=^\[|\Z)",lambda m:m.group(0) if '建筑施工图自动化' in m.group(0) else '',example)
(codex/'backups/config.user.toml.example').write_text(example,encoding='utf-8');tomllib.loads(example)
skills=['autocad2020-objectarx','cpp-coding-standards','cpp-testing','review-agent','construction-drawing-review']
for name in skills:
 src=Path('C:/Users/PC/.codex/skills')/name
 if src.exists():shutil.copytree(src,codex/'skills'/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc','.git'))
(codex/'README.md').write_text('''# Codex 配置\n\nAGENTS.md、.superpowers/ 和 .codex/skills/ 保留本项目规则及相关本地技能。config.toml 保留当前模型、推理强度及 Windows 偏好。backups/config.user.toml.example 保留本机配置作为迁移参考，移除了会话专用管道和其他项目的信任项；本机路径须按新机器调整，不要直接覆盖用户配置。\n\n没有复制 auth.json、令牌、聊天数据库、会话历史、浏览器状态、安装缓存或全局授权规则。原 C:/Users/PC/.codex 未修改。\n''',encoding='utf-8')
(codex/'preservation.json').write_text(json.dumps({'source':str(global_config),'sourceSHA256':sha(global_config),'sourceModified':False,'copiedSkills':skills,'removedFromExample':['session-specific native pipe','unrelated project trust entries'],'credentialsCopied':False},ensure_ascii=False,indent=2),encoding='utf-8')
# Directory-level moves are atomic on this D: volume. Exact resolved paths checked again by PowerShell.
move_dirs=['阶段1/runs','阶段2/runs','插件/build','阶段2/build','阶段1/build','立面插件/build','立面插件/阶段1/build','阶段1/原生接口探测/build','阶段1/原生接口探测/help-extracted','阶段2/tools/geometry_packages','插件/installer/build']
transient_names={'__pycache__','.pytest_cache','.mypy_cache','.ruff_cache','.vs','obj'}
for base,dirs,files in os.walk(ROOT):
 current=Path(base)
 if any(current==ROOT/p or ROOT/p in current.parents for p in move_dirs):dirs[:]=[];continue
 for n in list(dirs):
  if n in transient_names or n in {'build','obj'}:
   move_dirs.append((current/n).relative_to(ROOT).as_posix());dirs.remove(n)
# Individual generated files outside archived dirs; preserve native arx/exe and all evidence source files.
transient_ext={'.obj','.pdb','.ilk','.exp','.lib','.idb','.ipch','.tlog','.lastbuildstate','.pyc','.bak','.dwl','.dwl2'}
files=[]
for base,dirs,names in os.walk(ROOT):
 current=Path(base)
 if any(current==ROOT/p or ROOT/p in current.parents for p in move_dirs):dirs[:]=[];continue
 for name in names:
  p=current/name
  if p.suffix.lower() in transient_ext or (p.suffix.lower()=='.log' and p.parent in [ROOT/'插件',ROOT/'阶段1']):files.append(p.relative_to(ROOT).as_posix())
# Large raw REA extraction is repeatable and unnecessary for source control; retain research summary/index.
files += [p.relative_to(ROOT).as_posix() for p in (ROOT/'调研/第三维度一键平面重绘_REA_20261029').glob('rea-*-evidence.json')]
entries=[]
for rel in move_dirs+files:
 p=ROOT/rel
 if not p.exists():continue
 fs=[f for f in p.rglob('*') if f.is_file()] if p.is_dir() else [p]
 entries.append({'relative':rel,'kind':'directory' if p.is_dir() else 'file','files':len(fs),'bytes':sum(f.stat().st_size for f in fs)})
plan={'workspace':str(ROOT),'archive':str(ARCHIVE),'entries':entries,'keepElevationBuilds':sorted(selected),'protected':protected,'created':datetime.datetime.now().astimezone().isoformat()}
(ARCHIVE/'cleanup-plan.json').write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding='utf-8');(ROOT/'docs/cleanup-plan-pointer.json').write_text(json.dumps({'archive':str(ARCHIVE),'plan':str(ARCHIVE/'cleanup-plan.json')},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'archive':str(ARCHIVE),'moveEntries':len(entries),'files':sum(x['files'] for x in entries),'bytes':sum(x['bytes'] for x in entries),'keepFrozenBuilds':len(selected)},ensure_ascii=True))
