// Maidenhead conversion: coordinates are WGS84; upper world edges belong to the last cell.
function gridCenter(value){
 const g=String(value).trim().toUpperCase();
 if(!/^[A-R]{2}[0-9]{2}(?:[A-X]{2}(?:[0-9]{2})?)?$/.test(g))throw Error('网格地址须为 4、6 或 8 位，例如 OM89EV');
 let x=(g.charCodeAt(0)-65)*20+Number(g[2])*2,y=(g.charCodeAt(1)-65)*10+Number(g[3]),dx=2,dy=1;
 if(g.length>=6){dx/=24;dy/=24;x+=(g.charCodeAt(4)-65)*dx;y+=(g.charCodeAt(5)-65)*dy;}
 if(g.length===8){dx/=10;dy/=10;x+=Number(g[6])*dx;y+=Number(g[7])*dy;}
 return {latitude:y+dy/2-90,longitude:x+dx/2-180,grid:g,dx,dy};
}
function locationGrid(lat,lon,precision=6){
 if(!Number.isFinite(lat)||!Number.isFinite(lon)||Math.abs(lat)>90||Math.abs(lon)>180||![4,6,8].includes(precision))throw Error('经纬度超出范围');
 let x=Math.min(lon+180,360-1e-9),y=Math.min(lat+90,180-1e-9),out='';
 for(const [dx,dy,base] of [[20,10,65],[2,1,48],[1/12,1/24,65],[1/120,1/240,48]].slice(0,precision/2)){
  const a=Math.floor((x+1e-11)/dx),b=Math.floor((y+1e-11)/dy);out+=String.fromCharCode(base+a,base+b);x-=a*dx;y-=b*dy;
 }
 return out;
}
