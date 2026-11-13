"""Validate actual board policy against wrong chips, addresses and partitions."""
from pathlib import Path
import subprocess
p=Path(__file__).resolve().parents[1]/'web-flasher/board-policy.mjs'
code='''import assert from 'node:assert/strict';
import {BOARDS,LAYOUTS,partitionPlan,validateManifest,validPartitions,selectedParts} from MODULE;
for(const [id,b] of Object.entries(BOARDS)){
 const version='0.7.30',base=id+'/v'+version+'/';
 const m={version,board:id,chip:b.chip,flashSize:b.flashSize,download:base+'fmo-'+id+'-v'+version+'.zip',parts:b.addresses.map((address,i)=>({address,path:base+b.names[i],sha256:'a'.repeat(64)}))};
 assert.equal(validateManifest(m,id),m);assert.deepEqual(selectedParts(m,false).map(p=>p.address),[b.app]);assert.equal(selectedParts(m,true).length,b.addresses.length);
 for(const patch of [{chip:'wrong'},{board:'wrong'},{flashSize:'2MB'},{download:'../other.zip'},{version:'../bad'}])assert.throws(()=>validateManifest({...m,...patch},id));
 for(const key of ['address','path','sha256']){const bad=structuredClone(m);bad.parts[0][key]=key==='address'?4097:'bad';assert.throws(()=>validateManifest(bad,id));}
 const bytes=new Uint8Array(4096).fill(255),v=new DataView(bytes.buffer);for(const [i,type,sub,offset,size] of LAYOUTS[id].map((row,i)=>[i,...row])){const p=i*32;v.setUint16(p,0x50aa,true);v.setUint8(p+2,type);v.setUint8(p+3,sub);v.setUint32(p+4,offset,true);v.setUint32(p+8,size,true);v.setUint32(p+28,0,true);}
 assert(validPartitions(bytes,id));assert.equal(partitionPlan(bytes,id),"current");const old=bytes.slice();old.fill(255,(LAYOUTS[id].length-1)*32);assert.equal(partitionPlan(old,id),"migrate");const bad=old.slice();new DataView(bad.buffer).setUint32(32+4,0x22000,true);assert.equal(partitionPlan(bad,id),null);assert(!validPartitions(bytes,id==='m5-fire'?'xingzhi-cube-1.54tft-wifi':'m5-fire'));v.setUint32(8,b.nvsSize+4096,true);assert(!validPartitions(bytes,id));
}
console.log('PASS board manifests: chip/path/hash/address isolation, app-only upgrade and partition mismatch refusal');
'''.replace('MODULE',repr(p.as_uri()))
subprocess.run(['node','--input-type=module','-e',code],check=True)
