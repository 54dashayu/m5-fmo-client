// Bounded stop-and-wait reads avoid large bursts on native USB and consume
// the stub's trailing MD5 packet before any subsequent flash command.
export async function readPartitionTable(loader, md5, onProgress=()=>{}) {
 const size=4096, packetSize=1024, timeout=5000;
 const payload=new Uint8Array(16), view=new DataView(payload.buffer);
 [0x8000,size,packetSize,1].forEach((value,i)=>view.setUint32(i*4,value,true));
 if(!loader.IS_STUB) throw Error('分区读取需要刷写程序就绪，未写入');
 const result=await loader.checkCommand('read partition table',loader.ESP_READ_FLASH,payload,0,0,timeout);
 if(result!==0) throw Error('分区读取请求失败，未写入');
 const data=new Uint8Array(size);let received=0;
 onProgress(0,size);
 while(received<size){
  const packet=await loader.transport.read(timeout);
  if(!(packet instanceof Uint8Array)||packet.length!==Math.min(packetSize,size-received)) throw Error('分区读取数据长度异常，未写入');
  data.set(packet,received);received+=packet.length;
  const ack=new Uint8Array(4);new DataView(ack.buffer).setUint32(0,received,true);
  await loader.transport.write(ack);onProgress(received,size);
 }
 const digest=await loader.transport.read(timeout);
 const hex=Array.from(digest,b=>b.toString(16).padStart(2,'0')).join('');
 if(digest.length!==16||hex!==md5(data)) throw Error('分区读取校验失败，未写入');
 return data;
}
