"""Read a public APRS-derived snapshot; publish minimal server metadata only.
Source verification is reported, never upgraded to local CA verification/login.
"""
import base64,datetime,ipaddress,json,os,re,tempfile,time,urllib.request
from pathlib import Path
SOURCE='https://map.bg5esn.com/api/stream'
MAX_BYTES=4*1024*1024

def stamp(value):
 try:return int(datetime.datetime.fromisoformat(re.sub(r'(\.\d{6})\d+',r'\1',str(value)).replace('Z','+00:00')).timestamp())
 except (ValueError,TypeError,OverflowError):return 0

def normalize(snapshot,now=None):
 now=int(time.time()) if now is None else now
 rows={}
 if not isinstance(snapshot,dict) or not isinstance(snapshot.get('identities'),list):raise ValueError('invalid snapshot')
 for item in snapshot['identities'][:5000]:
  if not isinstance(item,dict):continue
  host=str(item.get('stationUrl') or '').lower().strip();call=str(item.get('callsign') or '').upper();fp=str(item.get('certFingerprint') or '')
  uid=item.get('uid');port=item.get('stationPort')
  if not re.fullmatch(r'[a-z0-9][a-z0-9.-]{0,62}',host) or not re.fullmatch(r'[A-Z0-9]{3,6}',call):continue
  if host=='localhost' or host.endswith(('.local','.localhost')):continue
  try:
   if not ipaddress.ip_address(host).is_global:continue
  except ValueError:
   if '.' not in host:continue
  if type(uid) is not int or not 0<uid<=4294967295 or type(port) is not int or not 0<port<=65535:continue
  seen=stamp(item.get('lastSeen'));verified=stamp(item.get('verifiedAt'))
  if not 0<=now-seen<=86400:continue
  valid_fp=False
  if re.fullmatch(r'[A-Za-z0-9_-]{43}',fp):
   raw=base64.urlsafe_b64decode(fp+'=');valid_fp=len(raw)==32 and base64.urlsafe_b64encode(raw).decode().rstrip('=')==fp
  # A current STATION announcement is required: a later unrelated signed message
  # does not by itself prove that an older cached station endpoint is signed.
  verified_source=(item.get('lastType')=='STATION' and item.get('certStatus')=='Ok' and item.get('sigStatus')=='Valid' and 0<=now-verified<=86400 and valid_fp)
  name=str(item.get('stationName') or call).encode()[:64].decode(errors='ignore')
  name=''.join(c for c in name if ord(c)>=32)
  row=dict(name=name,callsign=call,uid=uid,host=host,port=port,fingerprint=fp if valid_fp else '',lastSeen=seen,sourceVerified=verified_source,source='BG5ESN APRS 广播汇总',status='source_verified' if verified_source else 'unverified')
  rows[(uid,host,port,fp)]=row
 result=sorted(rows.values(),key=lambda r:(not r['sourceVerified'],-r['lastSeen']))[:256]
 return dict(format='m5-discovery-v1',updatedAt=now,sourceUrl=SOURCE,sourceConnected=snapshot.get('status',{}).get('connected') is True,servers=result)

def fetch():
 with urllib.request.urlopen(SOURCE,timeout=20) as response:
  total=0
  for _ in range(30):
   line=response.readline(MAX_BYTES+1);total+=len(line)
   if total>MAX_BYTES:raise ValueError('snapshot too large')
   if line.startswith(b'data:'):return normalize(json.loads(line[5:]))
 raise ValueError('snapshot missing')

def publish(path,data):
 path=Path(path);path.parent.mkdir(parents=True,exist_ok=True)
 fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.catalog-')
 try:
  with os.fdopen(fd,'w') as f:json.dump(data,f,ensure_ascii=False,separators=(',',':'));f.flush();os.fsync(f.fileno())
  os.chmod(tmp,0o644);os.replace(tmp,path)
 finally:
  if os.path.exists(tmp):os.unlink(tmp)
if __name__=='__main__':
 import sys
 data=fetch();publish(sys.argv[1],data);print('Published',len(data['servers']),'server entries')
