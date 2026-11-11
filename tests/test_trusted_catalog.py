import sys,tempfile,json,time,hashlib,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'discovery-service'))
from trusted_catalog import authorities,station
from fmo_identity import ephemeral_test_chain,tbs,b64,unb64
import cbor2
class Tests(unittest.TestCase):
 def setUp(self):
  self.now=1794386400
  self.root,self.mid,self.user,self.issuer,self.device=ephemeral_test_chain(self.now)
  self.now=self.user['iat']+1
  self.tmp=tempfile.TemporaryDirectory();self.p=Path(self.tmp.name)
  (self.p/'roots').mkdir();(self.p/'mid').mkdir();(self.p/'roots/r.json').write_text(json.dumps(self.root));(self.p/'mid/i.json').write_text(json.dumps(self.mid))
 def tearDown(self):self.tmp.cleanup()
 def raw(self,name='任意名称测试台'):
  c=cbor2.loads(tbs(self.user))+[unb64(self.user['signature'])];blob=cbor2.dumps(c,canonical=True);call=c[4]
  msg=['FMO',4,'STATION',call,15,'3037.98N','11932.20E',hashlib.sha256(blob).digest(),'CN',name,'example.net',1883,500,2,10,self.now//600]
  return f'{call}-15>APFMO4,TCPIP*,qAC,T2TEST:=3037.98N/11932.20EiFMO-V4,STATION,CERT:{b64(blob)},CN,{name},example.net,P1883,F500KM,U2/10,SIG:{b64(self.device.sign(cbor2.dumps(msg,canonical=True)))}'
 def test_valid(self):
  a=authorities(self.p/'roots',[self.p/'mid'],self.now);r=station(self.raw(),self.now,a,self.now);self.assertEqual(r['name'],'任意名称测试台');self.assertTrue(r['rootTrusted']);self.assertEqual(r['rootPublicKey'],self.root['subject']['publicKey'])
 def test_tamper_and_unknown(self):
  a=authorities(self.p/'roots',[self.p/'mid'],self.now)
  for raw,when,aa in [(self.raw().replace('example.net','attacker.net'),self.now,a),(self.raw(),self.now-1201,a),(self.raw('西南集群'),self.now,{})]:
   with self.assertRaises(ValueError):station(raw,when,aa,self.now)
  (self.p/'roots/r.json').unlink();self.assertEqual(authorities(self.p/'roots',[self.p/'mid'],self.now),{})
if __name__=='__main__':unittest.main()
