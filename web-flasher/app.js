import {ESPLoader, Transport} from 'esptool-js';
import {md5} from 'js-md5';
const $=id=>document.getElementById(id);
let busy=false;
const supported=isSecureContext && 'serial' in navigator;
$('support').textContent=supported?'点击后选择 M5 对应的 USB 串口。写入过程中请保持连接。':'当前浏览器不支持在线刷写。请用电脑 Chrome / Edge 打开此 HTTPS 页面，或下载固件包。';
function update(){const install=document.querySelector('input[name=mode]:checked').value==='install';$('installNote').hidden=$('confirmRow').hidden=!install;$('flash').disabled=busy||!supported||!$('hardware').checked||(install&&!$('installConfirm').checked);}
document.querySelectorAll('input').forEach(x=>x.addEventListener('change',update));update();
const log=t=>{$('log').textContent=($('log').textContent+String(t)+'\n').slice(-12000);$('log').scrollTop=$('log').scrollHeight;};
const status=t=>{$('status').textContent=t;log(t);};
const hex=bytes=>Array.from(new Uint8Array(bytes),x=>x.toString(16).padStart(2,'0')).join('');
async function binary(part){const r=await fetch('firmware/'+part.path);if(!r.ok)throw Error('固件下载失败：'+r.status);const b=new Uint8Array(await r.arrayBuffer());if(hex(await crypto.subtle.digest('SHA-256',b))!==part.sha256)throw Error('固件校验不符，请刷新页面重试');return b;}
function validPartitions(data){const d=new DataView(data.buffer,data.byteOffset,data.byteLength);let nvs=false,app=false;for(let p=0;p+32<=data.length;p+=32){if(d.getUint16(p,true)!==0x50aa)break;const type=d.getUint8(p+2),sub=d.getUint8(p+3),off=d.getUint32(p+4,true),size=d.getUint32(p+8,true);if(type===1&&sub===2&&off===0x9000&&size===0x6000)nvs=true;if(type===0&&sub===0&&off===0x10000&&size===0x300000)app=true;}return nvs&&app;}
$('flash').addEventListener('click',async()=>{
 if(busy)return;busy=true;update();document.querySelectorAll('input').forEach(x=>x.disabled=true);$('progress').value=0;$('log').textContent='';let transport,loader;
 try{
 const install=document.querySelector('input[name=mode]:checked').value==='install';
 const port=await navigator.serial.requestPort();
 status('下载并校验固件…');const r=await fetch('firmware/manifest.json',{cache:'no-store'});if(!r.ok)throw Error('无法获取版本信息');const manifest=await r.json();const parts=manifest.parts.filter(p=>install||p.address===0x10000);const files=await Promise.all(parts.map(async p=>({address:p.address,data:await binary(p)})));
 transport=new Transport(port,true);loader=new ESPLoader({transport,baudrate:115200,terminal:{clean(){},write:log,writeLine:log},debugLogging:false});status('正在连接 M5…');await loader.main();if(loader.chip.CHIP_NAME!=='ESP32')throw Error('检测到非原版 ESP32，已停止，未写入');
 if(!install){status('检查现有分区…');if(!validPartitions(await loader.readFlash(0x8000,0x1000)))throw Error('分区不兼容，未写入。确认硬件和备份后可使用首次安装。');}
 status('正在写入，请勿断开 USB…');await loader.writeFlash({fileArray:files,flashMode:'dio',flashFreq:'80m',flashSize:'4MB',eraseAll:false,compress:true,calculateMD5Hash:data=>md5(data),reportProgress:(i,w,total)=>{$('progress').value=100*(i+w/total)/files.length;}});
 $('progress').value=100;status('写入及校验完成，正在重启设备…');await loader.after('hard_reset');status('刷写成功。请查看 M5 屏幕，等待设备启动。');
 }catch(e){status(e.name==='NotFoundError'?'已取消选择，未写入固件。':'未完成：'+e.message+'。如已开始写入，请保持供电并重新完成刷写。');}
 finally{if(transport)try{await transport.disconnect();}catch(e){log(e.message);}busy=false;document.querySelectorAll('input').forEach(x=>x.disabled=false);update();}
});
window.addEventListener('beforeunload',e=>{if(busy){e.preventDefault();e.returnValue='';}});
