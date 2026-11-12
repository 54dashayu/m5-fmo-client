"""FCRD boundaries and identity binding; no live CA or private keys."""
import importlib.util,json,struct,zlib,unittest
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('delivery',root/'services/bi4tes-delivery/bi4tes_delivery.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class Delivery(unittest.TestCase):
 def setUp(self):
  self.root={'subject':{'publicKey':m.ROOT_KEY}}
  self.user={'subject':{'publicKey':'local','callsign':'BH1JSS'}}
 def frame(self, extra=b''):
  body=b''
  for tag,data in [(2,self.user),(3,{'type':'intermediateCA'})]:
   raw=json.dumps(data).encode();body+=struct.pack('<BH',tag,len(raw))+raw
  body+=extra
  raw=b'FCRD'+struct.pack('<II',2,len(body))+body
  return raw+struct.pack('<I',zlib.crc32(raw))
 def test_valid(self):
  b=m.unpack(self.frame(),self.root,'local','BH1JSS')
  self.assertEqual(b['userCert'],self.user)
 def test_identity(self):
  for pk,call in [('other','BH1JSS'),('local','BH1OFJ')]:
   with self.assertRaises(ValueError):m.unpack(self.frame(),self.root,pk,call)
 def test_integrity(self):
  raw=self.frame()
  for damaged in [raw[:-1],raw[:20]+bytes([raw[20]^1])+raw[21:],b'BAD!'+raw[4:]]:
   with self.assertRaises(ValueError):m.unpack(damaged,self.root,'local','BH1JSS')
 def test_tlv(self):
  for extra in [b'\x02\x02\x00{}',b'\x09',b'\x09\xff\xff']:
   with self.assertRaises(ValueError):m.unpack(self.frame(extra),self.root,'local','BH1JSS')
 def test_root_pin(self):
  with self.assertRaises(ValueError):m.unpack(self.frame(),{'subject':{'publicKey':'other'}},'local','BH1JSS')
unittest.main()
