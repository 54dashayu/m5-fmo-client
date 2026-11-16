"""Execute the production receive-page refresh with LVGL's hidden-flag semantics."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/main.c').read_text();a=s.index(' }else if(page==SSTV){',s.index('static void refresh('));b=s.index(' }else if(page==SSTV_TX_CONFIRM){',a);body=s[a+len(' }else if(page==SSTV){'):b]
pre=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#define FMO_SSTV_STREAMING 1
#define FMO_FULL_SSTV 1
#define FMO_WIDTH 320
#define LV_OBJ_FLAG_HIDDEN 1
#define LV_TEXT_ALIGN_RIGHT 1
static int image_obj,other;static int *sstv_image=&image_obj,*title=&other,*brand=&other,*footer=&other,*lines[5]={&other};
static bool hidden=true,sstv_start_failed,sstv_redraw_pending;static unsigned sstv_generation=2,sstv_lines=100,sstv_saved;static int sstv_status;static char image_notice[20];

enum{SSTV_RECEIVING,SSTV_COMPLETE,SSTV_INTERRUPTED};
static unsigned image_invalidations,overlap_moves;
static void ensure_image(void){}
static bool lv_obj_has_flag(void*o,int f){(void)f;return o==sstv_image&&hidden;}
// LVGL invalidates whenever clear_flag(HIDDEN) is called, even if already visible.
static void lv_obj_clear_flag(void*o,int f){(void)f;if(o==sstv_image){image_invalidations++;hidden=false;}}
static void lv_obj_add_flag(void*o,int f){(void)o;(void)f;}
static void image_changed(void){image_invalidations++;}
static bool sstv_rx_snapshot(int*s,unsigned*l,unsigned*g){(void)s;(void)l;(*g)++;return true;}
static void lv_label_set_text(void*o,const char*s){(void)o;(void)s;}
static void lv_label_set_text_fmt(void*o,const char*s,...){(void)o;(void)s;}
static void lv_obj_set_y(void*o,int y){if(o==lines[0]&&y>=30&&y<210)overlap_moves++;}
static void lv_obj_set_pos(void*o,int x,int y){(void)x;lv_obj_set_y(o,y);}
static void lv_obj_set_width(void*o,int w){(void)o;(void)w;}
static void lv_obj_set_style_text_align(void*o,int a,int p){(void)o;(void)a;(void)p;}
static int lv_disp_get_ver_res(void*o){(void)o;return 240;}
static const char*sstv_store_error(void){return 0;}
static void refresh_receive(void){
'''
tail=r'''
}
int main(void){refresh_receive();assert(image_invalidations==1&&!overlap_moves);for(unsigned i=0;i<400;i++)refresh_receive();assert(image_invalidations==1&&!overlap_moves);sstv_status=SSTV_COMPLETE;for(unsigned i=0;i<100;i++)refresh_receive();assert(image_invalidations==1&&!overlap_moves);puts("PASS actual RX refresh: first reveal only, 500 status updates preserve high-resolution pixels and keep labels outside image");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(pre+body+tail);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True);subprocess.run([d+'/test'],check=True)
