"""Draw production M5 glyphs even when the temporary-buffer heap cannot allocate."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/components/lvgl/src/draw/sw/lv_draw_sw_letter.c').read_text();a=s.index('static void LV_ATTRIBUTE_FAST_MEM draw_letter_normal(');b=s.index('\n#if LV_DRAW_COMPLEX && LV_USE_FONT_SUBPX',a)
pre=r'''
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define CONFIG_IDF_TARGET_ESP32 1
#define LV_ATTRIBUTE_FAST_MEM
#define LV_LOG_WARN(...) ((void)0)
#define LV_OPA_TRANSP 0
#define LV_OPA_MAX 253
#define LV_OPA_COVER 255
#define LV_DRAW_MASK_RES_CHANGED 2
typedef uint8_t lv_opa_t;typedef int lv_coord_t;
typedef struct{int x1,y1,x2,y2;}lv_area_t;
typedef struct{int x,y;}lv_point_t;
typedef struct{lv_area_t*clip_area;}lv_draw_ctx_t;
typedef struct{uint8_t opa;int color,blend_mode;}lv_draw_label_dsc_t;
typedef struct{unsigned bpp,box_w,box_h;}lv_font_glyph_dsc_t;
typedef struct{int color,blend_mode,mask_res;uint8_t opa;uint8_t*mask_buf;lv_area_t*blend_area,*mask_area;}lv_draw_sw_blend_dsc_t;
static uint8_t _lv_bpp1_opa_table[2]={0,255},_lv_bpp2_opa_table[4],_lv_bpp4_opa_table[16],_lv_bpp8_opa_table[256];
static unsigned allocations,releases,pixels;
static void *lv_mem_buf_get(unsigned n){(void)n;allocations++;return NULL;}
static void lv_mem_buf_release(void*p){(void)p;releases++;}
static void lv_memset_00(void*p,size_t n){memset(p,0,n);}
static void *_lv_refr_get_disp_refreshing(void){return NULL;}
static int lv_disp_get_hor_res(void*p){(void)p;return 320;}
static void lv_draw_sw_blend(lv_draw_ctx_t*c,const lv_draw_sw_blend_dsc_t*d){(void)c;unsigned n=(d->blend_area->x2-d->blend_area->x1+1)*(d->blend_area->y2-d->blend_area->y1+1);for(unsigned i=0;i<n;i++)assert(d->mask_buf[i]==255);pixels+=n;}
'''
post=r'''
int main(void){_lv_bpp4_opa_table[15]=255;lv_area_t clip={0,0,319,239};lv_draw_ctx_t ctx={&clip};lv_point_t pos={0,0};lv_draw_label_dsc_t d={.opa=255};lv_font_glyph_dsc_t glyph={.bpp=4,.box_w=16,.box_h=16};uint8_t data[128];memset(data,255,sizeof(data));for(int i=0;i<1000;i++)draw_letter_normal(&ctx,&d,&pos,&glyph,data);assert(pixels==256000&&!allocations&&!releases);pixels=0;clip.x1=8;clip.y1=8;draw_letter_normal(&ctx,&d,&pos,&glyph,data);assert(pixels==64&&!allocations&&!releases);puts("PASS real glyph rasterizer: repeated/clipped 4-bpp text with unavailable heap, zero temporary allocations");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+s[a:b]+post);subprocess.run(['cc','-Wall','-Wextra','-Werror',str(p),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
