"""Exercise the actual portable firmware verifier against independent Python signatures."""
import os, copy, ctypes as C, json, subprocess, sys, tempfile, time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from fmo_identity import ephemeral_test_chain, unb64, sign_cert
import cbor2
base=Path(__file__).resolve().parents[1]/'ui-preview-firmware'
cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory() as td:
 libpath=Path(td)/'wire.dylib'
 subprocess.run(['cc','-shared','-fPIC','-O2','-o',str(libpath),'-I'+str(cjson),'-I'+str(base/'components/monocypher'),str(base/'main/cert_wire.c'),str(cjson/'cJSON.c'),str(base/'components/monocypher/monocypher.c'),str(base/'components/monocypher/monocypher-ed25519.c'),'-lm'],check=True)
 lib=C.CDLL(str(libpath));lib.cJSON_Parse.argtypes=[C.c_char_p];lib.cJSON_Parse.restype=C.c_void_p;lib.cJSON_Delete.argtypes=[C.c_void_p]
 lib.wire_validate_bundle.argtypes=[C.c_void_p,C.c_char_p,C.c_char_p,C.c_int64];lib.wire_validate_bundle.restype=C.c_char_p
 now=int(time.time());r,i,u,ik,dk=ephemeral_test_chain(now);good=dict(rootCert=r,intermediateCert=i,userCert=u);pk=unb64(u['subject']['publicKey'])
 def check(name,b,expect=True,key=pk,call=b'BH1JSS',clock=now):
  obj=lib.cJSON_Parse((b if isinstance(b,str) else json.dumps(b)).encode());assert obj
  err=lib.wire_validate_bundle(obj,key,call,clock);lib.cJSON_Delete(obj)
  assert (err is None)==expect,(name,err)
  print('PASS',name)
 check('valid chain',good);check('wrong device',good,False,key=bytes(32));check('wrong callsign',good,False,call=b'BI4TES');check('expired',good,False,clock=now+4000);check('not yet valid',good,False,clock=now-100);check('clock pending permits storage',good,clock=0)
 for name,path,value in [('tamper signature',['userCert','signature'],'A'*86),('issuer mismatch',['userCert','issuerSn'],5),('path length',['rootCert','extensions','pathLen'],0),('no CN',['intermediateCert','extensions','issuingCountries'],['US']),('fractional uid',['userCert','subject','uid'],1.5),('duplicate uid',['userCert','subject','uid'],1791)]:
  b=copy.deepcopy(good);target=b
  for p in path[:-1]:target=target[p]
  target[path[-1]]=value;check(name,b,False)
 b=copy.deepcopy(good);b['userCert']=sign_cert({**u,'subject':{**u['subject'],'uid':910000999}},ik);check('signed uid outside range',b,False)
 check('duplicate JSON field',json.dumps(good).replace('"type": "userCert"','"type": "userCert", "type": "userCert"'),False)
 lib.wire_request_tbs.argtypes=[C.c_void_p,C.c_size_t,C.c_char_p,C.c_char_p,C.c_char_p];lib.wire_request_tbs.restype=C.c_size_t
 out=C.create_string_buffer(256);n=lib.wire_request_tbs(out,256,b'BH1JSS',pk,b'nonce')
 assert out.raw[:n]==cbor2.dumps(['FMO-M5',1,'certificateRequest','BH1JSS',pk,'nonce'],canonical=True)
 print('PASS request canonical CBOR')

 lib.wire_login_tbs.argtypes=[C.c_void_p,C.c_size_t,C.c_int64,C.c_char_p,C.c_char_p,C.c_int,C.c_char_p,C.c_int64,C.c_char_p];lib.wire_login_tbs.restype=C.c_size_t
 sfp=bytes(range(32));ufp=bytes(reversed(range(32)));out=C.create_string_buffer(512)
 n=lib.wire_login_tbs(out,512,1791,b'BH1JSS',b'120.27.244.89',28883,sfp,now,ufp)
 assert out.raw[:n]==cbor2.dumps(['FMO',4,'serverAuthorizerReqHttp',1791,'BH1JSS',1791,'user','120.27.244.89',28883,sfp,now,ufp],canonical=True)
 print('PASS login canonical CBOR')
