import assert from 'node:assert/strict';
import fs from 'node:fs';
import vm from 'node:vm';
import {webcrypto,createHash} from 'node:crypto';
import {resolveMd5} from '../web-flasher/flash-hash.mjs';
const realHash=data=>createHash('md5').update(data).digest('hex');
for(const module of [{default:realHash},{md5:realHash}]) assert.equal(resolveMd5(module)(new Uint8Array([0,128,255])),realHash(new Uint8Array([0,128,255])));
for(const module of [{},{default:()=>''}]) assert.throws(()=>resolveMd5(module),/校验组件加载失败/);
import {BOARDS,LAYOUTS,partitionPlan,validateManifest,validPartitions,selectedParts} from '../web-flasher/board-policy.mjs';
const source=fs.readFileSync(new URL('../web-flasher/app.js',import.meta.url),'utf8')
 .replace(/^import .*;\n/gm,'')
 .replace(/let flashToolsPromise=null;\nconst loadFlashTools=[\s\S]*?\n\}\)\);\n/,'const loadFlashTools=async()=>({ESPLoader,Transport,md5});\n');
function table(id,legacy=false){const rows=legacy?LAYOUTS[id].slice(0,-1):LAYOUTS[id],buf=new Uint8Array(4096).fill(255),v=new DataView(buf.buffer);rows.forEach(([type,sub,off,size],i)=>{let p=i*32;v.setUint16(p,0x50aa,true);v.setUint8(p+2,type);v.setUint8(p+3,sub);v.setUint32(p+4,off,true);v.setUint32(p+8,size,true);v.setUint32(p+28,0,true)});return buf;}
async function scenario(id,{install=false,mismatch=false,incompatible=false,cancel=false,migrate=false,smallFlash=false}={}){
 const els={},input={value:install?'install':'upgrade'},writes=[];let signals=[],manifest,files={},selectedChip=BOARDS[id].chip;
 function el(id){return els[id]??={value:id==='boardSelect'?'m5-fire':'',checked:false,textContent:'',hidden:false,disabled:false,listeners:{},addEventListener(n,f){this.listeners[n]=f},removeAttribute(n){delete this[n]},focus(){}}}
 async function fixtures(board){const b=BOARDS[board],base=board+'/v0.7.30/';manifest={version:'0.7.30',board,chip:b.chip,flashSize:b.flashSize,download:base+'fmo-'+board+'-v0.7.30.zip',parts:[]};files={};for(let i=0;i<b.addresses.length;i++){const buf=b.addresses[i]===0x8000?table(board):new Uint8Array([i,2,3]);const path=base+b.names[i];files[path]=buf;manifest.parts.push({address:b.addresses[i],path,sha256:Buffer.from(await webcrypto.subtle.digest('SHA-256',buf)).toString('hex')})}}
 await fixtures('m5-fire');
 class Transport{async setDTR(v){signals.push(['D',v])}async setRTS(v){signals.push(['R',v])}async disconnect(){}}
 class ESPLoader{constructor(options){assert.equal(options.serialOptions.bufferSize,65536);this.chip={CHIP_NAME:mismatch?'ESP32-C3':selectedChip}}async main(){}async detectFlashSize(){return smallFlash?'2MB':BOARDS[id].flashSize}async readFlash(){const buf=table(id,migrate);if(incompatible)new DataView(buf.buffer).setUint32(8,1,true);return buf}async writeFlash(opts){writes.push(opts)}}
 const context=vm.createContext({BOARDS,LAYOUTS,partitionPlan,validateManifest,validPartitions,selectedParts,ESPLoader,Transport,readPartitionTable:loader=>loader.readFlash(),md5:()=>'',isSecureContext:true,navigator:{serial:{requestPort:async()=>({})}},document:{getElementById:el,querySelector:()=>input,querySelectorAll:()=>[el('hardware'),el('installConfirm'),el('boardSelect')]},window:{addEventListener(){}},crypto:webcrypto,AbortSignal,Uint8Array,DataView,setTimeout:(f)=>setTimeout(f,0),fetch:async url=>url.includes('manifest.json')?{ok:true,json:async()=>manifest}:{ok:true,arrayBuffer:async()=>files[url.replace('firmware/','')].buffer}});
 vm.runInContext(source,context);await new Promise(r=>setTimeout(r,5));
 await fixtures(id);el('boardSelect').value=id;el('hardware').checked=true;await el('boardSelect').listeners.change();await new Promise(r=>setTimeout(r,5));assert.equal(el('hardware').checked,false);el('hardware').checked=true;el('installConfirm').checked=install;
 await el('flash').listeners.click();assert.equal(writes.length,0,'preparation must not write');
 if(mismatch||incompatible||smallFlash||(migrate&&id==='esp32-2432s028r-plus')){assert(el('review').hidden);assert.match(el('status').textContent,/未写入/);return;}
 assert.equal(el('review').hidden,false);assert.equal(el('boardSelect').disabled,true);
 if(cancel){await el('cancelFlash').listeners.click();assert.equal(writes.length,0);return;}
 await el('confirmFlash').listeners.click();assert.equal(writes.length,1);assert.equal(writes[0].eraseAll,false);assert.equal(writes[0].flashSize,BOARDS[id].flashSize);assert.deepEqual(Array.from(writes[0].fileArray,p=>p.address),install?BOARDS[id].addresses:migrate?(id==='m5-fire'?[BOARDS[id].app,0x1000,0x8000]:[BOARDS[id].app,0x8000]):[BOARDS[id].app]);assert(signals.some(([k,v])=>k==='R'&&v===true));assert.match(el('status').textContent,/成功/);
}
for(const id of Object.keys(BOARDS)){for(const options of [{},{install:true},{cancel:true},{mismatch:true},{incompatible:true},{migrate:true},{smallFlash:true}])await scenario(id,options)}
console.log('PASS real flasher flow: both boards, app-only upgrade, initial layout, pre-confirm no writes, cancellation, chip/partition refusal and reset pulse');

// Exercise the actual read protocol, including the trailing digest and failures.
const {readPartitionTable}=await import('../web-flasher/partition-reader.mjs');
for(const failure of [null,'digest','short','timeout','not-stub']) {
 const bytes=table('xingzhi-cube-1.54tft-wifi'),acks=[],progress=[];
 let packet=0,commands=0;
 const loader={IS_STUB:failure!=='not-stub',ESP_READ_FLASH:0xd2,
  async checkCommand(label,op,payload,checksum,responseLength,timeout){
   commands++;assert.equal(op,0xd2);assert.equal(timeout,5000);
   assert.deepEqual(Array.from(new Uint32Array(payload.buffer)),[0x8000,4096,1024,1]);return 0;
  },transport:{async read(timeout){assert.equal(timeout,5000);if(failure==='timeout')throw Error('read timeout');if(failure==='short')return new Uint8Array(1);if(packet<4)return bytes.slice(packet++*1024,packet*1024);packet++;return new Uint8Array(Buffer.from(failure==='digest'?'00'.repeat(16):realHash(bytes),'hex'));},async write(ack){acks.push(new DataView(ack.buffer).getUint32(0,true));}}};
 const read=()=>readPartitionTable(loader,realHash,(n)=>progress.push(n));
 if(failure)await assert.rejects(read);else {assert.deepEqual(await read(),bytes);assert.deepEqual(acks,[1024,2048,3072,4096]);assert.deepEqual(progress,[0,1024,2048,3072,4096]);assert.equal(packet,5);}
 if(failure==='not-stub')assert.equal(commands,0);
}
console.log('PASS bounded partition read: packet ACKs, digest, progress, corruption and timeout refusal');
