"""Independent CBOR and Ed25519 checks of the actual embedded beacon encoder."""
from pathlib import Path
import ctypes as C, os, subprocess, tempfile, sys, json, time, hashlib
import cbor2
from cryptography.hazmat.primitives.serialization import Encoding, PrivateFormat, NoEncryption
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from fmo_identity import ephemeral_test_chain, unb64, tbs
m=r/'ui-preview-firmware/main';b=r/'ui-preview-firmware/components';cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
wrapper='''
#include "station_beacon.h"
#include "monocypher-ed25519.h"
#include <string.h>
size_t probe(char*out,const char*json,const unsigned char*seed_in,int mask,int ssid,double lat,double lon,beacon_hash_fn hash){
 cJSON*u=cJSON_Parse(json);station_settings_t s;station_defaults(&s);s.has_location=true;s.latitude=lat;s.longitude=lon;s.frequency=438.5;s.aprs_ssid=ssid;
 if(mask&1)s.height=15;if(mask&2)strcpy(s.rig,"FT-5DR");if(mask&4)strcpy(s.antenna,"J天线");
 unsigned char seed[32],sk[64],pk[32];memcpy(seed,seed_in,32);crypto_ed25519_key_pair(sk,pk,seed);
 size_t n=station_beacon(out,&s,u,sk,1800000000,hash);cJSON_Delete(u);return n;
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'probe.c').write_text(wrapper)
 subprocess.run(['cc','-shared','-fPIC','-O2','-I'+str(m),'-I'+str(cj),'-I'+str(b/'monocypher'),'-I'+str(b/'fmo_board'),str(p/'probe.c'),*[str(m/f) for f in ['station_beacon.c','station_settings.c','cert_wire.c']],str(cj/'cJSON.c'),str(b/'monocypher/monocypher.c'),str(b/'monocypher/monocypher-ed25519.c'),'-lm','-o',str(p/'probe.dylib')],check=True)
 lib=C.CDLL(str(p/'probe.dylib'));H=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_size_t,C.c_void_p)
 @H
 def digest(src,n,out):C.memmove(out,hashlib.sha256(C.string_at(src,n)).digest(),32);return True
 lib.probe.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.c_int,C.c_int,C.c_double,C.c_double,H];lib.probe.restype=C.c_size_t
 _,_,u,ik,dk=ephemeral_test_chain(int(time.time()));seed=dk.private_bytes(Encoding.Raw,PrivateFormat.Raw,NoEncryption());raw=json.dumps(u).encode()
 for mask in range(8):
  for ssid in [0,15]:
   out=C.create_string_buffer(513);n=lib.probe(out,raw,seed,mask,ssid,89.999999,-179.999999,digest);assert 0<n<=512
   packet=out.raw[:n].decode();assert packet.endswith('\r\n');assert packet.startswith(('BH1JSS-15' if ssid else 'BH1JSS')+'>');assert '=9000.00N/18000.00WI ' in packet
   fields=dict(x.split(':',1) for x in packet.strip().split(',')[3:]);blob=unb64(fields['CERT']);decoded=cbor2.loads(blob)
   assert len(decoded)==10 and blob==cbor2.dumps(cbor2.loads(tbs(u))+[unb64(u['signature'])],canonical=True)
   ik.public_key().verify(decoded[-1],tbs(u))
   values=['FMO',4,'BEACON','BH1JSS',ssid,'9000.00N','18000.00W',hashlib.sha256(blob).digest(),'438.5000']
   if mask&1:values.append(15)
   if mask&2:values.append('FT-5DR')
   if mask&4:values.append('J天线')
   values.append(1800000000//600);dk.public_key().verify(unb64(fields['SIG']),cbor2.dumps(values,canonical=True))
 for lat,lon in [(91,0),(0,181),(float('nan'),0)]:
  assert not lib.probe(C.create_string_buffer(513),raw,seed,0,0,lat,lon,digest)
 print('PASS actual BEACON: full signed certificate hash, independent signature, 16 optional/SSID combinations, boundary rounding, invalid coordinates, 512-byte bound')
