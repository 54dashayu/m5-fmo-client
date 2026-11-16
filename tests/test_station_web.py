from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text();script=s.split('<script>')[1].split('</script>')[0]
subprocess.run(['node','--check'],input=script,text=True,check=True)
js=script[script.index("let stationLoadedCall="):script.rindex('})();')]
pre=r'''const assert=require('node:assert/strict');
class El{constructor(){this.value='';this.checked=false;this.hidden=false;this.disabled=false;this.readOnly=false;this.textContent='';this.children=[];}setCustomValidity(v){this.validationMessage=v}append(x){this.children.push(x)}querySelector(){return new El()}}
const els={};const $=id=>els[id]??=(new El());const document={createElement:()=>new El()};let calls=[],reject=false,rejectBroadcast=false;
let data={callsign:'BH1JSS',callsignLocked:true,prefix:true,boardPrefix:'E.',aprsSsid:15,latitude:0,longitude:0,frequencyMHz:438.5,antennaHeightM:10,rig:'IC-705',antenna:'GP',remark:'test',aprsHost:'srvr.aprs-is.net',aprsPort:14580,aprsPasscodeSet:false,broadcastEnabled:false,broadcastSent:0};
async function api(path,method,body){calls.push({path,method,body});if(reject||(rejectBroadcast&&path==='/api/station/broadcast'&&method==='POST'))throw Error('save failed');return method==='POST'?{message:'saved'}:{...data}}
const window={addEventListener(){}};const status=async()=>{};const showTab=()=>{};
'''
tail=r'''
(async()=>{
 await loadStation();assert.equal($('stationLat').value,0);assert.equal($('stationSsid').children.length,16);
 $('stationLat').value='0';$('stationLon').value='0';$('stationFreq').value='438.5';$('stationHeight').value='10';$('stationPrefix').checked=false;$('stationSsid').value='7';
 const ev={preventDefault(){},target:new El()};await $('stationForm').onsubmit(ev);let v=calls.findLast(c=>c.path==='/api/station'&&c.method==='POST').body;assert.equal(v.prefix,false);assert.equal(v.aprsSsid,7);assert.equal(v.latitude,0);assert.equal(v.longitude,0);
 const before=calls.length;$('stationLon').value='';stationCoordinatesChanged();await $('stationForm').onsubmit(ev);assert.equal(calls.length,before);assert($('stationFeedback').textContent.includes('一起'));
 $('stationLat').value='';$('stationLon').value='';stationCoordinatesChanged();await $('stationForm').onsubmit(ev);assert.equal(calls.findLast(c=>c.path==='/api/station'&&c.method==='POST').body.latitude,null);
 reject=true;$('stationRemark').value='retained';await $('stationForm').onsubmit(ev);assert.equal($('stationRemark').value,'retained');assert.equal($('stationFeedback').textContent,'save failed');
 reject=false;
 const old=calls.length;$('broadcastPass').value='bad';await $('stationForm').onsubmit(ev);assert.equal(calls.length,old); // reject before either write
 $('broadcastPass').value='';$('broadcastEnabled').checked=true;await $('stationForm').onsubmit(ev);assert.equal(calls.length,old);
 $('broadcastEnabled').checked=false;rejectBroadcast=true;await $('stationForm').onsubmit(ev);assert($('stationFeedback').textContent.includes('资料已保存'));assert.equal($('stationRemark').value,'retained');
 rejectBroadcast=false;stationBroadcastEnabled=true;const begin=calls.length;await $('stationForm').onsubmit(ev);const posted=calls.slice(begin).filter(c=>c.method==='POST');assert.deepEqual(posted.map(c=>c.path),['/api/station/broadcast','/api/station']);
 reject=true;await loadStation();const n=calls.length;await $('stationForm').onsubmit(ev);assert.equal(calls.length,n);assert($('stationSave').disabled);
 console.log('PASS actual station form: zero location, paired coordinates, prefix, SSID, save failure draft preservation');
})().catch(e=>{console.error(e);process.exit(1)});
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.js';p.write_text(pre+js+tail);subprocess.run(['node',str(p)],check=True)
assert script.index('let stationLoadedCall=')<script.rindex('})();')
assert "if(tab==='certificates'){loadStation()}" in script
