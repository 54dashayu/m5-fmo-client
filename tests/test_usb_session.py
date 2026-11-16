from pathlib import Path
import subprocess,tempfile,sys,time
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'));from usb_admin import Link,LinkBusy
s=(r/'ui-preview-firmware/main/web.html').read_text();api=s[s.index('async function api('):s.index('function lock()')]
pre="""const assert=require('assert');let usbTransport=true,token='old',locked=false;const lock=()=>locked=true;let requests=[];
let mode='usb';async function fetch(path,args){requests.push({path,args});if(path==='/api/transport')return {ok:true,json:async()=>({usb:true,token:'new'})};if(mode==='wifi'||args.headers['X-Device-Token']==='old')return {status:401};return {ok:true,status:200,json:async()=>({ok:true})};}
"""
tail="""(async()=>{assert((await api('/api/status')).ok);assert.equal(token,'new');assert(!locked);assert.equal(requests.length,3);usbTransport=false;mode='wifi';await assert.rejects(()=>api('/api/status'));assert(locked);console.log('PASS USB renews local token after service restart; Wi-Fi still requires login');})().catch(e=>{console.error(e);process.exit(1)});"""
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'t.js';p.write_text(pre+api+tail);subprocess.run(['node',str(p)],check=True)
l=Link('unused');l.lock.acquire();t=time.monotonic()
try:
 try:l.call('status');raise AssertionError('queued indefinitely')
 except LinkBusy:pass
 assert time.monotonic()-t<1
finally:l.lock.release()
l._call=lambda op,data: {'ok':True};assert l.call('status')['ok'];assert l.lock.acquire(False);l.lock.release()
print('PASS USB contention returns promptly and releases lock after requests')
