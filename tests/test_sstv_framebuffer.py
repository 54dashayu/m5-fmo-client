"""Actual LVGL framebuffer regression: exact strips survive later refreshes."""
from pathlib import Path
import subprocess
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/main.c').read_text();draw=s[s.index('static void image_draw('):s.index('static void image_changed(')]
a=s.index(' }else if(page==SSTV){',s.index('static void refresh('));b=s.index(' }else if(page==SSTV_TX_CONFIRM){',a);body=s[a+len(' }else if(page==SSTV){'):b]
strip=s[s.index('static void image_changed_strip('):s.index('static void ensure_image(')]
pre=r'''
#include "lvgl.h"
#include "src/draw/sw/lv_draw_sw_blend.h"
#include "sstv_decoder.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define FMO_WIDTH 320
static int page;enum{GALLERY=1};static uint8_t gallery_pixels[4800],gallery_sides[2][1200],pixels[SSTV_PREVIEW_BYTES];static bool gallery_side_ready[2],sstv_redraw_pending;
static unsigned high_first,high_count,sharp_reads,low_reads;
static uint16_t native_pixel(unsigned x,unsigned y){return (((x*3+y*5)&31)<<11)|(((x*7+y*11)&63)<<5)|((x*13+y*17)&31);}
static lv_color_t expected_pixel(unsigned x,unsigned y){unsigned v=native_pixel(x,y);return lv_color_make(((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31);}
static uint8_t*sstv_rx_pixels(void){return pixels;}
static bool sstv_rx_preview_row(unsigned y,uint8_t*out){low_reads++;memcpy(out,pixels+y*SSTV_PREVIEW_WIDTH,SSTV_PREVIEW_WIDTH);return true;}
static bool sstv_rx_preview_high_row(unsigned y,uint16_t*out){if(!high_count||y<high_first||y>=high_first+high_count)return false;sharp_reads++;for(unsigned x=0;x<240;x++)out[x]=native_pixel(x,y);return true;}
static lv_obj_t *sstv_image,*title,*brand,*footer,*lines[5];static bool sstv_start_failed;static unsigned sstv_generation,sstv_lines,sstv_saved,sstv_last_saved;static char image_notice[20];static sstv_status_t sstv_status;
static void ensure_image(void){}
static void image_changed(void){lv_obj_invalidate(sstv_image);}
static bool sstv_rx_snapshot(sstv_status_t*s,unsigned*l,unsigned*g){(void)s;(void)l;(*g)++;return true;}
static bool sstv_rx_start_busy(void){return true;}
static bool voice_busy(void){return false;}
static bool sstv_rx_enable(bool on){return on;}
static const char*sstv_rx_start_error(void){return "waiting";}
'''
tail=r'''
static lv_color_t frame[320*240];static unsigned flushes;
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){for(int y=a->y1;y<=a->y2;y++)for(int x=a->x1;x<=a->x2;x++)frame[y*320+x]=*p++;flushes++;lv_disp_flush_ready(d);}
static lv_obj_t*label(int x,int y,int w){lv_obj_t*o=lv_label_create(lv_scr_act());lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_label_set_text(o,"");return o;}
static unsigned check(const char*when){unsigned bad=0;for(unsigned y=0;y<180;y++)for(unsigned x=0;x<240;x++){unsigned want=expected_pixel(x,y).full;if(frame[(y+30)*320+x+40].full!=want)bad++;}printf("%s: bad=%u sharp=%u low=%u flush=%u\n",when,bad,sharp_reads,low_reads,flushes);return bad;}
int main(void){lv_init();static lv_color_t buf[800];static lv_disp_draw_buf_t db;lv_disp_draw_buf_init(&db,buf,NULL,800);static lv_disp_drv_t d;lv_disp_drv_init(&d);d.hor_res=320;d.ver_res=240;d.draw_buf=&db;d.flush_cb=flush;lv_disp_drv_register(&d);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
brand=label(270,7,42);title=label(12,40,296);footer=label(12,221,296);lv_label_set_long_mode(footer,LV_LABEL_LONG_SCROLL_CIRCULAR);lines[0]=label(8,7,252);for(int i=1;i<5;i++)lines[i]=label(12,70+i*27,296);sstv_image=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(sstv_image);lv_obj_set_pos(sstv_image,0,30);lv_obj_set_size(sstv_image,320,180);lv_obj_clear_flag(sstv_image,LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(sstv_image,image_draw,LV_EVENT_DRAW_MAIN,NULL);
memset(pixels,3,sizeof(pixels));lv_tick_inc(20);lv_timer_handler();
// Negative control: LVGL's object-area expansion destroys preceding sharp rows.
for(unsigned y=0;y<180;y+=2){high_first=y;high_count=2;sstv_lines=(y+2)*240/180;sstv_status=SSTV_RECEIVING;refresh_receive();lv_area_t a={40,30+y,279,31+y};lv_obj_invalidate_area(sstv_image,&a);lv_tick_inc(150);lv_timer_handler();high_count=0;}
assert(check("expanded-area regression")>20000);
image_changed();lv_tick_inc(20);lv_timer_handler();
for(unsigned y=0;y<180;y+=2){high_first=y;high_count=2;sstv_lines=(y+2)*240/180;sstv_status=SSTV_RECEIVING;refresh_receive();image_changed_strip(y,2);lv_tick_inc(150);lv_timer_handler();high_count=0;}
assert(check("end receive")==0);sstv_status=SSTV_COMPLETE;for(unsigned i=0;i<100;i++){refresh_receive();lv_tick_inc(150);lv_timer_handler();}assert(check("after 100 refreshes")==0);return 0;}
'''
import tempfile
_tmp=tempfile.TemporaryDirectory(prefix='sstv-framebuffer-');work=Path(_tmp.name);p=work/'test.c';p.write_text(pre+draw+strip+'static void refresh_receive(void){'+body+'}\n'+tail)
lv=r/'ui-preview-firmware/components/lvgl';(work/'lv_conf.h').write_text((lv/'lv_conf.h').read_text().replace('#define LV_USE_PERF_MONITOR 1','#define LV_USE_PERF_MONITOR 0'));cmd=['cc','-O0','-DLV_CONF_INCLUDE_SIMPLE','-DLV_CONF_PATH='+str(work/'lv_conf.h'),'-DLV_KCONFIG_IGNORE','-DCONFIG_IDF_TARGET_ESP32','-I'+str(lv),'-I'+str(r/'ui-preview-firmware/main'),str(p),*[str(x) for x in (lv/'src').rglob('*.c')],'-lm','-o',str(work/'test')];subprocess.run(cmd,check=True);subprocess.run([str(work/'test')],check=True)
