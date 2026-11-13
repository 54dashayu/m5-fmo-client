"""Exercise actual packed UI settings persistence and failure handling."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/main.c').read_text()
fn=s[s.index('static bool save_settings'):s.index('static display_idle_t')]
pre='''#include "call_indicator.h"
#include "key_layout.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#define ESP_OK 0
#define NVS_READWRITE 1
static bool ui_flashing_enabled;static uint8_t rotation;static bool breathing=true,ptt_latch=true,ui_nv_ready;static int ui_nv;static char settings_notice[64];
static unsigned ui_key_layout;static int keys;static void buttons_block(int*k){(void)k;}
static uint32_t durable[2]={12,0},pending[2];static bool exists[2],staged[2],fail_write,fail_commit;
static int slot(const char*k){return !strcmp(k,"key_layout");}
static int nvs_open(const char*a,int b,int*c){(void)a;(void)b;*c=1;return 0;}
static int nvs_set_u32(int a,const char*b,uint32_t c){(void)a;pending[slot(b)]=c;staged[slot(b)]=true;return fail_write;}
static int nvs_commit(int a){(void)a;if(!fail_commit)for(int i=0;i<2;i++)if(staged[i]){durable[i]=pending[i];exists[i]=true;staged[i]=false;}return fail_commit;}
static int nvs_get_u32(int a,const char*b,uint32_t*c){(void)a;int i=slot(b);if(!exists[i])return 1;*c=durable[i];return 0;}
'''
tail='''int main(void){load_settings();assert(rotation==0&&breathing&&ptt_latch);
for(int rot=0;rot<4;rot++)for(int pulse=0;pulse<2;pulse++)for(int latch=0;latch<2;latch++){assert(save_settings(rot,pulse,latch));rotation=0;breathing=ptt_latch=false;load_settings();assert(rotation==rot&&breathing==pulse&&ptt_latch==latch);assert(ui_flashing_enabled==(!FMO_HAS_SIDE_LEDS||!pulse));}
for(unsigned i=0;i<6;i++){assert(save_key_layout(i));ui_key_layout=99;load_settings();assert(ui_key_layout==i);}assert(!save_key_layout(6));
assert(save_settings(0,true,true));fail_write=true;assert(!save_settings(1,false,false));assert(rotation==0&&breathing&&ptt_latch);fail_write=false;fail_commit=true;assert(!save_settings(2,false,false));load_settings();assert(rotation==0&&breathing&&ptt_latch);fail_commit=false;assert(save_key_layout(2));fail_commit=true;assert(!save_key_layout(3));assert(ui_key_layout==2);load_settings();assert(ui_key_layout==2);durable[1]=99;load_settings();assert(ui_key_layout==0);
puts("PASS UI settings: six key schemes, invalid fallback, 16 restart combinations, write/commit failure preserves runtime settings");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'t.c';p.write_text(pre+fn+tail)
 for leds in (0,1):
  subprocess.run(['cc','-DFMO_HAS_SIDE_LEDS='+str(leds),'-I'+str(r/'ui-preview-firmware/main'),'-Wall','-Wextra','-Werror',str(p),'-o',d+'/t'],check=True);subprocess.run([d+'/t'],check=True)
