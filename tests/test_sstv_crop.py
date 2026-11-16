"""Check crop geometry across aspect ratios, zoom and drag bounds."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text();fn=s[s.index('function cropGeometry('):s.index('function cropSstvImage(')]
js=fn+'''
const assert=require('assert');
for(const [w,h] of [[1200,800],[800,1200],[400,300],[1,1],[10000,1],[1,10000]]){
 const f=cropGeometry(w,h,'fit',1,0,0);assert(f.w<=320.00001&&f.h<=240.00001);assert(Math.abs(f.x*2+f.w-320)<.001&&Math.abs(f.y*2+f.h-240)<.001);
 for(const z of [1,1.5,4])for(const x of [-1e6,0,1e6])for(const y of [-1e6,0,1e6]){
  const c=cropGeometry(w,h,'crop',z,x,y);assert(c.w>=319.9999&&c.h>=239.9999);assert(c.x<=.0001&&c.y<=.0001&&c.x+c.w>=319.9999&&c.y+c.h>=239.9999);assert(Math.abs(c.w/c.h-w/h)<.001);
 }
}
console.log('PASS crop geometry: portrait/landscape/tiny/extreme images, aspect ratio, fit padding, zoom and drag bounds');
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'crop.cjs';p.write_text(js);subprocess.run(['node',str(p)],check=True)
