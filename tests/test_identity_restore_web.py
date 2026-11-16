from pathlib import Path
import subprocess,tempfile,json
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text();start=s.index("$('certfile').onchange=");end=s.index("$('rollback').onclick",start);handlers=s[start:end]
pre=r'''
const assert=require('assert');const nodes={};const $=id=>nodes[id]||(nodes[id]={value:'',textContent:'',checked:true,hidden:false,files:[]});
let bundle=null,privateSeed=null,certificateFileError='',usbTransport=false,poll=null,lastMessage='',calls=[];
const msg=s=>lastMessage=s;const status=async()=>{};
let fixture={format:'fmo-device-identity-v1',deviceSeed:'A'.repeat(43),bundle:{rootCert:{subject:{}},intermediateCert:{},userCert:{subject:{callsign:'BH1JSS',publicKey:'oldkey'},exp:1800000000}}};
const decodeCertificateFile=()=>fixture;const api=async(...args)=>{calls.push(args);return {message:'saved'}};
$('certfile').files=[{size:1600,arrayBuffer:async()=>new ArrayBuffer(0)}];
'''
tail=r'''
(async()=>{await $('certfile').onchange();assert(lastMessage.includes('USB'));assert.equal(bundle,null);
await $('importForm').onsubmit({preventDefault(){}});assert(lastMessage.includes('USB'));assert.equal(calls.length,0);
usbTransport=true;await $('certfile').onchange();assert.equal(privateSeed,'A'.repeat(43));assert.equal($('certpreview').hidden,false);
await $('importForm').onsubmit({preventDefault(){}});assert.equal(calls.length,1);assert.equal(calls[0][2].privateSeed,'A'.repeat(43));assert.equal(privateSeed,null);
console.log('PASS full identity import: Wi-Fi rejection survives submit; no secret sent over Wi-Fi; USB import carries backup and clears secret');})();
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.js';p.write_text(pre+handlers+tail);subprocess.run(['node',str(p)],check=True)
