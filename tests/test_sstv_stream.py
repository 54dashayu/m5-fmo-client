from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];m=r/'ui-preview-firmware/main'
with tempfile.TemporaryDirectory() as td:
 d=Path(td);s=(r/'tests/test_sstv.c').read_text();s=s.replace('#include <stdlib.h>','#include <stdlib.h>\n#include <string.h>')
 s=s.replace('static sstv_decoder_t d;', '''static sstv_decoder_t d;
static uint8_t output_frame[SSTV_BYTES];
static unsigned nextrow;
static bool begin(void *ctx){(void)ctx;nextrow=0;memset(output_frame,0,sizeof(output_frame));return true;}
static bool rows(void *ctx,unsigned row,const uint8_t *p){(void)ctx;assert(row==nextrow);memcpy(output_frame+row*640,p,1280);nextrow+=2;return true;}
static void reset(void){sstv_decoder_init(&d);d.begin=begin;d.rows=rows;}
''').replace('d.image[at]','output_frame[at]').replace('d.image[at+1]','output_frame[at+1]').replace('sstv_decoder_init(&d);','reset();')
 s=s.replace('static void reset(void){reset();','static void reset(void){sstv_decoder_init(&d);')
 (d/'test.c').write_text(s)
 subprocess.run(['cc','-DCONFIG_IDF_TARGET_ESP32','-std=c11','-Wall','-Wextra','-Werror','-I'+str(m),str(d/'test.c'),str(m/'sstv_decoder.c'),str(m/'voice_wire.c'),'-lm','-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
 print('Streaming decoder size:', 'two rows + DSP; full-resolution color and transport tests passed')
