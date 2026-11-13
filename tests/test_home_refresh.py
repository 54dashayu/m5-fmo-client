from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];m=r/'ui-preview-firmware/main';lv=m.parent/'components/lvgl';s=(m/'main.c').read_text()
basic=s[s.index('static lv_obj_t *label('):s.index('static void build_wide_home(')]
helper=s[s.index('#ifdef CONFIG_FMO_BOARD_XINGZHI\n/* Avoid restarting layout/animations'):s.index('static void refresh_home(')]
refresh=s[s.index('static void refresh_home('):s.index('static void go(')]
home=(m/'boards/xingzhi/home.inc').read_text()
pre=r'''
#include "lvgl.h"
#include "device_web.h"
#include "voice.h"
#include "key_layout.h"
#include "call_indicator.h"
#include "utf8_name.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define FMO_WIDTH 240
#define FMO_CALL_PREFIX "E."
#define FMO_BOARD_TITLE "FMO / S3"
#define FMO_VERSION "test"
#define FMO_FULL_SSTV 1
#define CONFIG_FMO_BOARD_XINGZHI 1
extern const lv_font_t font20,history_font,status_font,ui_small,ui_call,ui_clock,relay_font;
typedef struct{char name[65],call[8];uint32_t uid;}relay_t;
static lv_obj_t *title,*lines[5],*footer,*brand,*a_hint,*c_hint,*home,*battery_label,*battery_fill,*local_call,*net,*caption,*badge,*call_label,*hint,*relay_name,*relay_id,*home_notice,*last_call,*previous_call,*b_hint,*bars[9],*call_card;
static bool ui_flashing_enabled=true,show_clock=false,ptt_latch=true;static unsigned ui_key_layout=4,ui_call_rgb,ui_call_opacity;static char held_call[13]="AI-BH1JSS";static int64_t now;
static void battery_view(int*p,bool*c){*p=100;*c=false;}
static int64_t esp_timer_get_time(void){return now;}
static bool relay_glyph(uint32_t c){(void)c;return true;}
static lv_color_t pixels[240*240],draw[240*20],expected[240*240];static unsigned long pixels_flushed;
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++){pixels[y*240+x]=*p++;pixels_flushed++;}lv_disp_flush_ready(d);}
'''
post=r'''
int main(void){lv_init();static lv_disp_draw_buf_t buf;lv_disp_draw_buf_init(&buf,draw,NULL,240*20);static lv_disp_drv_t drv;lv_disp_drv_init(&drv);drv.hor_res=drv.ver_res=240;drv.draw_buf=&buf;drv.flush_cb=flush;lv_disp_drv_register(&drv);build_compact_home();lv_obj_clear_flag(home,LV_OBJ_FLAG_HIDDEN);
device_view_t v={.online=true,.mqtt_online=true,.call="BH1JSS"};voice_view_t a={.rx=true,.caller="AI-BH1JSS/P",.last_caller="AI-BH1JSS",.previous_caller="BA1ABC",.volume=100};relay_t r={.name="test",.call="BH1JSS",.uid=1791};int min_offset=0;
for(int i=0;i<1200;i++){now=i*10000LL;lv_tick_inc(10);if(i%15==0)refresh_home(&v,&a,&r);lv_timer_handler();if(((lv_label_t*)call_label)->offset.x<min_offset)min_offset=((lv_label_t*)call_label)->offset.x;}
assert(min_offset<0);assert(!strcmp(lv_label_get_text(call_label),"AI-BH1JSS/P"));assert(!strcmp(lv_label_get_text(last_call),"AI-BH1JSS"));assert(!strcmp(lv_label_get_text(previous_call),"BA1ABC"));assert(((lv_label_t*)call_label)->offset.y==0);assert(lv_obj_get_height(call_label)==44);
v.online=false;refresh_home(&v,&a,&r);lv_obj_update_layout(net);assert(!strcmp(lv_label_get_text(net),"未联网"));assert(lv_obj_get_height(net)==ui_small.line_height);lv_point_t net_size;lv_txt_get_size(&net_size,"未联网",&ui_small,0,0,240,0);assert(net_size.x<=lv_obj_get_width(net));unsigned long dirty=pixels_flushed;lv_refr_now(NULL);memcpy(expected,pixels,sizeof(pixels));lv_obj_invalidate(lv_scr_act());lv_refr_now(NULL);assert(!memcmp(expected,pixels,sizeof(pixels)));printf("PASS actual S3 home: scroll minimum=%d; 12s dirty pixels=%lu; current/history labels intact; partial redraw matches full\n",min_offset,dirty);}
'''
with tempfile.TemporaryDirectory() as td:
 d=Path(td);(d/'esp_err.h').write_text('typedef int esp_err_t;\n');(d/'test.c').write_text(pre+basic+home+helper+refresh+post)
 cmd=['cc','-O1','-w','-DLV_CONF_SKIP','-DLV_MEM_CUSTOM=1','-DLV_COLOR_DEPTH=16','-DLV_COLOR_16_SWAP=1','-DLV_IMG_CACHE_DEF_SIZE=0','-I'+td,'-I'+str(lv),'-I'+str(lv.parent),'-I'+str(m),str(d/'test.c'),*[str(p) for p in (lv/'src').rglob('*.c')],*[str(p) for p in (m/'fonts').glob('*.c')],'-lm','-o',td+'/test'];subprocess.run(cmd,check=True);subprocess.run([td+'/test'],check=True,timeout=30)
