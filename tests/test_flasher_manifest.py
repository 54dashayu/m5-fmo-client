"""Validate actual board policy against wrong chips, addresses and partitions."""
from pathlib import Path
import subprocess
p=Path(__file__).resolve().parents[1]/'web-flasher/board-policy.mjs'
code='''import assert from 'node:assert/strict';
import {BOARDS,validateManifest,validPartitions,selectedParts} from MODULE;
for(const [id,b] of Object.entries(BOARDS)){
 const version='0.7.30',base=id+'/v'+version+'/';
 const m={version,board:id,chip:b.chip,flashSize:b.flashSize,download:base+'fmo-'+id+'-v'+version+'.zip',parts:b.addresses.map((address,i)=>({address,path:base+b.names[i],sha256:'a'.repeat(64)}))};
 assert.equal(validateManifest(m,id),m);assert.deepEqual(selectedParts(m,false).map(p=>p.address),[b.app]);assert.equal(selectedParts(m,true).length,b.addresses.length);
 for(const patch of [{chip:'wrong'},{board:'wrong'},{flashSize:'2MB'},{download:'../other.zip'},{version:'../bad'}])assert.throws(()=>validateManifest({...m,...patch},id));
 for(const key of ['address','path','sha256']){const bad=structuredClone(m);bad.parts[0][key]=key==='address'?4097:'bad';assert.throws(()=>validateManifest(bad,id));}
 const bytes=new Uint8Array(4096).fill(255),v=new DataView(bytes.buffer);for(const [i,type,sub,offset,size] of [[0,1,2,0x9000,b.nvsSize],[1,0,0,b.app,b.appSize]]){const p=i*32;v.setUint16(p,0x50aa,true);v.setUint8(p+2,type);v.setUint8(p+3,sub);v.setUint32(p+4,offset,true);v.setUint32(p+8,size,true);}
 assert(validPartitions(bytes,id));assert(!validPartitions(bytes,id==='m5-fire'?'xingzhi-cube-1.54tft-wifi':'m5-fire'));v.setUint32(8,b.nvsSize+4096,true);assert(!validPartitions(bytes,id));
}
console.log('PASS board manifests: chip/path/hash/address isolation, app-only upgrade and partition mismatch refusal');
'''.replace('MODULE',repr(p.as_uri()))
subprocess.run(['node','--input-type=module','-e',code],check=True)
