'use strict';
const $=id=>document.getElementById(id),params=new URLSearchParams(location.hash.slice(1));
let targetOrigin='',nonce=params.get('nonce')||'',selected=null;
try{const u=new URL(params.get('origin'));if(['http:','https:'].includes(u.protocol)&&u.origin===params.get('origin')&&/^[a-f0-9]{32}$/.test(nonce))targetOrigin=u.origin;}catch(e){}
// The fragment is never sent to the server; it contains no device credentials.
const map=L.map('map').setView([35,105],4);
L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',{maxZoom:18,attribution:'&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'}).on('tileerror',()=>{$('feedback').textContent='底图暂时无法加载，可输入已知网格地址定位。';}).addTo(map);
let marker=null,cell=null;
function select(lat,lon,grid){
 const g=grid||locationGrid(lat,lon),center=gridCenter(g);
 selected={latitude:lat,longitude:lon,grid:g};$('grid').value=g;
 if(marker)map.removeLayer(marker);if(cell)map.removeLayer(cell);
 marker=L.marker([lat,lon],{icon:L.divIcon({html:'＋',className:'cross',iconSize:[28,28],iconAnchor:[14,14]})}).addTo(map);
 cell=L.rectangle([[center.latitude-center.dy/2,center.longitude-center.dx/2],[center.latitude+center.dy/2,center.longitude+center.dx/2]],{color:'#f4772b',weight:2,fillOpacity:.08}).addTo(map);
 $('selection').textContent=g+' · 纬度 '+lat.toFixed(6)+' · 经度 '+lon.toFixed(6);
 $('confirm').disabled=!window.opener||!targetOrigin;$('copy').disabled=false;
 if(!window.opener||!targetOrigin)$('feedback').textContent='可复制网格地址，返回设备页面粘贴。';
}
map.on('click',e=>{const p=e.latlng.wrap();if(Math.abs(p.lat)<=90){select(p.lat,p.lng);$('feedback').textContent='已选位置，确认后填回设备页面。';}});
$('find').onclick=()=>{try{const p=gridCenter($('grid').value);select(p.latitude,p.longitude,p.grid);map.fitBounds(cell.getBounds(),{maxZoom:14,padding:[25,25]});}catch(e){$('feedback').textContent=e.message;}};
$('grid').onkeydown=e=>{if(e.key==='Enter')$('find').click();};
$('locate').onclick=()=>{if(!navigator.geolocation){$('feedback').textContent='浏览器不支持定位，请点击地图选点。';return;}navigator.geolocation.getCurrentPosition(p=>{select(p.coords.latitude,p.coords.longitude);map.setView([p.coords.latitude,p.coords.longitude],13);},()=>{$('feedback').textContent='未能获取位置，请点击地图或输入网格地址。';},{timeout:10000});};
$('confirm').onclick=()=>{if(!selected||!window.opener||!targetOrigin)return;window.opener.postMessage({type:'fmo-grid-selection',nonce,latitude:selected.latitude,longitude:selected.longitude},targetOrigin);$('feedback').textContent='已填回设备页面，请返回并保存资料。';window.close();};
$('copy').onclick=async()=>{if(!selected)return;try{await navigator.clipboard.writeText(selected.grid);$('feedback').textContent='已复制 '+selected.grid+'，请粘贴到设备的网格地址栏。';}catch(e){$('feedback').textContent='请手动复制上方网格地址。';$('grid').select();}};
if(params.has('lat')&&params.has('lon')){try{const lat=Number(params.get('lat')),lon=Number(params.get('lon'));locationGrid(lat,lon);select(lat,lon);map.setView([lat,lon],11);}catch(e){}}
