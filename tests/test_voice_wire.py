"""Independent IMA reference and malformed wire checks; no hardware/audio recording."""
import ctypes as C, audioop, struct, zlib, random, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'ui-preview-firmware/main'
tmp=tempfile.TemporaryDirectory();lib=Path(tmp.name)/'voice.dylib'
subprocess.run(['cc','-shared','-fPIC','-Wall','-Wextra','-Werror',str(root/'voice_wire.c'),'-o',str(lib)],check=True)
c=C.CDLL(str(lib));c.radpcm_encode.argtypes=[C.c_void_p,C.c_void_p,C.c_uint16,C.POINTER(C.c_int),C.POINTER(C.c_int)]
c.radpcm_decode.argtypes=[C.c_void_p,C.c_void_p];c.radpcm_decode.restype=C.c_bool
c.voice_pack.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p,C.c_int,C.c_uint32,C.c_char_p,C.c_uint32,C.c_uint32,C.c_uint32,C.c_uint16];c.voice_pack.restype=C.c_size_t
c.voice_parse.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];c.voice_parse.restype=C.c_bool
rng=random.Random(1791);frames=[];state=(0,0)
for i in range(3):
 pcm=struct.pack('<640h',*[rng.randint(-10000,10000) for _ in range(640)])
 raw=C.create_string_buffer(328);p,index=C.c_int(state[0]),C.c_int(state[1]);c.radpcm_encode(pcm,raw,i,C.byref(p),C.byref(index))
 expected,after=audioop.lin2adpcm(pcm,2,state)
 assert raw.raw[8:]==expected and (p.value,index.value)==after
 decoded=C.create_string_buffer(1280);assert c.radpcm_decode(raw,decoded)
 reference,_=audioop.adpcm2lin(expected,2,state);assert decoded.raw==reference
 frames.append(raw.raw);state=after
out=C.create_string_buffer(1400);n=c.voice_pack(out,1400,b''.join(frames),3,17910101,b'BH1JSS',1791,1000,1000,1);assert n==1096
assert c.voice_pack(out,1400,b''.join(frames),3,17910101,b'BH1JSS',1791,1000,1240,4)==1096
assert [struct.unpack_from('<H',out.raw,64+i*344)[0] for i in range(3)]==[1,2,3]
packet=out.raw[:n];assert struct.unpack_from('<I',packet,36)[0]==zlib.crc32(packet[64:]);parsed=C.create_string_buffer(256);assert c.voice_parse(packet,n,parsed)
for cut in range(n):assert not c.voice_parse(packet,cut,parsed)
for pos in (0,30,34,36,80,1095):
 bad=bytearray(packet);bad[pos]^=128;assert not c.voice_parse(bytes(bad),len(bad),parsed)
for pos,value in [(64,0),(408,1),(73,0),(84,89)]:
 bad=bytearray(packet);bad[pos]=value;struct.pack_into('<I',bad,36,zlib.crc32(bad[64:]));assert not c.voice_parse(bytes(bad),len(bad),parsed)
# One small synthetic Opus payload only tests framing; it is not published.
body=struct.pack('<HHI',1,19,0)+struct.pack('<BH5x',1,11)+b'\xf8\xff\xfe'
op=bytearray(packet[:64]+body);struct.pack_into('<IH',op,30,len(op),1);struct.pack_into('<I',op,36,zlib.crc32(body));assert c.voice_parse(bytes(op),len(op),parsed)
print('PASS: IMA encode/decode reference, aggregate CRC, 1096 truncations, malformed frames, Opus framing')
# Short uplink batches retain the same framing, timestamp and independent decoder.
for count in (1,2):
 n=c.voice_pack(out,752,b''.join(frames[:count]),count,17910101,b'BH1JSS',1791,1000,1480,7)
 assert n==64+344*count and n+12<1024
 packet=out.raw[:n];assert c.voice_parse(packet,n,parsed)
 assert struct.unpack_from('<I',packet,36)[0]==zlib.crc32(packet[64:])
 assert [struct.unpack_from('<H',packet,64+i*344)[0] for i in range(count)]==list(range(1,count+1))
print('PASS: one/two frame uplink fits MSS with unchanged decoding')
# Protocol 6.2: emulate a stateful peer (not our per-frame decoder), including
# resynchronization after deliberate frame losses and uint16 sequence wrap.
import math
encoder_state=(0,0);peer_state=(0,0);last_frame=0xffff
for number in range(140):
 frame=(65500+number)&0xffff
 samples=[int(9500*math.sin(2*math.pi*(110+number%7)*((number*640+j)/8000))) for j in range(640)]
 pcm=struct.pack('<640h',*samples);p,index=C.c_int(encoder_state[0]),C.c_int(encoder_state[1]);raw=C.create_string_buffer(328)
 c.radpcm_encode(pcm,raw,frame,C.byref(p),C.byref(index))
 fi,recover,step,reserved,length=struct.unpack_from('<HhBBH',raw.raw)
 assert (fi,recover,step,reserved,length)==(frame,*encoder_state,0,320)
 reference,next_state=audioop.lin2adpcm(pcm,2,encoder_state)
 assert raw.raw[8:]==reference
 encoder_state=(p.value,index.value);assert encoder_state==next_state
 if number in (5,6,39):continue
 if last_frame==0xffff or fi==0 or fi!=((last_frame+1)&0xffff):peer_state=(recover,step)
 decoded,peer_state=audioop.adpcm2lin(raw.raw[8:],2,peer_state)
 independent=C.create_string_buffer(1280);assert c.radpcm_decode(raw,independent)
 assert decoded==independent.raw
 assert peer_state==encoder_state
 last_frame=fi
print('PASS: 140-frame stateful peer decoding, dropped-frame recovery and uint16 wrap match independent IMA')
