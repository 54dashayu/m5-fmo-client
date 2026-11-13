"""Execute actual add handlers: saving a candidate must not select/connect."""
from pathlib import Path
import subprocess,tempfile,json
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text()
local=s[s.index('function renderLocalDiscovery()'):s.index('let fullCatalog=')]
full=s[s.index('function mutualForCurrent('):s.index('async function loadFullCatalog(')]
js='''const assert=require('node:assert/strict');
class El{constructor(){this.children=[];this.value='';this.disabled=false;}append(...x){this.children.push(...x)}replaceChildren(...x){this.children=x}get childElementCount(){return this.children.length}}
const elements={};function $(id){return elements[id]??=(new El())}
const document={createElement:()=>new El()};const Option=function(text,value){this.text=text;this.value=value};
let calls=[];const api=async(...args)=>calls.push(args);const loadRelays=async()=>{};const loadDiscovery=async()=>{};const msg=()=>{};const confirm=()=>true;
const activeRootKey='A'.repeat(43);let savedRelays=[],discoveryRevision=9,discoverPage=0,catalogConnected=true,catalogUpdated=Date.now()/1000,catalogError='',catalogLoading=false;
function relayKey(v){return [v.uid,v.host,v.port,v.fingerprint].join('|')}
let discovered=[{uid:1845,name:'node',callsign:'BI4TES',state:'pending',addIndex:3,selectIndex:7}];
let fullCatalog=[{uid:1845,name:'node',callsign:'BI4TES',host:'example.net',port:1883,fingerprint:'A'.repeat(43),lastSeen:Date.now()/1000,sourceVerified:true,addable:true,acceptedClientRoots:['A'.repeat(43)],adminConfirmed:true,mutualTrustConfirmed:true,confirmedUntil:Date.now()/1000+500}];
$('discoverScope').value='all';$('discoverFilter').value='all';$('discoverPageSize').value='10';
'''+local+full+'''
(async()=>{
 assert(mutualForCurrent(fullCatalog[0]));assert(!mutualForCurrent({...fullCatalog[0],acceptedClientRoots:['B'.repeat(43)]}));
 renderLocalDiscovery();let row=$('discoverList').children[0];let b=row.children[1].children[0];assert.equal(b.textContent,'添加');await b.onclick();assert.equal(calls[0][0],'/api/discovery/add');assert.deepEqual(calls[0][2],{index:3,revision:9});
 calls=[];renderDiscovery();row=$('discoverList').children[0];b=row.children[1];assert.equal(b.textContent,'添加');await b.onclick();assert.equal(calls[0][0],'/api/relays');assert.equal(calls[0][2].uid,1845);assert(!('connect' in calls[0][2]));assert.equal(calls.length,1);
 console.log('PASS real browser handlers add without connecting, use add index, and scope mutual trust by active root');
})().catch(e=>{console.error(e);process.exit(1)});
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cjs';p.write_text(js);subprocess.run(['node',str(p)],check=True)
