"""Verify APRS STATION signatures against enabled local roots and public issuers.
Never infer trust from a display name. This feed establishes server identity only;
the M5 must perform its own target-bound MQTT CONNECT before exposing a node.
"""
import base64,hashlib,json,sys,time,re,urllib.request,ipaddress
from pathlib import Path
import cbor2
from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey,Ed25519PrivateKey
from catalog import publish,stamp
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from fmo_identity import tbs,unb64,b64

def verify(cert,key):
 if cert.get('signatureAlgorithm')!='Ed25519':raise ValueError('algorithm')
 Ed25519PublicKey.from_public_bytes(unb64(key)).verify(unb64(cert['signature']),tbs(cert))

def authorities(roots,issuers,now):
 trusted={};mids={}
 for p in Path(roots).glob('*.json'):
  try:
   c=json.loads(p.read_text());assert c['type']=='rootCA' and c['iat']<=now<c['exp'] and c['extensions']['isCA'] is True and c['extensions']['pathLen']>=1 and c['issuer']['name']==c['subject']['name'];verify(c,c['subject']['publicKey']);trusted[c['sn']]=c
  except (ValueError,KeyError,AssertionError,TypeError,InvalidSignature):continue
 for folder in issuers:
  for p in Path(folder).glob('*.json'):
   try:
    c=json.loads(p.read_text());c=c.get('intermediateCert',c)
    assert c['type']=='intermediateCA';r=trusted[c['issuer']['sn']]
    assert c['issuer']['publicKey']==r['subject']['publicKey'] and c['issuer']['name']==r['subject']['name'] and r['iat']<=c['iat']<=now<c['exp']<=r['exp'] and c['extensions']['isCA'] is True and c['extensions']['pathLen']==0
    verify(c,r['subject']['publicKey']);mids.setdefault(c['sn'],[]).append((c,r))
   except (ValueError,KeyError,AssertionError,TypeError,InvalidSignature):continue
 return mids

def station(raw,received,issuers,now):
 if len(raw.encode())>2048 or not 0<=now-received<1200:raise ValueError('stale')
 m=re.fullmatch(r'([A-Z0-9]{3,6})-(\d{1,2})>APFMO4,[^:]*:=(\d{4}\.\d{2}[NS]).(\d{5}\.\d{2}[EW]).\s*(FMO-V4,STATION,.*)',raw)
 if not m:raise ValueError('not STATION')
 call,ssid,lat,lon,payload=m.groups();ssid=int(ssid)
 if ssid>15:raise ValueError('ssid')
 fields=payload.split(',')
 if len(fields)!=10 or not fields[2].startswith('CERT:') or not fields[9].startswith('SIG:'):raise ValueError('fields')
 blob=unb64(fields[2][5:]);cert=cbor2.loads(blob)
 if cbor2.dumps(cert,canonical=True)!=blob or len(cert)!=10 or cert[:3]!=['FMO',4,'userCert'] or cert[4]!=call or type(cert[5]) is not int or not 0<cert[5]<=4294967295 or len(cert[6])!=32 or not cert[7]<=now<cert[8]:raise ValueError('certificate')
 cc,name,host=fields[3:6]
 if not re.fullmatch('[A-Z]{2}',cc) or not name or len(name.encode())>64 or any(ord(c)<32 for c in name):raise ValueError('name')
 if not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9.-]{0,62}',host) or '.' not in host or host.endswith(('.local','.localhost')):raise ValueError('host')
 try:
  if not ipaddress.ip_address(host).is_global:raise ValueError('private host')
 except ValueError:
  if re.fullmatch(r'[\d.]+',host):raise
 if not re.fullmatch(r'P\d{1,5}',fields[6]) or not re.fullmatch(r'F\d+KM',fields[7]) or not re.fullmatch(r'U\d+/\d+',fields[8]):raise ValueError('numbers')
 port=int(fields[6][1:]);km=int(fields[7][1:-2]);online,peak=map(int,fields[8][1:].split('/'))
 if not 0<port<=65535:raise ValueError('port')
 root=None
 for mid,r in issuers.get(cert[3],[]):
  try:
   ext=mid['extensions'];assert mid['iat']<=cert[7] and cert[8]<=mid['exp'] and ext['uidRange']['start']<=cert[5]<=ext['uidRange']['end'] and cc in ext['issuingCountries'];Ed25519PublicKey.from_public_bytes(unb64(mid['subject']['publicKey'])).verify(cert[9],cbor2.dumps(cert[:9],canonical=True));root=r;break
  except Exception:continue
 if root is None:raise ValueError('unknown or invalid issuer')
 signature=unb64(fields[9][4:]);prefix=['FMO',4,'STATION',call,ssid,lat,lon,hashlib.sha256(blob).digest(),cc,name,host,port,km,online,peak]
 valid=False
 for salt in [int(now)//600,int(now)//600-1,int(now)//600+1]:
  try:Ed25519PublicKey.from_public_bytes(cert[6]).verify(signature,cbor2.dumps(prefix+[salt],canonical=True));valid=True;break
  except Exception:pass
 if not valid:raise ValueError('announcement signature')
 return dict(name=name,callsign=call,uid=cert[5],host=host.lower(),port=port,fingerprint=b64(hashlib.sha256(cbor2.dumps(cert[:9],canonical=True)).digest()),lastSeen=received,sourceVerified=True,rootTrusted=True,rootPublicKey=root['subject']['publicKey'])

def main():
 now=int(time.time());base=Path('/var/lib/fmo-discovery');cache=base/'raw-stations.json';raws=[]
 if cache.exists():
  try:raws=json.loads(cache.read_text())
  except ValueError:pass
 # Read-only published APRS log; no APRS transmission or private credentials.
 with urllib.request.urlopen('https://map.bg5esn.com/api/log?limit=5000',timeout=15) as r:
  data=r.read(4*1024*1024+1)
  if len(data)>4*1024*1024:raise ValueError('log too large')
  logs=json.loads(data)
 for x in logs:
  raw=x.get('raw','');at=stamp(x.get('time'))
  if ',STATION,' in raw and 0<=now-at<1200:raws.append(dict(raw=raw,at=at))
 raws=list({x['raw']:x for x in raws if 0<=now-x['at']<1200}.values())[-512:];publish(cache,raws)
 issuers=authorities('/opt/fmo-sas/.sas/roots',['/opt/fmo-m5-enrollment/public-certificates',base/'intermediates'],now)
 servers={};unknown=0
 for x in raws:
  try:
   row=station(x['raw'],x['at'],issuers,now);servers[(row['uid'],row['host'],row['port'],row['fingerprint'])]=row
  except Exception:unknown+=1
 result=dict(format='m5-trusted-discovery-v1',updatedAt=now,sourceConnected=True,servers=sorted(servers.values(),key=lambda r:-r['lastSeen'])[:6])
 for row in result['servers']:row.pop('rootPublicKey',None)
 payload=json.dumps(result,ensure_ascii=False,separators=(',',':'));key=Ed25519PrivateKey.from_private_bytes((base/'catalog-signing.key').read_bytes());publish(base/'trusted.json',dict(payload=payload,signature=b64(key.sign(payload.encode()))));print('CA verified stations:',len(result['servers']),'unresolved:',unknown,'issuer identities:',len(issuers))
if __name__=='__main__':main()
