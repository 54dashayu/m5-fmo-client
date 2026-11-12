import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {webcrypto} from 'node:crypto';
import {BOARDS,validateManifest,validPartitions,selectedParts} from '../web-flasher/board-policy.mjs';
const source=fs.readFileSync(new URL('../web-flasher/app.js',import.meta.url),'utf8').replace(/^import .*;\n/gm,'');
async function scenario(id,{install=false,mismatch=false,incompatible=false,cancel=false}={}){
 const els={},input={value:install?'install':'upgrade'},writes=[];let signals=[],manifest,files={},selectedChip=BOARDS[id].chip;
 function el(id){return els[id]??={value:id==='boardSelect'?'m5-fire':'',checked:false,textContent:'',hidden:false,disabled:false,listeners:{},addEventListener(n,f){this.listeners[n]=f},removeAttribute(n){delete this[n]},focus(){}}}
 async function fixtures(board){const b=BOARDS[board],base=board+'/v0.7.30/';manifest={version:'0.7.30',board,chip:b.chip,flashSize:b.flashSize,download:base+'fmo-'+board+'-v0.7.30.zip',parts:[]};files={};for(let i=0;i<b.addresses.length;i++){const buf=new Uint8Array([i,2,3]);const path=base+b.names[i];files[path]=buf;manifest.parts.push({address:b.addresses[i],path,sha256:Buffer.from(await webcrypto.subtle.digest('SHA-256',buf)).toString('hex')})}}
 await fixtures('m5-fire');
 class Transport{async setDTR(v){signals.push(['D',v])}async setRTS(v){signals.push(['R',v])}async disconnect(){}}
 class ESPLoader{constructor(){this.chip={CHIP_NAME:mismatch?'ESP32-C3':selectedChip}}async main(){}async readFlash(){const b=BOARDS[id],buf=new Uint8Array(4096).fill(255),v=new DataView(buf.buffer);for(const [i,type,sub,off,size] of [[0,1,2,0x9000,b.nvsSize],[1,0,0,b.app,b.appSize]]){const p=i*32;v.setUint16(p,0x50aa,true);v.setUint8(p+2,type);v.setUint8(p+3,sub);v.setUint32(p+4,off,true);v.setUint32(p+8,incompatible?1:size,true)}return buf}async writeFlash(opts){writes.push(opts)}}
 const context=vm.createContext({BOARDS,validateManifest,validPartitions,selectedParts,ESPLoader,Transport,md5:()=>'',isSecureContext:true,navigator:{serial:{requestPort:async()=>({})}},document:{getElementById:el,querySelector:()=>input,querySelectorAll:()=>[el('hardware'),el('installConfirm'),el('boardSelect')]},window:{addEventListener(){}},crypto:webcrypto,AbortSignal,Uint8Array,DataView,setTimeout:(f)=>setTimeout(f,0),fetch:async url=>url.includes('manifest.json')?{ok:true,json:async()=>manifest}:{ok:true,arrayBuffer:async()=>files[url.replace('firmware/','')].buffer}});
 vm.runInContext(source,context);await new Promise(r=>setTimeout(r,5));
 await fixtures(id);el('boardSelect').value=id;el('hardware').checked=true;await el('boardSelect').listeners.change();await new Promise(r=>setTimeout(r,5));assert.equal(el('hardware').checked,false);el('hardware').checked=true;el('installConfirm').checked=install;
 await el('flash').listeners.click();assert.equal(writes.length,0,'preparation must not write');
 if(mismatch||incompatible){assert(el('review').hidden);assert.match(el('status').textContent,/未写入/);return;}
 assert.equal(el('review').hidden,false);assert.equal(el('boardSelect').disabled,true);
 if(cancel){await el('cancelFlash').listeners.click();assert.equal(writes.length,0);return;}
 await el('confirmFlash').listeners.click();assert.equal(writes.length,1);assert.equal(writes[0].eraseAll,false);assert.equal(writes[0].flashSize,BOARDS[id].flashSize);assert.deepEqual(Array.from(writes[0].fileArray,p=>p.address),install?BOARDS[id].addresses:[BOARDS[id].app]);assert(signals.some(([k,v])=>k==='R'&&v===true));assert.match(el('status').textContent,/成功/);
}
for(const id of Object.keys(BOARDS)){for(const options of [{},{install:true},{cancel:true},{mismatch:true},{incompatible:true}])await scenario(id,options)}
console.log('PASS real flasher flow: both boards, app-only upgrade, initial layout, pre-confirm no writes, cancellation, chip/partition refusal and reset pulse');
