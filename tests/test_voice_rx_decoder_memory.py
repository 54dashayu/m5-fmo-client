"""Exercise the production RX codec transition and allocation-failure path."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/voice.c').read_text()
code=s[s.index('static int decode_receive_frame('):s.index('static bool sstv_start(')]
pre=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <assert.h>
static int storage;static void *decoder=&storage;
static bool active,valid=true;static unsigned frees,creates,decodes;
#ifdef FMO_SSTV_STREAMING
static bool fail;
static bool sstv_rx_active(void){return active;}
static void opus_decoder_destroy(void*p){assert(p==&storage);frees++;}
static void *opus_decoder_create(int rate,int channels,int*error){assert(rate==8000&&channels==1);creates++;*error=fail?-1:0;return fail?NULL:&storage;}
#endif
static int opus_decode(void*p,const uint8_t*d,int n,int16_t*out,int cap,int fec){assert(p==&storage&&d&&n==10&&out&&cap==640&&!fec);decodes++;return 160;}
static bool radpcm_decode(const uint8_t*d,int16_t*out){assert(d&&out);return valid;}
'''
tail=r'''
int main(void){uint8_t data[328]={0};int16_t pcm[640];
assert(decode_receive_frame(2,data,328,pcm)==640&&decoder);active=true;
for(int i=0;i<463;i++)assert(decode_receive_frame(2,data,328,pcm)==640);
#ifdef FMO_SSTV_STREAMING
assert(!decoder&&frees==1&&!creates);fail=true;
assert(decode_receive_frame(1,data,10,pcm)==-1&&!decoder&&!decodes);fail=false;
assert(decode_receive_frame(1,data,10,pcm)==160&&decoder&&creates==2);
assert(decode_receive_frame(1,data,10,pcm)==160&&creates==2);
assert(decode_receive_frame(2,data,328,pcm)==640&&!decoder&&frees==2);
active=false;assert(decode_receive_frame(1,data,10,pcm)==160&&decoder);
#else
assert(decoder&&!frees&&!creates);assert(decode_receive_frame(1,data,10,pcm)==160);
#endif
valid=false;assert(decode_receive_frame(2,data,328,pcm)==-1);
return 0;}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+code+tail)
 for flags in ([],['-DFMO_SSTV_STREAMING']):
  subprocess.run(['cc','-Wall','-Wextra','-Werror',*flags,str(p),'-o',td+'/test'],check=True)
  subprocess.run([td+'/test'],check=True)
print('PASS RX codec lifetime: RADPCM, Opus restoration, allocation failure/retry, non-M5 unchanged')
