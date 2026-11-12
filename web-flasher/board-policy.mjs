export const BOARDS={
 'm5-fire':{name:'M5 FIRE 类（ESP32）',chip:'ESP32',flashSize:'4MB',app:0x10000,nvsSize:0x6000,appSize:0x300000,addresses:[0x1000,0x8000,0x10000],names:['bootloader.bin','partition-table.bin','application.bin'],hardware:'原版 ESP32 的 M5 FIRE 类硬件：模拟麦克风 GPIO34、扬声器 GPIO25。不是 Core2 / CoreS3。',photo:'photos/m5-device-1.jpg'},
 'xingzhi-cube-1.54tft-wifi':{name:'小智 Xingzhi Cube 1.54（ESP32-S3）',chip:'ESP32-S3',flashSize:'16MB',app:0x30000,nvsSize:0x17000,appSize:0x500000,addresses:[0,0x8000,0x9000,0x30000],names:['bootloader.bin','partition-table.bin','initial-settings.bin','application.bin'],hardware:'Xingzhi Cube 1.54 英寸 TFT，ESP32-S3 R8N16（16MB Flash / 8MB PSRAM），三实体按键。不是所有小智设备通用。',photo:'photos/xiaozhi-device.jpg'}
};
export function validateManifest(manifest,id){
 const b=BOARDS[id];if(!b)throw Error('请选择支持的硬件');
 if(!/^\d+\.\d+\.\d+(?:-dev(?:\.[1-9][0-9]*)?|-rc[1-9][0-9]*)?$/.test(manifest.version))throw Error('版本格式异常');
 if(manifest.board!==id||manifest.chip!==b.chip||manifest.flashSize!==b.flashSize||!Array.isArray(manifest.parts)||manifest.parts.length!==b.addresses.length)throw Error('固件清单与所选硬件不符');
 const base=id+'/v'+manifest.version+'/';
 manifest.parts.forEach((p,i)=>{if(p.address!==b.addresses[i]||p.path!==base+b.names[i]||!/^[a-f0-9]{64}$/.test(p.sha256))throw Error('固件路径、地址或校验值异常');});
 if(manifest.download!==base+'fmo-'+id+'-v'+manifest.version+'.zip')throw Error('下载路径异常');
 return manifest;
}
export function validPartitions(data,id){
 const b=BOARDS[id];if(!b)return false;
 const d=new DataView(data.buffer,data.byteOffset,data.byteLength);let nvs=false,app=false;
 for(let p=0;p+32<=data.length;p+=32){if(d.getUint16(p,true)!==0x50aa)break;const type=d.getUint8(p+2),sub=d.getUint8(p+3),off=d.getUint32(p+4,true),size=d.getUint32(p+8,true);if(type===1&&sub===2&&off===0x9000&&size===b.nvsSize)nvs=true;if(type===0&&sub===0&&off===b.app&&size===b.appSize)app=true;}
 return nvs&&app;
}
export function selectedParts(manifest,install){return manifest.parts.filter(p=>install||p.address===BOARDS[manifest.board].app);}
