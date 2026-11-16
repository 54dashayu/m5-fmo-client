"""Non-TF boards never query or expose card management; M5 keeps it."""
from pathlib import Path
import subprocess,tempfile,json
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/web.html').read_text()
fn=s[s.index('const tfSupported='):s.index('let storageWorking=false;')]
js='''const vm=require('vm'),assert=require('assert');
(async()=>{for(const enabled of [false,true]){let requests=[];const els={};const ctx={api:async path=>{requests.push(path);return path.endsWith('/status')?{active:false}:{selected:'flash',sdSupported:true,sdReady:true}},$:id=>els[id]??={querySelector:()=>({})},resourceBytes:()=>'',storageWorking:false};vm.createContext(ctx);vm.runInContext(SOURCE.replace('@FMO_WEB_HAS_TF@',enabled?'1':'0'),ctx);await ctx.storageInfo();assert.deepEqual(requests,enabled?['/api/gallery/card/status','/api/gallery/storage']:[]);}console.log('PASS per-board gallery: no TF requests on Xiaozhi, card status retained on M5');})().catch(e=>{console.error(e);process.exit(1)});'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.cjs';p.write_text(js.replace('SOURCE',json.dumps(fn)));subprocess.run(['node',str(p)],check=True)
assert 'id="galleryMedium"' not in s
assert 'id="galleryMedium"' in (r/'ui-preview-firmware/main/web_gallery_tf.html').read_text()
