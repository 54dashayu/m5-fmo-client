"""Exercise actual packed UI settings persistence and failure handling."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/main.c').read_text()
fn=s[s.index('static bool save_settings'):s.index('static display_idle_t')]
pre='''#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#define ESP_OK 0
#define NVS_READWRITE 1
static uint8_t rotation;static bool breathing=true,ptt_latch=true,ui_nv_ready;static int ui_nv;static char settings_notice[64];
static uint32_t durable=12,pending;static bool exists,fail_write,fail_commit;
static int nvs_open(const char*a,int b,int*c){(void)a;(void)b;*c=1;return 0;}
static int nvs_set_u32(int a,const char*b,uint32_t c){(void)a;(void)b;pending=c;return fail_write;}
static int nvs_commit(int a){(void)a;if(!fail_commit){durable=pending;exists=true;}return fail_commit;}
static int nvs_get_u32(int a,const char*b,uint32_t*c){(void)a;(void)b;if(!exists)return 1;*c=durable;return 0;}
'''
tail='''int main(void){load_settings();assert(rotation==0&&breathing&&ptt_latch);
for(int rot=0;rot<4;rot++)for(int pulse=0;pulse<2;pulse++)for(int latch=0;latch<2;latch++){assert(save_settings(rot,pulse,latch));rotation=0;breathing=ptt_latch=false;load_settings();assert(rotation==rot&&breathing==pulse&&ptt_latch==latch);}
assert(save_settings(0,true,true));fail_write=true;assert(!save_settings(1,false,false));assert(rotation==0&&breathing&&ptt_latch);fail_write=false;fail_commit=true;assert(!save_settings(2,false,false));load_settings();assert(rotation==0&&breathing&&ptt_latch);puts("PASS UI settings: 16 restart combinations, write/commit failure preserves runtime settings");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';p.write_text(pre+fn+tail)
 subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',d+'/t'],check=True);subprocess.run([d+'/t'],check=True)
