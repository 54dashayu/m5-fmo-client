import importlib.util,unittest
from pathlib import Path
s=importlib.util.spec_from_file_location('catalog',Path(__file__).resolve().parents[1]/'discovery-service/catalog.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class Test(unittest.TestCase):
 def row(self,**kw):return dict(uid=1791,callsign='BH1JSS',stationName='测试',stationUrl='example.net',stationPort=28883,certFingerprint='A'*43,lastSeen='2026-11-09T00:00:00Z',verifiedAt='2026-11-09T00:00:00Z',lastType='STATION',certStatus='Ok',sigStatus='Valid',**kw)
 def runrows(self,rows):return m.normalize({'status':{'connected':True},'identities':rows},1794182400)['servers']
 def test_validate(self):
  x=self.row();self.assertTrue(self.runrows([x])[0]['sourceVerified'])
  for k,v in [('sigStatus','Invalid'),('certStatus','IssuerUnknown'),('lastType','VOCAL')]:
   y={**x,k:v};self.assertFalse(self.runrows([y])[0]['sourceVerified'])
  for k,v in [('stationUrl','127.0.0.1'),('stationUrl','http://example.net'),('stationPort',True),('uid',0),('lastSeen','2020-01-01T00:00:00Z')]:self.assertEqual(self.runrows([{**x,k:v}]),[])
  self.assertEqual(len(self.runrows([x,x])),1)
  self.assertNotIn('lat',self.runrows([{**x,'lat':42}])[0])
if __name__=='__main__':unittest.main()
