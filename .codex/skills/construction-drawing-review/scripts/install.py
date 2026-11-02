from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
wheels=list((root/'wheels').glob('villa_drawing_review-*.whl'))
if len(wheels)!=1:raise SystemExit('需要一个随技能发布的引擎wheel；开发环境请安装仓库review-engine。')
subprocess.run([sys.executable,'-m','pip','install',str(wheels[0])],check=True)
