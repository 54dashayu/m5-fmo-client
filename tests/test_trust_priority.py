import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'discovery-service'))
from trust_priority import prioritize
class Tests(unittest.TestCase):
 def setUp(self):
  self.r=dict(name='node',callsign='BH1JSS',uid=1791,host='example.net',port=28883,fingerprint='A'*43,lastSeen=100,rootTrusted=True,rootPublicKey='preferred')
  self.p=dict(preferredRootPublicKeys=['preferred'],confirmedServers=[dict(self.r,enabled=True,mutualTrustConfirmed=True,confirmedAt=10,expiresAt=200,evidence='administrator confirmation')])
 def test_priority_and_persistence(self):
  other=dict(self.r,uid=1,lastSeen=150,rootPublicKey='other');preferred=dict(self.r,uid=2)
  rows=prioritize([other,preferred],self.p,160)
  self.assertEqual([r['uid'] for r in rows],[1791,2,1]);self.assertTrue(rows[0]['adminConfirmed']);self.assertFalse(rows[0]['rootTrusted']);self.assertFalse(rows[0]['sourceVerified'])
 def test_expired_disabled_not_promoted(self):
  self.assertEqual(prioritize([],self.p,201),[])
  self.p['confirmedServers'][0]['mutualTrustConfirmed']=False
  self.assertEqual(prioritize([],self.p,160),[])
 def test_endpoint_changes_do_not_inherit(self):
  changed=dict(self.r,fingerprint='B'*43,host='other.net')
  rows=prioritize([changed],self.p,160);self.assertEqual(len(rows),2);self.assertFalse(rows[1]['adminConfirmed'])
 def test_claims_from_broadcast_are_not_confirmation(self):
  rows=prioritize([dict(self.r,adminConfirmed=True,mutualTrustConfirmed=True)],{},160)
  self.assertFalse(rows[0]['adminConfirmed']);self.assertEqual(rows[0]['trustPriority'],2)
if __name__=='__main__':unittest.main()
