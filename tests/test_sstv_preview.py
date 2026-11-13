"""Check actual S3 in-place RGB332 expansion without overwriting unread pixels."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/main.c').read_text();fn=s[s.rindex('static void image_changed('):s.rindex('static void ensure_image(')]
pre=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define SSTV_WIDTH 320
#define SSTV_HEIGHT 240
#define SSTV_PIXELS 76800
typedef struct{uint16_t full;}lv_color_t;
static lv_color_t display_storage[240*180];
static lv_color_t *image_display=display_storage;
static uint8_t buffer[SSTV_PIXELS*2+16];static int sstv_image_desc,*sstv_image=&sstv_image_desc,invalidated;
static uint8_t*sstv_rx_pixels(void){return buffer;}
typedef lv_color_t color;
static color lv_color_make(unsigned r,unsigned g,unsigned b){return (color){(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3))};}
static void lv_img_cache_invalidate_src(void*p){(void)p;}
static void lv_obj_invalidate(void*p){(void)p;invalidated++;}
'''
tail=r'''
int main(void){memset(buffer,0xa5,sizeof(buffer));for(int i=0;i<SSTV_PIXELS;i++){unsigned v=(i*73+19)&65535;buffer[i*2]=v;buffer[i*2+1]=v>>8;}image_changed();
for(int y=0;y<180;y++)for(int x=0;x<240;x++){unsigned at=(y*240/180)*320+x*320/240,v=(at*73+19)&65535;unsigned r=((v>>11)&31)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;assert(image_display[y*240+x].full==((r>>3)<<11|(g>>2)<<5|(b>>3)));}
for(int i=0;i<SSTV_PIXELS;i++){unsigned v=(i*73+19)&65535;assert(buffer[i*2]==(v&255)&&buffer[i*2+1]==v>>8);}
for(unsigned i=SSTV_PIXELS*2;i<sizeof(buffer);i++){assert(buffer[i]==0xa5);}
assert(invalidated==1);puts("PASS 240x180 separate preview: correct full-frame sampling, unchanged source and guard");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'p.c';p.write_text(pre+fn+tail);subprocess.run(['cc','-DFMO_FULL_SSTV','-Wall','-Wextra','-Werror',str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
