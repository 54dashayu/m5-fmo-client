from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/main.c').read_text();fn=s[s.index('static void image_draw('):s.index('static void image_changed(')]
pre=r'''
#include <stdint.h>
#include "sstv_decoder.h"
#define GALLERY_PREVIEW_WIDTH 80
#define GALLERY_PREVIEW_HEIGHT 60
static uint8_t gallery_sides[2][40*30];static int gallery_side_ready[2]={1,1};
static int page;enum{GALLERY=1};
static uint8_t gallery_buffer[80*60];static uint8_t *gallery_pixels=gallery_buffer;
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef uint16_t lv_color_t;
typedef struct{int x1,y1,x2,y2;}lv_area_t;
typedef struct{lv_area_t *clip_area;}lv_draw_ctx_t;
typedef struct{int unused;}lv_event_t;
typedef struct{int unused;}lv_obj_t;
typedef struct{const lv_area_t *blend_area;const lv_color_t *src_buf;unsigned opa,blend_mode;}lv_draw_sw_blend_dsc_t;
#define LV_OPA_COVER 255
#define LV_BLEND_MODE_NORMAL 0
static uint8_t pixels[SSTV_PREVIEW_BYTES];static uint16_t rendered[320*180];static unsigned rows;
static lv_area_t clip={0,30,319,209};static lv_draw_ctx_t context={&clip};
static lv_draw_ctx_t *lv_event_get_draw_ctx(lv_event_t*e){(void)e;return &context;}
static lv_obj_t *lv_event_get_target(lv_event_t*e){(void)e;return NULL;}
static uint8_t *sstv_rx_pixels(void){return pixels;}
static void lv_obj_get_coords(lv_obj_t*o,lv_area_t*b){(void)o;*b=(lv_area_t){0,30,319,209};}
static uint16_t lv_color_make(unsigned r,unsigned g,unsigned b){return (r>>3)<<11|(g>>2)<<5|(b>>3);}
static void lv_draw_sw_blend(lv_draw_ctx_t*c,const lv_draw_sw_blend_dsc_t*d){const lv_area_t*b=d->blend_area;assert(d->opa==255&&d->blend_mode==0&&b->x2-b->x1+1==(page==GALLERY?320:240));assert(b->y1>=c->clip_area->y1&&b->y2<=c->clip_area->y2);memcpy(rendered+(b->y1-30)*320+b->x1,d->src_buf,(b->x2-b->x1+1)*2);rows++;}
'''
tail=r'''
int main(void){for(unsigned i=0;i<sizeof(pixels);i++){pixels[i]=i*37;}
image_draw(NULL);assert(rows==180);for(unsigned y=0;y<180;y++)for(unsigned x=0;x<240;x++){uint8_t v=pixels[(y*SSTV_PREVIEW_HEIGHT/180)*SSTV_PREVIEW_WIDTH+x*SSTV_PREVIEW_WIDTH/240];assert(rendered[y*320+x+40]==lv_color_make((v>>5)*255/7,((v>>2)&7)*255/7,(v&3)*255/3));}
clip.y1=69;clip.y2=76;rows=0;image_draw(NULL);assert(rows==8);
page=GALLERY;clip.y1=30;clip.y2=209;rows=0;
memset(gallery_buffer,224,sizeof(gallery_buffer));memset(gallery_sides[0],28,sizeof(gallery_sides[0]));memset(gallery_sides[1],3,sizeof(gallery_sides[1]));image_draw(NULL);assert(rows==180);
for(unsigned y=0;y<180;y++)for(unsigned x=0;x<320;x++){
 uint16_t expected=lv_color_make(13,21,29);
 if(x>=98&&x<222&&y>=43&&y<137&&(x<100||x>=220||y<45||y>=135))expected=lv_color_make(88,214,178);
 else if(x>=100&&x<220&&y>=45&&y<135)expected=lv_color_make(255,0,0);
 else if(x>=12&&x<92&&y>=60&&y<120)expected=lv_color_make(0,255,0);
 else if(x>=228&&x<308&&y>=60&&y<120)expected=lv_color_make(0,0,255);
 assert(rendered[y*320+x]==expected);
}
clip.y1=99;clip.y2=106;rows=0;image_draw(NULL);assert(rows==8);
gallery_side_ready[0]=gallery_side_ready[1]=0;clip.y1=30;clip.y2=209;image_draw(NULL);assert(rendered[80*320+20]==lv_color_make(13,21,29));
puts("PASS M5 row rendering: RX scaling preserved; three gallery images, 120x90 centered focus/border, 80x60 sides, empty sides and clipped redraw");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+fn+tail);subprocess.run(['cc','-DCONFIG_IDF_TARGET_ESP32','-I'+str(root/'ui-preview-firmware/main'),'-Wall','-Wextra','-Werror',str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
