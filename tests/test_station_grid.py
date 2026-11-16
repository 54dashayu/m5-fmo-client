from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/web.html').read_text()
grid=(r/'web-flasher/public/grid-map/grid.js').read_text()
assert grid in s, 'Device and hosted map must share identical conversion functions'
js=s.split("let stationLoadedCall='',stationBoardPrefix='';")[1].split('for(let i=0;i<16;')[0]
# Stop before the existing station setup even if whitespace changes.
js=js[:js.index("for(let i=")] if "for(let i=" in js else js
pre=r'''
const assert=require('node:assert/strict'),els={};
const $=id=>els[id]??={value:'',setCustomValidity(v){this.error=v}};
let listener;const window={addEventListener(t,f){listener=f},open(){return {}}};
const crypto=require('node:crypto').webcrypto,location={origin:'http://192.168.1.1'};
'''
tail=r'''
assert.equal(locationGrid(0,0),'JJ00AA');assert.equal(locationGrid(39.9,116.4),'OM89EV');
assert.equal(locationGrid(-90,-180,8),'AA00AA00');assert.equal(locationGrid(90,180,8),'RR99XX99');
for(const g of ['AA00','RR99XX99','OM89EV','FN31PR','jj00aa00']){const p=gridCenter(g);assert.equal(locationGrid(p.latitude,p.longitude,g.length),g.toUpperCase());}
for(const g of ['','AA','SS00','AA00YY','AA0000','AA00AAA','AA00AA000'])assert.throws(()=>gridCenter(g));
for(const p of [[NaN,0],[0,Infinity],[91,0],[0,-181]])assert.throws(()=>locationGrid(...p));
for(let i=0;i<2000;i++){const lat=-90+Math.random()*180,lon=-180+Math.random()*360;for(const n of [4,6,8]){const g=locationGrid(lat,lon,n),p=gridCenter(g);assert(Math.abs(p.latitude-lat)<=p.dy/2+1e-9);assert(Math.abs(p.longitude-lon)<=p.dx/2+1e-9);}}
$('stationGrid').value='om89ev';stationGridChanged();assert.equal($('stationGrid').value,'OM89EV');assert.equal(locationGrid(Number($('stationLat').value),Number($('stationLon').value)),'OM89EV');
$('stationLat').value='39.9';$('stationLon').value='116.4';stationCoordinatesChanged();stationGridChanged();assert.equal($('stationLat').value,'39.9'); // no precision loss when saving existing point
$('stationMap').onclick();const source=stationMapWindow,nonce=stationMapNonce;
const msg={origin:'https://app.bh1jss.net',source,data:{type:'fmo-grid-selection',nonce,latitude:0,longitude:0}};
listener({...msg,origin:'https://evil.example'});assert.equal($('stationLat').value,'39.9');
listener({...msg,source:{}});assert.equal($('stationLat').value,'39.9');
listener({...msg,data:{...msg.data,nonce:'wrong'}});assert.equal($('stationLat').value,'39.9');
listener({...msg,data:{...msg.data,latitude:91}});assert.equal($('stationLat').value,'39.9');
listener(msg);assert.equal($('stationLat').value,'0.0000000');assert.equal($('stationGrid').value,'JJ00AA');
listener({...msg,data:{...msg.data,latitude:10}});assert.equal($('stationLat').value,'0.0000000'); // consumed selection
console.log('PASS grid boundary/roundtrip, existing coordinate precision, map origin/source/nonce and replay checks');
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'grid.js';p.write_text(pre+js+tail);subprocess.run(['node',str(p)],check=True)
subprocess.run(['node','--check',str(r/'web-flasher/public/grid-map/picker.js')],check=True)
