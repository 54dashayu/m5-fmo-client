import {ESPLoader, Transport} from 'esptool-js';
import {md5} from 'js-md5';
import {BOARDS,validateManifest,validPartitions,selectedParts,partitionPlan} from './board-policy.mjs';
const $=id=>document.getElementById(id);
let boardId='m5-fire';
let busy=false, session=null, currentManifest=null, refreshing=false;
const supported=isSecureContext && 'serial' in navigator;
$('support').textContent=supported?'先选择硬件、连接设备并检查固件，再点击确认开始刷写。连接检查会使设备进入刷写模式，但不会写入 Flash。':'当前浏览器不支持在线刷写。请用电脑 Chrome / Edge 打开此 HTTPS 页面，或下载固件包。';
function update(){const install=document.querySelector('input[name=mode]:checked').value==='install';$('installNote').hidden=$('confirmRow').hidden=!install;$('flash').disabled=busy||refreshing||!currentManifest||!!session||!supported||!$('hardware').checked||(install&&!$('installConfirm').checked);document.querySelectorAll('input,select').forEach(x=>x.disabled=busy||!!session||refreshing);$('confirmFlash').disabled=busy||!session?.ready;$('cancelFlash').disabled=busy;$('refreshVersion').disabled=busy||!!session||refreshing;}
document.querySelectorAll('input').forEach(x=>x.addEventListener('change',update));update();
const log=t=>{$('log').textContent=($('log').textContent+String(t)+'\n').slice(-12000);$('log').scrollTop=$('log').scrollHeight;};
const status=t=>{$('status').textContent=t;log(t);};
const hex=bytes=>Array.from(new Uint8Array(bytes),x=>x.toString(16).padStart(2,'0')).join('');
async function binary(part){const r=await fetch('firmware/'+part.path,{cache:'no-store',signal:AbortSignal.timeout(90000)});if(!r.ok)throw Error('固件下载失败：'+r.status);const b=new Uint8Array(await r.arrayBuffer());if(hex(await crypto.subtle.digest('SHA-256',b))!==part.sha256)throw Error('固件校验不符，请刷新页面重试');return b;}
async function refreshVersion(){
 refreshing=true;currentManifest=null;$('versionStatus').textContent='正在查询服务器版本…';$('downloadFirmware').removeAttribute('href');$('checksums').removeAttribute('href');update();
 try{
 const r=await fetch('firmware/'+boardId+'/manifest.json?t='+Date.now(),{cache:'no-store',signal:AbortSignal.timeout(15000)});
 if(!r.ok)throw Error('HTTP '+r.status);
 const manifest=await r.json();
 validateManifest(manifest,boardId);
 currentManifest=manifest;
 $('versionStatus').textContent='当前可刷版本：v'+manifest.version+(manifest.revision?' / '+manifest.revision:'')+(manifest.version.includes('-')?' · 测试版':' · 正式版');
 $('versionTime').textContent='查询时间：'+new Date().toLocaleString()+' · 应用 SHA256：'+manifest.parts.at(-1).sha256.slice(0,12)+'…';
 $('downloadFirmware').href='firmware/'+manifest.download;
 $('checksums').href='firmware/'+boardId+'/v'+manifest.version+'/SHA256SUMS';
 return manifest;
 }catch(e){$('versionStatus').textContent='版本查询失败：'+e.message;$('versionTime').textContent='请点击“刷新版本”重试；查询成功后才能连接刷写。';throw e;}
 finally{refreshing=false;update();}
}
$('refreshVersion').addEventListener('click',()=>refreshVersion().catch(()=>{}));
$('copyLog').addEventListener('click',async()=>{try{await navigator.clipboard.writeText('网页版本：20261113.6\n'+$('versionStatus').textContent+'\n'+$('log').textContent);$('copyLog').textContent='已复制日志';}catch{$('copyLog').textContent='复制失败，请选择日志文字复制';}});
function changeBoard(){boardId=$('boardSelect').value;const b=BOARDS[boardId];$('hardware').checked=false;$('installConfirm').checked=false;$('hardwareDetails').textContent=b.hardware;$('selectedPhoto').src=b.photo;$('selectedPhoto').alt=b.name+' 实机照片';$('boardIntro').textContent=b.name;$('status').textContent='等待选择设备';refreshVersion().catch(()=>{});}
$('boardSelect').addEventListener('change',changeBoard);changeBoard();
// Original ESP32 + USB UART: keep GPIO0 released and pulse EN through RTS.
// esptool-js 0.7.1 HardReset only deasserts RTS, leaving the stub running.
async function restartDevice(transport,loader){
 await transport.setDTR(false);
 await transport.setRTS(true);
 await new Promise(resolve=>setTimeout(resolve,loader?.chip?.CHIP_NAME==='ESP32-S3'?200:120));
 await transport.setRTS(false);
 await new Promise(resolve=>setTimeout(resolve,200));
}
async function release(reset=false){
 const old=session;session=null;$('review').hidden=true;
 if(old){if(reset&&old.loader?.chip)try{await restartDevice(old.transport,old.loader);}catch(e){log(e.message);}try{await old.transport.disconnect();}catch(e){log(e.message);}}
}
$('flash').addEventListener('click',async()=>{
 if(busy||session)return;busy=true;update();$('progress').value=0;$('log').textContent='';
 try{
 const install=document.querySelector('input[name=mode]:checked').value==='install';
 const port=await navigator.serial.requestPort();
 status('下载并校验固件…');const manifest=await refreshVersion();log('固件 v'+manifest.version+' · 网页 20261113.6 · 波特率 115200');const board=BOARDS[boardId];const parts=selectedParts(manifest,install);
 const expected=install?board.addresses:[board.app];if(JSON.stringify(parts.map(p=>p.address))!==JSON.stringify(expected))throw Error('固件清单分区不符合预期');
 const files=await Promise.all(parts.map(async p=>({address:p.address,data:await binary(p)})));
 const transport=new Transport(port,true);const loader=new ESPLoader({transport,baudrate:115200,terminal:{clean(){},write:log,writeLine:log},debugLogging:false});session={transport,loader,files,boardId,ready:false};status('正在连接 '+board.name+'…');await loader.main();if(loader.chip.CHIP_NAME!==board.chip)throw Error('芯片与所选硬件不符，已停止，未写入');
 const detected=await loader.detectFlashSize();if(!detected||!/^\d+MB$/.test(detected)||parseInt(detected,10)<16)throw Error('本版需要至少 16MB Flash，检测到 '+(detected||'未知容量')+'，未写入');
 let migrated=false;
 if(!install){status('检查现有分区…');const plan=partitionPlan(await loader.readFlash(0x8000,0x1000),boardId);if(!plan)throw Error('分区不兼容，未写入。请备份并核对原固件布局。');if(plan==='migrate'){const table=manifest.parts.find(p=>p.address===0x8000);const data=await binary(table);if(partitionPlan(data,boardId)!=='current')throw Error('新相册分区校验失败，未写入');if(boardId==='m5-fire'){const boot=manifest.parts.find(p=>p.address===0x1000);files.push({address:boot.address,data:await binary(boot)});}files.push({address:0x8000,data});migrated=true;}}

 session.ready=true;
 $('reviewText').textContent='固件 v'+manifest.version+' · '+board.name+' · '+(install?(boardId==='m5-fire'?'首次安装：将写入引导程序、分区表和应用；原固件数据不保证兼容。':'首次安装：将写入新分区和应用，清空新布局设置区，需重新配网与配置证书。'):(migrated?'保留配置升级：更新应用并新增相册分区；M5 同时更新引导程序以识别完整容量。已有设置和数据分区位置不变。':'保留配置升级：仅写应用区，保留 Wi-Fi、密钥、证书和相册。'));
 $('review').hidden=false;status('检查通过，尚未写入。请核对信息后确认开始刷写。');
 }catch(e){status(e.name==='NotFoundError'?'已取消选择，未写入固件。':'准备失败，未写入：'+e.message);await release(true);}
 finally{busy=false;update();if(session?.ready)$('confirmFlash').focus();}
});
$('confirmFlash').addEventListener('click',async()=>{
 if(busy||!session?.ready)return;busy=true;update();const {loader,files,transport}=session;
 try{
 status('正在写入，请勿断开 USB…');await loader.writeFlash({fileArray:files,flashMode:'dio',flashFreq:'80m',flashSize:BOARDS[session.boardId].flashSize,eraseAll:false,compress:true,calculateMD5Hash:data=>md5(data),reportProgress:(i,w,total)=>{$('progress').value=100*(i+w/total)/files.length;$('status').textContent='正在写入第 '+(i+1)+'/'+files.length+' 个文件 · '+Math.round(100*w/total)+'% · 请勿断开 USB';}});
 $('progress').value=100;status('写入及校验完成，正在重启设备…');try{await restartDevice(transport,loader);status('写入及校验成功，已发送重启信号。请查看设备屏幕确认启动。');}catch(e){log(e.message);status('固件写入及校验成功，但自动重启失败。请手动重启设备，无需重复刷写。');}
 }catch(e){status('刷写未完成：'+e.message+'。请保持供电并重新完成刷写。');}
 finally{await release();busy=false;update();}
});
$('cancelFlash').addEventListener('click',async()=>{
 if(busy)return;busy=true;update();await release(true);status('已取消，未写入固件；已尝试重启原系统。');busy=false;update();
});
window.addEventListener('beforeunload',e=>{if(busy||session){e.preventDefault();e.returnValue='';}});
