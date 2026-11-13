from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/main.c').read_text();fn=s[s.index('static void image_draw('):s.index('static void image_changed(')]
pre=r'''
#include <stdint.h>
#define SSTV_PREVIEW_WIDTH 120
#define SSTV_PREVIEW_HEIGHT 90
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t lv_color_t;
typedef struct{int x1,y1,x2,y2;}lv_area_t;
typedef struct{lv_area_t *clip_area;}lv_draw_ctx_t;
typedef struct{int unused;}lv_event_t;
typedef struct{int unused;}lv_obj_t;
typedef struct{struct{int cf,w,h;}header;size_t data_size;const uint8_t*data;}lv_img_dsc_t;
typedef int lv_draw_img_dsc_t;
#define LV_IMG_CF_TRUE_COLOR 1
static uint8_t pixels[120*90];static uint16_t rendered[240*180];static unsigned rows;
static lv_area_t clip={40,30,279,209};static lv_draw_ctx_t context={&clip};
static lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t*e){(void)e;return &context;}
static lv_obj_t *lv_event_get_target(lv_event_t*e){(void)e;return NULL;}
static uint8_t *sstv_rx_pixels(void){return pixels;}
static void lv_obj_get_coords(lv_obj_t*o,lv_area_t*b){(void)o;*b=(lv_area_t){40,30,279,209};}
static void lv_draw_img_dsc_init(lv_draw_img_dsc_t*d){*d=0;}
static uint16_t lv_color_make(unsigned r,unsigned g,unsigned b){return (r>>3)<<11|(g>>2)<<5|(b>>3);}
static void lv_draw_img(lv_draw_ctx_t*c,lv_draw_img_dsc_t*s,lv_area_t*b,const lv_img_dsc_t*i){(void)s;assert(i->header.w==240&&i->header.h==1&&i->data_size==480);assert(b->y1>=c->clip_area->y1&&b->y2<=c->clip_area->y2);memcpy(rendered+(b->y1-30)*240,i->data,480);rows++;}
'''
tail=r'''
int main(void){for(unsigned i=0;i<sizeof(pixels);i++){pixels[i]=i*37;}
image_draw(NULL);assert(rows==180);for(unsigned y=0;y<180;y++)for(unsigned x=0;x<240;x++){uint8_t v=pixels[(y*90/180)*120+x*120/240];assert(rendered[y*240+x]==lv_color_make((v>>5)*255/7,((v>>2)&7)*255/7,(v&3)*255/3));}
clip.y1=69;clip.y2=76;rows=0;image_draw(NULL);assert(rows==8);
puts("PASS M5 row rendering: 240x180 scaling, all rows present and clipped redraw only");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+fn+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
