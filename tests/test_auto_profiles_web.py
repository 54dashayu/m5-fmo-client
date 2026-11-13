"""Exercise actual checkbox/all/none/save handlers, including polling during edits."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text();js=s[s.index('let autoDraft='):s.index('async function loadRoots()')]
pre=r'''const assert=require('node:assert/strict');
class El{constructor(){this.children=[];this.dataset={};this.disabled=false;this.checked=false;this.textContent=''}append(...v){this.children.push(...v)}replaceChildren(...v){this.children=v}querySelectorAll(){return this.children.flatMap(v=>v.children).filter(v=>v.type==='checkbox')}}
const els={};function $(id){return els[id]??=(new El())}const document={createElement:()=>new El()};let calls=[],reject=false;
const api=async(...args)=>{calls.push(args);if(reject)throw Error('certificate unavailable');return {message:'saved'}};const msg=()=>{};
let profiles=[{id:1,name:'BH',autoAvailable:true,autoEnabled:true},{id:2,name:'TES',autoAvailable:true,autoEnabled:false},{id:3,name:'Root only',autoAvailable:false,autoEnabled:false,autoReason:'no certificate'}];
async function loadRoots(){renderAutoRoots(profiles)}
'''
tail=r'''
(async()=>{
 renderAutoRoots(profiles);let inputs=$('autoRootList').querySelectorAll();assert(inputs[0].checked&&!inputs[1].checked&&inputs[2].disabled);assert($('autoSave').disabled);
 inputs[1].checked=true;inputs[1].onchange();renderAutoRoots(profiles);assert($('autoRootList').querySelectorAll()[1].checked,'polling must preserve edits');
 $('autoAll').onclick();assert.deepEqual([...autoDraft],[1,2]);await $('autoSave').onclick();assert.equal(calls[0][0],'/api/roots');assert.deepEqual(calls[0][2],{action:'auto',profiles:[1,2]});assert(!autoDirty);
 $('autoNone').onclick();assert.equal(autoDraft.size,0);await $('autoSave').onclick();assert.deepEqual(calls[1][2].profiles,[]);
 $('autoAll').onclick();reject=true;await $('autoSave').onclick();assert(autoDirty&&!$('autoSave').disabled);assert.match($('autoRootStatus').textContent,/unavailable/);
 console.log('PASS actual per-profile checkboxes, select eligible all, select none, save, polling draft preservation and save-error retry');
})().catch(e=>{console.error(e);process.exit(1)});
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.js';p.write_text(pre+js+tail);subprocess.run(['node',str(p)],check=True)
