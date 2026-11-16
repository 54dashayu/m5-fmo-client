"""Real browser controller with simulated device stages; no physical formatting."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
html=(r/'ui-preview-firmware/main/web.html').read_text()
code=html[html.index('let storageWorking=false;'):html.index('if(tfSupported){')]
js=r'''
const vm=require('vm'),assert=require('assert');
const source=SOURCE;
(async()=>{for(const mode of ['success','failure','reboot','resume']){
const els={},seen=[],requests=[];const $=id=>els[id]??={disabled:false,hidden:true,textContent:'',set value(v){seen.push(v)},set max(v){}};
let stages=[{jobId:7,active:true,completedStages:2,format:true},{jobId:mode==='reboot'?0:7,active:false,success:mode!=='failure',error:'write failed',completedStages:mode==='failure'?0:4}];
const start={jobId:7,active:true,completedStages:0,format:true};
const ctx={tfSupported:true,$,setTimeout:f=>{queueMicrotask(f)},galleryBefore:1,gallerySelected:new Set(),loadGallery:async()=>{},storageInfo:async()=>{},api:async(path,method)=>{requests.push([path,method]);return method==='POST'?start:stages.shift()}};
vm.createContext(ctx);vm.runInContext(source,ctx);
await ctx.storageAction(mode==='resume'?null:'/api/gallery/card/format',{},mode==='resume'?start:undefined);
assert(seen.includes(2));assert.equal($('galleryCardFormat').disabled,false);
assert.match($('galleryStorageMessage').textContent,mode==='failure'||mode==='reboot'?/未能确认完成/:/通过容量校验/);
if(mode==='resume')assert(!requests.some(r=>r[1]==='POST'));
}console.log('PASS actual stage controller: success, failure, reboot detection and resume without repeated formatting');})();
'''
import json
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.cjs';p.write_text(js.replace('SOURCE',json.dumps(code)))
 subprocess.run(['node',str(p)],check=True)
