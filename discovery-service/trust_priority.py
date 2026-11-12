"""Administrator-confirmed endpoints stay separate from broadcast/CA evidence."""
import re,ipaddress

def identity(r):
 return (r['uid'],r['callsign'],r['host'].lower(),r['port'],r['fingerprint'])

def confirmed_rows(policy,now):
 result=[]
 for r in policy.get('confirmedServers',[]):
  try:
   assert r.get('enabled') is True and r.get('mutualTrustConfirmed') is True
   assert type(r['confirmedAt']) is int and r['confirmedAt']<=now<r['expiresAt']
   assert r.get('evidence') and type(r['uid']) is int and 0<r['uid']<=4294967295
   assert re.fullmatch(r'B[A-Z][0-9][A-Z]{1,3}',r['callsign'])
   assert re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9.-]{0,62}',r['host']) and '.' in r['host']
   assert not r['host'].endswith(('.local','.localhost'))
   if re.fullmatch(r'[0-9.]+',r['host']):assert ipaddress.ip_address(r['host']).is_global
   assert type(r['port']) is int and 0<r['port']<=65535
   assert re.fullmatch(r'[A-Za-z0-9_-]{43}',r['fingerprint'])
   assert 0<len(r['name'].encode())<=64 and all(ord(c)>=32 for c in r['name'])
   row={k:r[k] for k in ['uid','callsign','host','port','fingerprint','name']}
   row.update(lastSeen=now,sourceVerified=False,rootTrusted=False,adminConfirmed=True,mutualTrustConfirmed=True,addable=True,stale=False,trustPriority=0,source='管理员确认互信',confirmedAt=r['confirmedAt'])
   result.append(row)
  except (KeyError,TypeError,ValueError,AssertionError):continue
 return result

def prioritize(rows,policy,now):
 preferred=set(policy.get('preferredRootPublicKeys',[]));merged={}
 for raw in rows:
  row=dict(raw);row['adminConfirmed']=False;row['mutualTrustConfirmed']=False
  row['trustPriority']=1 if row.get('rootTrusted') and row.get('rootPublicKey') in preferred else 2 if row.get('rootTrusted') else 3
  merged[identity(row)]=row
 for row in confirmed_rows(policy,now):
  old=merged.get(identity(row),{});row['rootTrusted']=old.get('rootTrusted',False);row['sourceVerified']=old.get('sourceVerified',False)
  merged[identity(row)]=row
 return sorted(merged.values(),key=lambda r:(r.get('stale',False),r['trustPriority'],-r['lastSeen'],r['uid']))
