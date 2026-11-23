"""Execute production touch controller: PTT hold, release, drag, wake, errors."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'ui-preview-firmware/main/main.c').read_text()
code=s[s.index('static void cyd_controls(void)'):s.index('\n#endif',s.index('static void cyd_controls(void)'))]
pre=r'''#include "cyd28_plus_touch.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
typedef int lv_obj_t;
#define LV_OPA_COVER 255
#define LV_OBJ_FLAG_SCROLLABLE 1
#define LV_TEXT_ALIGN_CENTER 0
static int ui_small;
static int obj;
#define lv_layer_top() (&obj)
#define lv_obj_create(a) (&obj)
#define lv_label_create(a) (&obj)
#define lv_obj_set_size(...) ((void)0)
#define lv_obj_set_pos(...) ((void)0)
#define lv_obj_set_style_pad_all(...) ((void)0)
#define lv_obj_set_style_bg_color(...) ((void)0)
#define lv_obj_set_style_text_color(...) ((void)0)
#define lv_obj_set_style_bg_opa(...) ((void)0)
#define lv_obj_clear_flag(...) ((void)0)
#define lv_obj_set_style_text_align(...) ((void)0)
#define lv_obj_set_style_text_font(...) ((void)ui_small)
#define lv_label_set_text(a,b) ((void)(a),(void)(b))
enum {HOME,MENU,GALLERY,DISCOVER};
static int page,selection;static unsigned discovered_revision;static atomic_int gallery_work;
static struct {bool sleeping;} screen_sleep;
static struct {int64_t last_activity;} display_idle;
static int64_t now;static int px=270,py=220,starts,stops,actions;static bool pen,valid=true;
static int64_t esp_timer_get_time(void){return now;}
bool cyd_touch_poll(int*x,int*y,bool*d){*x=px;*y=py;*d=pen;return valid;}
static int cyd_content_rows(void){return 0;}
static int cyd_content_kind(void){return page==HOME?CYD_CONTENT_HOME:CYD_CONTENT_NONE;}
static bool voice_ptt(bool d){if(d)starts++;else stops++;return true;}
static void cyd_activate(int t){(void)t;actions++;}
static void refresh(void){}
'''
post=r'''
static void tick(bool d,int ms){pen=d;now+=ms*1000;cyd_controls();}
int main(void){
 tick(true,0);assert(starts==0);tick(true,59);assert(starts==0);tick(true,1);assert(starts==1);tick(false,1);assert(stops==1&&actions==0);
 tick(true,100);tick(false,20);assert(starts==1);
 tick(true,100);px=100;tick(true,70);tick(false,1);assert(starts==1);
 px=270;screen_sleep.sleeping=true;tick(true,100);tick(true,70);tick(false,1);assert(starts==1);
 tick(true,100);tick(true,70);assert(starts==2);valid=false;tick(true,1);assert(stops==2);valid=true;tick(false,1);
 page=MENU;tick(true,100);tick(true,70);assert(starts==3);page=HOME;tick(true,1);assert(stops==3);tick(false,1);
 puts("PASS actual PTT: debounce, release, slide cancellation, wake suppression, read failure, page change, no tap leakage");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'ptt.c';p.write_text(pre+code+post)
 subprocess.run(['cc','-Wall','-Wextra','-I'+str(r/'ui-preview-firmware/components/lv_porting'),str(p),'-o',td+'/ptt'],check=True)
 subprocess.run([td+'/ptt'],check=True)
