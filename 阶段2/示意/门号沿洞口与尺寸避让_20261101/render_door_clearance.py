"""Internal native-print visual checks; PDFs are not user deliverables."""
from pathlib import Path
import json,sys,subprocess,concurrent.futures
from PIL import Image
HERE=Path(__file__).resolve().parent
version=sys.argv[1]
qa=json.loads((HERE/f'door-clearance-qa-{version}.json').read_text(encoding='utf-8'))
poppler=Path('C:/Users/PC/.cache/codex-runtimes/codex-primary-runtime/dependencies/native/poppler/Library/bin/pdftoppm.exe')
def render(f):
 n=f['floor'];prefix=HERE/f'native-{version}-floor-{n}'
 subprocess.run([str(poppler),'-f','1','-singlefile','-r','300','-png',f['PDF'],str(prefix)],check=True,creationflags=subprocess.CREATE_NO_WINDOW)
 p=json.loads((Path(f['previewJob'])/'stage2-preview-native.json').read_text(encoding='utf-8'));w=[e for e in p['originalBefore'] if e['type']=='TDbWall'];b=[min(e['min'][i] for e in w)-1600 for i in [0,1]]+[max(e['max'][i] for e in w)+1600 for i in [0,1]];s=p['page']['validatedSettings'];x0,y0,_,_=s['windowDCS'];ox,oy=s['originMM'];fac=300/25.4;xx0,yy0,xx1,yy1=b
 box=[round((ox+(xx0-x0)/100)*fac),round((420-oy-(yy1-y0)/100)*fac),round((ox+(xx1-x0)/100)*fac),round((420-oy-(yy0-y0)/100)*fac)];im=Image.open(prefix.with_suffix('.png'));out=HERE/f'building-{version}-floor-{n}.png';im.crop(box).save(out);print(str(out),flush=True)
with concurrent.futures.ThreadPoolExecutor(2) as pool:list(pool.map(render,qa['floors']))
