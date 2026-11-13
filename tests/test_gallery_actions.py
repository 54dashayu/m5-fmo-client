"""Exercise real browser deletion logic with a simulated device, never real photos."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text();logic=s[s.index('let galleryEpoch='):s.index('const galleryMemory=')]
harness=r'''
const assert=require('node:assert/strict');
const elements=new Map();const $=id=>{if(!elements.has(id))elements.set(id,{});return elements.get(id)};
const document={querySelectorAll:()=>[]};let accepted=true,confirms=0,calls=[],remaining=[1,2,3],failId=0,hold;
function confirm(){confirms++;return accepted}
async function api(path,method,data){assert.equal(path,'/api/gallery/delete');assert.equal(method,'POST');calls.push(data.id);if(hold)await hold;if(data.id===failId)throw Error('通联中');remaining=remaining.filter(id=>id!==data.id);return {ok:true}}
async function loadGallery(){galleryIds=[...remaining];for(const id of gallerySelected)if(!galleryIds.includes(id))gallerySelected.delete(id)}
'''
tests=r'''
(async()=>{
 galleryIds=[1,2,3];gallerySelectionUpdate();assert.equal($('galleryDeleteSelected').disabled,true);
 $('gallerySelectAll').onchange({target:{checked:true}});assert.equal(gallerySelected.size,3);assert.equal($('gallerySelectAll').checked,true);
 accepted=false;await deleteGalleryImages([1,2]);assert.deepEqual(calls,[]);assert.equal(gallerySelected.size,3);
 accepted=true;failId=2;await deleteGalleryImages([1,2,3]);assert.deepEqual(calls,[1,2]);assert.deepEqual([...gallerySelected],[2,3]);assert.match($('galleryStatus').textContent,/已确认删除 1 张/);assert.equal(galleryDeleting,false);
 failId=0;calls=[];await deleteGalleryImages([2,2,3,999]);assert.deepEqual(calls,[2,3]);assert.equal(gallerySelected.size,0);assert.equal($('galleryDeleteSelected').disabled,true);
 remaining=[4,5];galleryIds=[4,5];gallerySelected.add(4);let release;hold=new Promise(r=>release=r);calls=[];const first=deleteGalleryImages([4]);await deleteGalleryImages([5]);assert.deepEqual(calls,[4]);assert.equal($('galleryRefresh').disabled,true);release();await first;hold=null;assert.deepEqual(remaining,[5]);assert.equal($('galleryRefresh').disabled,false);
 console.log('PASS gallery deletion: selection, cancel, sequential failure, retry, invalid IDs, duplicate clicks');
})().catch(e=>{console.error(e);process.exitCode=1});
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.js';p.write_text(harness+logic+tests);subprocess.run(['node',str(p)],check=True)
