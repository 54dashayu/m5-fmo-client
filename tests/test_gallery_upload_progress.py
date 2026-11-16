"""Real upload controller: byte progress, save acknowledgement, uncertain errors."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text()
assert 'sstvStop' not in s
fn=s[s.index('function uploadGalleryImage('):s.index("$('galleryImport').onclick=")]
js='''const assert=require('assert');let req;const token='test';
class XMLHttpRequest{constructor(){req=this;this.upload={};}open(){}setRequestHeader(){}send(bytes){this.bytes=bytes;}}
const node={};const $=()=>node;let calls=0;let loadGallery=async()=>++calls===2;const setTimeout=f=>f();
'''+fn+'''
(async()=>{
 let progress=[],saving=false,settled=false;
 const p=uploadGalleryImage(new Uint8Array(153600),n=>progress.push(n),()=>saving=true);p.then(()=>settled=true);
 req.upload.onprogress({lengthComputable:true,loaded:38400,total:153600});assert.deepEqual(progress,[25]);
 req.upload.onload();await Promise.resolve();assert(saving&&!settled);
 req.status=200;req.responseText='{"ok":true,"id":4}';req.onload();assert.equal((await p).id,4);
 for(const failure of ['ontimeout','onerror','onabort']){const bad=uploadGalleryImage([],()=>{},()=>{});req[failure]();await assert.rejects(bad,/尚未确认/);}
 const bad=uploadGalleryImage([],()=>{},()=>{});req.status=500;req.responseText='{"ok":false,"error":"写入失败"}';req.onload();await assert.rejects(bad,/写入失败/);
 await refreshUploadedImage({id:4});assert.equal(calls,2);assert(node.textContent.includes('自动更新'));
 calls=0;loadGallery=async()=>{calls++;return false};await refreshUploadedImage({id:4});assert.equal(calls,3);assert(node.textContent.includes('不必重复上传'));
 console.log('PASS upload progress/save acknowledgement/errors/refresh retry without repeat upload');
})().catch(e=>{console.error(e);process.exit(1)});
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'upload.cjs';p.write_text(js);subprocess.run(['node',str(p)],check=True)
