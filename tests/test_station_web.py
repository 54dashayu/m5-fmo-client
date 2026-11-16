from pathlib import Path
import subprocess,tempfile,re
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/web.html').read_text();script=s.split('<script>')[1].split('</script>')[0]
subprocess.run(['node','--check'],input=script,text=True,check=True)
js=script[script.index("let stationLoadedCall="):script.rindex('})();')]
pre=r'''const assert=require('node:assert/strict');
class El{constructor(){this.value='';this.checked=false;this.hidden=false;this.disabled=false;this.readOnly=false;this.textContent='';this.children=[];}append(x){this.children.push(x)}querySelector(){return new El()}}
const els={};const $=id=>els[id]??=(new El());const document={createElement:()=>new El()};let calls=[],reject=false;
let data={callsign:'BH1JSS',callsignLocked:true,prefix:true,boardPrefix:'E.',aprsSsid:15,latitude:0,longitude:0,frequencyMHz:438.5,antennaHeightM:10,rig:'IC-705',antenna:'GP',remark:'test'};
async function api(path,method,body){calls.push({path,method,body});if(reject)throw Error('save failed');return method==='POST'?{message:'saved'}:{...data}}
const status=async()=>{};const showTab=()=>{};
'''
tail=r'''
(async()=>{
 await loadStation();assert.equal($('stationLat').value,0);assert.equal($('stationSsid').children.length,16);
 $('stationLat').value='0';$('stationLon').value='0';$('stationFreq').value='438.5';$('stationHeight').value='10';$('stationPrefix').checked=false;$('stationSsid').value='7';
 const ev={preventDefault(){},target:new El()};await $('stationForm').onsubmit(ev);let v=calls.at(-1).body;assert.equal(v.prefix,false);assert.equal(v.aprsSsid,7);assert.equal(v.latitude,0);assert.equal(v.longitude,0);
 const before=calls.length;$('stationLon').value='';await $('stationForm').onsubmit(ev);assert.equal(calls.length,before);assert($('stationFeedback').textContent.includes('一起'));
 $('stationLat').value='';$('stationLon').value='';await $('stationForm').onsubmit(ev);assert.equal(calls.at(-1).body.latitude,null);
 reject=true;$('stationRemark').value='retained';await $('stationForm').onsubmit(ev);assert.equal($('stationRemark').value,'retained');assert.equal($('stationFeedback').textContent,'save failed');
 console.log('PASS actual station form: zero location, paired coordinates, prefix, SSID, save failure draft preservation');
})().catch(e=>{console.error(e);process.exit(1)});
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.js';p.write_text(pre+js+tail);subprocess.run(['node',str(p)],check=True)
assert script.index('let stationLoadedCall=')<script.rindex('})();')
assert "if(tab==='certificates'){loadStation();broadcastStatus(true)}" in script
