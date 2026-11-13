#include "resource_metrics.h"
#include "ui_diagnostics.h"
#include "esp_heap_caps.h"
#include "call_indicator.h"
#include "key_layout.h"
#include "screen_sleep.h"
#include "board.h"
#include "nvs.h"
#ifdef CONFIG_FMO_BOARD_XINGZHI
#include "driver/rtc_io.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "fmo_display.h"
#include "display_idle.h"
#include "device_web.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
extern const lv_font_t font20, history_font, status_font, ui_small, ui_call, ui_clock, relay_font;
#include "fonts/coverage.h"
#include "buttons.h"
#include "voice.h"
#include "sstv_rx.h"
#include "sstv_store.h"
#include "battery.h"
#include "side_leds.h"
#include "relays.h"
#include "discovery.h"
#include "utf8_name.h"
#ifdef CONFIG_FMO_BOARD_XINGZHI
atomic_uint ui_stage,ui_stage_ms,ui_loop_ms,ui_loop_max_ms,ui_refresh_max_ms,ui_render_max_ms;
static void ui_mark(unsigned stage){atomic_store(&ui_stage,stage);atomic_store(&ui_stage_ms,(unsigned)(esp_timer_get_time()/1000));}
static void ui_max(atomic_uint *value,unsigned elapsed){if(elapsed>atomic_load(value))atomic_store(value,elapsed);}
#else
#define ui_mark(stage) ((void)0)
#endif
static bool relay_glyph(uint32_t c){lv_font_glyph_dsc_t d;return lv_font_get_glyph_dsc(&relay_font,&d,c,0);}
static lv_obj_t *title,*lines[5],*footer,*brand;
static lv_obj_t *a_hint,*c_hint;
static lv_obj_t *home,*battery_label,*battery_fill,*local_call,*net,*caption,*badge,*call_label,*hint,*relay_name,*relay_id,*home_notice,*last_call,*previous_call,*b_hint,*bars[9];
static buttons_t keys;
static uint8_t rotation;
unsigned ui_key_layout;bool ui_screen_sleeping;
static screen_sleep_t screen_sleep;
static bool breathing=true,ptt_latch=true;
bool ui_flashing_enabled=!FMO_HAS_SIDE_LEDS;unsigned ui_call_rgb=0xffd166,ui_call_opacity=255;
static nvs_handle_t ui_nv;
static bool ui_nv_ready;
static int shown=-1;
static char settings_notice[64];
#undef FMO_WIDTH
#define FMO_WIDTH lv_disp_get_hor_res(NULL)
static void create_ui(void);
static bool save_settings(uint8_t rot,bool pulse,bool latch){
 uint32_t packed=rot|(pulse?4:0)|(latch?8:0);
 if(!ui_nv_ready||nvs_set_u32(ui_nv,"options",packed)!=ESP_OK||nvs_commit(ui_nv)!=ESP_OK){snprintf(settings_notice,sizeof(settings_notice),"设置保存失败，请重试");return false;}
 rotation=rot;breathing=pulse;ui_flashing_enabled=call_flashing_enabled(FMO_HAS_SIDE_LEDS,pulse);ptt_latch=latch;settings_notice[0]=0;return true;
}
static bool save_key_layout(unsigned scheme){
 if(scheme>=KEY_LAYOUT_COUNT||!ui_nv_ready||nvs_set_u32(ui_nv,"key_layout",scheme)!=ESP_OK||nvs_commit(ui_nv)!=ESP_OK){snprintf(settings_notice,sizeof(settings_notice),"按键方案保存失败");return false;}
 ui_key_layout=scheme;settings_notice[0]=0;buttons_block(&keys);return true;
}
static void load_settings(void){
 ui_nv_ready=nvs_open("fmo_ui",NVS_READWRITE,&ui_nv)==ESP_OK;
 uint32_t packed=12;if(ui_nv_ready&&nvs_get_u32(ui_nv,"options",&packed)==ESP_OK&&packed<=15){rotation=packed&3;breathing=packed&4;ptt_latch=packed&8;ui_flashing_enabled=call_flashing_enabled(FMO_HAS_SIDE_LEDS,breathing);}
 uint32_t scheme=0;if(ui_nv_ready&&nvs_get_u32(ui_nv,"key_layout",&scheme)==ESP_OK&&scheme<KEY_LAYOUT_COUNT)ui_key_layout=scheme;else ui_key_layout=0;
}

static display_idle_t display_idle;
unsigned ui_backlight_percent=100,ui_idle_seconds,ui_pressed_mask;
unsigned ui_key_events;int ui_last_key=-1,ui_last_action=-1;
enum {HOME,MENU,RELAYS,VOLUME,IDENTITY,WEB,MIC,DISCOVER,CA_LIST,CA_CONFIRM,SETTINGS,KEY_LAYOUT,MEDIA,SSTV,GALLERY,RESOURCES};
static int page=HOME,selection=0;
static lv_obj_t *sstv_image;
static lv_img_dsc_t sstv_image_desc;
static sstv_status_t sstv_status;
static unsigned sstv_lines,sstv_generation;
static uint32_t sstv_saved;
static char image_notice[112];
static sstv_entry_t gallery_entries[SSTV_GALLERY_MAX];
static int gallery_count;
#ifdef FMO_FULL_SSTV
static sstv_storage_t gallery_space;static bool gallery_space_valid;
#endif
#ifdef FMO_SSTV_STREAMING
static void image_draw(lv_event_t *event){
 lv_draw_ctx_t *ctx=lv_event_get_draw_ctx(event);lv_obj_t *obj=lv_event_get_target(event);
 const uint8_t *pixels=sstv_rx_pixels();if(!pixels)return;
 lv_area_t box;lv_obj_get_coords(obj,&box);lv_color_t row[240];
 lv_img_dsc_t img={.header={.cf=LV_IMG_CF_TRUE_COLOR,.w=240,.h=1},.data_size=sizeof(row),.data=(const uint8_t*)row};
 lv_draw_img_dsc_t style;lv_draw_img_dsc_init(&style);
 int first=ctx->clip_area->y1>box.y1?ctx->clip_area->y1-box.y1:0;
 int last=ctx->clip_area->y2<box.y2?ctx->clip_area->y2-box.y1:179;
 for(int y=first;y<=last;y++){
  for(unsigned x=0;x<240;x++){uint8_t v=pixels[(y*SSTV_PREVIEW_HEIGHT/180)*SSTV_PREVIEW_WIDTH+x*SSTV_PREVIEW_WIDTH/240];row[x]=lv_color_make((v>>5)*255/7,((v>>2)&7)*255/7,(v&3)*255/3);}
  lv_area_t line={box.x1,box.y1+y,box.x1+239,box.y1+y};lv_draw_img(ctx,&style,&line,&img);
 }
}
static void image_changed(void){if(sstv_image)lv_obj_invalidate(sstv_image);}
static void ensure_image(void){
 if(sstv_image||!sstv_rx_pixels())return;
 sstv_image=lv_obj_create(lv_scr_act());lv_obj_remove_style_all(sstv_image);lv_obj_set_size(sstv_image,240,180);
 lv_obj_set_pos(sstv_image,(FMO_WIDTH-240)/2,30);lv_obj_clear_flag(sstv_image,LV_OBJ_FLAG_SCROLLABLE);
 lv_obj_add_event_cb(sstv_image,image_draw,LV_EVENT_DRAW_MAIN,NULL);lv_obj_add_flag(sstv_image,LV_OBJ_FLAG_HIDDEN);
}
#else
#ifdef FMO_FULL_SSTV
static lv_color_t *image_display;
#endif
static void image_changed(void){
#ifdef FMO_FULL_SSTV
 const uint8_t*p=sstv_rx_pixels();if(p&&image_display){for(int y=0;y<180;y++)for(int x=0;x<240;x++){
 unsigned at=((y*SSTV_HEIGHT/180)*SSTV_WIDTH+x*SSTV_WIDTH/240)*2;unsigned v=p[at]|((unsigned)p[at+1]<<8);
 image_display[y*240+x]=lv_color_make(((v>>11)&31)*255/31,((v>>5)&63)*255/63,(v&31)*255/31);
 }}
#endif
 lv_img_cache_invalidate_src(&sstv_image_desc);if(sstv_image)lv_obj_invalidate(sstv_image);
}
static void ensure_image(void){
 if(sstv_image||!sstv_rx_pixels())return;
#ifdef FMO_FULL_SSTV
 if(!image_display)image_display=heap_caps_calloc(240*180,sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!image_display)return;
#endif
 sstv_image_desc.header.cf=
#ifdef FMO_FULL_SSTV
 LV_IMG_CF_TRUE_COLOR;
#else
 LV_IMG_CF_INDEXED_8BIT;
#endif
#ifdef FMO_FULL_SSTV
 sstv_image_desc.header.w=240;sstv_image_desc.header.h=180;
#else
 sstv_image_desc.header.w=SSTV_WIDTH;sstv_image_desc.header.h=SSTV_HEIGHT;
#endif
 sstv_image_desc.data_size=
#ifdef FMO_FULL_SSTV
 240*180*sizeof(lv_color_t);
#else
 1024+SSTV_PIXELS;
#endif
#ifdef FMO_FULL_SSTV
sstv_image_desc.data=(const uint8_t*)image_display;
#else
sstv_image_desc.data=sstv_rx_pixels();
#endif
 sstv_image=lv_img_create(lv_scr_act());lv_img_set_src(sstv_image,&sstv_image_desc);lv_obj_set_pos(sstv_image,(FMO_WIDTH-SSTV_WIDTH)/2,88);
#ifdef FMO_FULL_SSTV
 /* Pre-scale into a dedicated immutable LVGL frame; avoid zoom render buffers. */
 lv_obj_set_pos(sstv_image,(FMO_WIDTH-240)/2,30);
#endif
 lv_obj_add_flag(sstv_image,LV_OBJ_FLAG_HIDDEN);
}
#endif
static void gallery_load(void){
#ifdef FMO_FULL_SSTV
 gallery_space_valid=sstv_store_stats(&gallery_space);
#endif
 image_notice[0]=0;uint8_t*p=sstv_rx_pixels();
#ifndef FMO_FULL_SSTV
 if(p)p+=1024;
#endif
#ifdef FMO_SSTV_STREAMING
 if(p)memset(p,0,SSTV_PREVIEW_BYTES);
 if(gallery_count>0&&p){if(!sstv_store_preview(gallery_entries[selection].id,p))snprintf(image_notice,sizeof(image_notice),"图片已删除、损坏或相册忙");}
#else
 if(p)memset(p,0,SSTV_BYTES);
 if(gallery_count>0&&p){if(!sstv_store_load(gallery_entries[selection].id,p))snprintf(image_notice,sizeof(image_notice),"图片已删除、损坏或相册忙");}
 #endif
 else snprintf(image_notice,sizeof(image_notice),"%s",sstv_store_error()?sstv_store_error():!p?"内存不足，无法显示图片":gallery_count<0?"相册读取失败，请重新进入":"相册为空");
 image_changed();
}

static device_profile_t ca_profiles[DEVICE_PROFILE_MAX],ca_target;
static int ca_count;
static bool ca_submitted;
static char ca_notice[96],ca_current[97];
static void load_ca_profiles(void){ca_count=device_profiles(ca_profiles);ca_notice[0]=0;ca_current[0]=0;if(ca_count<0){ca_count=0;snprintf(ca_notice,sizeof(ca_notice),"通联或后台忙，请稍后按 %c 刷新",key_layout_letter(ui_key_layout,1));}for(int i=0;i<ca_count;i++)if(ca_profiles[i].current)snprintf(ca_current,sizeof(ca_current),"%s",ca_profiles[i].name);}

static relay_t discovered_ui[DISCOVERY_MAX];static int discovered_count;static uint32_t discovered_revision;static char discovery_notice[96];
static bool idle_started=false,was_active=false,show_clock=false;
static int64_t idle_since;
static char held_call[16];
#ifdef FMO_FULL_SSTV
static lv_obj_t *call_card;
#endif
static const char*items[]={"选择中继","音量与静音","设备身份","网页管理","扬声器测试","麦克风测试","显示与按键","按键分配方案",
#ifdef FMO_FULL_SSTV
"内存与存储",
#endif
"返回通联"};
static void set_text(lv_obj_t*o,const char*s){
 char mapped[256];snprintf(mapped,sizeof(mapped),"%s",s);
 // Existing UI hints describe logical A/B/C roles, translate to physical keys.
 // The scheme selector itself deliberately shows physical A/B/C unchanged.
 if(page!=KEY_LAYOUT&&(o==footer||o==hint||((page==SETTINGS||page==IDENTITY||page==MIC)&&o==lines[3])||(page==CA_CONFIRM&&!ca_submitted&&o==lines[4])))for(char*p=mapped;*p;p++)if(*p>='A'&&*p<='C'&&(p==mapped||p[-1]==' '||p[-1]=='/'||(unsigned char)p[-1]>=128)&&(p[1]==' '||p[1]=='/'||(unsigned char)p[1]>=128))*p=key_layout_letter(ui_key_layout,*p-'A');
 if(strcmp(lv_label_get_text(o),mapped))lv_label_set_text(o,mapped);
}
static void set_text_fmt(lv_obj_t*o,const char*fmt,...){char text[256];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);set_text(o,text);}
#define lv_label_set_text set_text
#define lv_label_set_text_fmt set_text_fmt
static int ui_y(int y){return y*lv_disp_get_ver_res(NULL)/240;}
static void ui_pos(lv_obj_t*o,int x,int y){lv_obj_set_pos(o,x,ui_y(y));}
static void ui_size(lv_obj_t*o,int w,int h){lv_obj_set_size(o,w,ui_y(h));}
static void ui_set_y(lv_obj_t*o,int y){lv_obj_set_y(o,ui_y(y));}
static void ui_height(lv_obj_t*o,int h){lv_obj_set_height(o,ui_y(h));}
#define lv_obj_set_pos ui_pos
#define lv_obj_set_size ui_size
#define lv_obj_set_y ui_set_y
#define lv_obj_set_height ui_height
static lv_obj_t *label(const char *text,int x,int y,const lv_font_t *font,uint32_t color){
 lv_obj_t *o=lv_label_create(lv_scr_act());lv_label_set_text(o,text);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,FMO_WIDTH-24);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);return o;
}
static lv_obj_t *box(lv_obj_t*p,int x,int y,int w,int h,uint32_t bg,int radius){
 lv_obj_t*o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_bg_color(o,lv_color_hex(bg),0);lv_obj_set_style_radius(o,radius,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *home_label(const char*t,int x,int y,int w,const lv_font_t*f,uint32_t c){
 lv_obj_t*o=lv_label_create(home);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_obj_set_style_text_font(o,f,0);lv_obj_set_style_text_color(o,lv_color_hex(c),0);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_label_set_text(o,t);return o;
}
static void build_wide_home(void){
 home=box(lv_scr_act(),0,0,320,240,0x0b151c,0);
 home_label(FMO_BOARD_TITLE,12,8,72,&status_font,0xd4e4eb);
 local_call=home_label("--",90,8,102,&status_font,0x7ec9ff);
 lv_label_set_long_mode(local_call,LV_LABEL_LONG_SCROLL_CIRCULAR);
 net=home_label("Wi-Fi",198,9,38,&ui_small,0xb4c7d2);lv_obj_set_style_text_align(net,LV_TEXT_ALIGN_RIGHT,0);
 lv_obj_t*battery_box=box(home,242,11,22,12,0x0b151c,2);lv_obj_set_style_border_width(battery_box,1,0);lv_obj_set_style_border_color(battery_box,lv_color_hex(0x9cb2bf),0);box(home,264,14,2,6,0x9cb2bf,1);battery_fill=box(home,244,13,18,8,0x58d6b2,1);battery_label=home_label("--",269,9,40,&ui_small,0xb4c7d2);
 lv_obj_t*card=box(home,10,32,300,93,0x14232d,12);
#ifdef FMO_FULL_SSTV
 call_card=card;
#endif
lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0x263b48),0);
 caption=home_label("我的呼号",23,43,180,&ui_small,0xb4c7d2);
 badge=home_label("未接入",240,42,58,&ui_small,0xa8b5bf);lv_obj_set_style_text_align(badge,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_bg_opa(badge,LV_OPA_COVER,0);lv_obj_set_style_bg_color(badge,lv_color_hex(0x263139),0);lv_obj_set_style_radius(badge,9,0);lv_obj_set_style_pad_ver(badge,3,0);
 call_label=home_label("--",22,64,276,&ui_call,0xffd166);
 hint=home_label("等待重新接入",23,110,216,&ui_small,0xc2d3de);
 for(int i=0;i<9;i++)bars[i]=box(home,247+i*5,116,3,3,0x58d6b2,1);
 lv_obj_t*last_card=box(home,10,132,300,34,0x162a35,8);lv_obj_set_style_border_width(last_card,1,0);lv_obj_set_style_border_color(last_card,lv_color_hex(0x2a4858),0);
 home_label("上次",20,143,32,&status_font,0xb4c7d2);
 last_call=home_label("--",54,136,124,&font20,0x7ec9ff);
 box(home,182,140,1,18,0x35505f,0);
 previous_call=home_label("--",190,138,108,&history_font,0x7ec9ff);lv_obj_set_style_text_align(previous_call,LV_TEXT_ALIGN_RIGHT,0);
 lv_label_set_long_mode(last_call,LV_LABEL_LONG_SCROLL_CIRCULAR);
 lv_label_set_long_mode(previous_call,LV_LABEL_LONG_SCROLL_CIRCULAR);
 relay_name=home_label("",12,173,245,&relay_font,0xe0ebf1);
 relay_id=home_label("",259,174,49,&status_font,0xb4c7d2);lv_obj_set_style_text_align(relay_id,LV_TEXT_ALIGN_RIGHT,0);
 home_notice=home_label("",12,190,296,&ui_small,0xc2d3de);
 box(home,0,208,320,32,0x101e28,0);box(home,0,208,320,1,0x243743,0);
 lv_obj_t*o=a_hint=home_label("A  音量 -",0,211,106,&ui_small,0xd4e4eb);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
 b_hint=home_label("B  开始发话",106,211,108,&ui_small,0x58d6b2);lv_obj_set_style_text_align(b_hint,LV_TEXT_ALIGN_CENTER,0);
 o=c_hint=home_label("C  音量 +",214,211,106,&ui_small,0xd4e4eb);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
 const char*sub[]={"长按中继/图片","PTT 模式见设置","长按进菜单"};for(int i=0;i<3;i++){o=home_label(sub[i],i*107,225,106,&ui_small,0xa9bfcd);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);}
 lv_obj_add_flag(home,LV_OBJ_FLAG_HIDDEN);
}
#include "boards/xingzhi/home.inc"
static void build_home(void){
 if(FMO_WIDTH==320){build_wide_home();return;}
 build_compact_home();
}
#ifdef CONFIG_FMO_BOARD_XINGZHI
/* Avoid restarting layout/animations and allocating identical text every poll. */
static void home_text(lv_obj_t*o,const char*t){if(strcmp(lv_label_get_text(o),t))lv_label_set_text(o,t);}
static void home_text_fmt(lv_obj_t*o,const char*fmt,...){char text[256];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);home_text(o,text);}
static void home_text_color(lv_obj_t*o,lv_color_t c,lv_style_selector_t sel){if(lv_obj_get_style_text_color(o,sel).full!=c.full)lv_obj_set_style_text_color(o,c,sel);}
static void home_bg_color(lv_obj_t*o,lv_color_t c,lv_style_selector_t sel){if(lv_obj_get_style_bg_color(o,sel).full!=c.full)lv_obj_set_style_bg_color(o,c,sel);}
static void home_text_opa(lv_obj_t*o,lv_opa_t v,lv_style_selector_t sel){if(lv_obj_get_style_text_opa(o,sel)!=v)lv_obj_set_style_text_opa(o,v,sel);}
static void home_text_align(lv_obj_t*o,lv_text_align_t v,lv_style_selector_t sel){if(lv_obj_get_style_text_align(o,sel)!=v)lv_obj_set_style_text_align(o,v,sel);}
static void home_x(lv_obj_t*o,lv_coord_t v){if(lv_obj_get_x(o)!=v)lv_obj_set_x(o,v);}
static void home_y(lv_obj_t*o,lv_coord_t v){if(lv_obj_get_y(o)!=v)lv_obj_set_y(o,v);}
static void home_width(lv_obj_t*o,lv_coord_t v){if(lv_obj_get_width(o)!=v)lv_obj_set_width(o,v);}
static void home_height(lv_obj_t*o,lv_coord_t v){if(lv_obj_get_height(o)!=v)lv_obj_set_height(o,v);}
static void home_pos(lv_obj_t*o,lv_coord_t x,lv_coord_t y){home_x(o,x);home_y(o,y);}
static void home_visibility(lv_obj_t*o,bool visible){bool hidden=lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN);if(visible&&hidden)lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);else if(!visible&&!hidden)lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
#else
#define home_text lv_label_set_text
#define home_text_fmt lv_label_set_text_fmt
#define home_text_color lv_obj_set_style_text_color
#define home_bg_color lv_obj_set_style_bg_color
#define home_text_opa lv_obj_set_style_text_opa
#define home_text_align lv_obj_set_style_text_align
#define home_x lv_obj_set_x
#define home_y lv_obj_set_y
#define home_width lv_obj_set_width
#define home_height lv_obj_set_height
#define home_pos lv_obj_set_pos
static void home_visibility(lv_obj_t*o,bool visible){if(visible)lv_obj_clear_flag(o,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);}
#endif
static void refresh_home(const device_view_t*v,const voice_view_t*a,const relay_t*r){
 lv_obj_t*key_hints[]={a_hint,b_hint,c_hint};
 for(unsigned i=0;i<3;i++){
  unsigned role=key_layouts[ui_key_layout<KEY_LAYOUT_COUNT?ui_key_layout:0][i];
  home_text_fmt(key_hints[i],"%c %s",'A'+i,key_layout_caption(ui_key_layout,i,ptt_latch,a->tx));
  int width=lv_disp_get_hor_res(NULL)/3;
  home_x(key_hints[i],i*width);home_width(key_hints[i],width);
  home_text_align(key_hints[i],LV_TEXT_ALIGN_CENTER,0);
  home_text_color(key_hints[i],lv_color_hex(role==1?0x58d6b2:0xd4e4eb),0);
 }
 uint32_t accent=a->tx?0xff3030:a->rx?0x20ff70:v->mqtt_online?0x58d6b2:0xa8b5bf;
 uint32_t tint=a->tx?0x441818:a->rx?0x123d25:v->mqtt_online?0x12352f:0x263139;
 int percent;bool charging;battery_view(&percent,&charging);if(percent<0)home_text(battery_label,"--");else home_text_fmt(battery_label,"%d%%%s",percent,charging?"+":"");if(percent<=0)home_visibility(battery_fill,false);else{home_visibility(battery_fill,true);home_width(battery_fill,(18*percent+99)/100);}home_bg_color(battery_fill,lv_color_hex(charging?0x5ceaff:percent<=25?0xff7766:0x58d6b2),0);
 home_text_fmt(local_call,FMO_CALL_PREFIX "%s",v->call);
 home_text(net,v->online?"Wi-Fi":"未联网");
 home_text_color(net,lv_color_hex(v->online?0x58d6b2:0xffb778),0);
 home_text(caption,a->rx&&!a->tx?"当前通联 · 对方呼号":a->tx?"当前通联 · 本机发话":show_clock?"北京时间":"最近通联");
 home_text(badge,a->tx?"发话中":a->rx?"接收中":v->mqtt_online?"守听中":"未接入");
 home_text_color(badge,lv_color_hex(accent),0);home_bg_color(badge,lv_color_hex(tint),0);
 time_t utc=time(NULL);bool idle=show_clock;bool pending_clock=idle&&utc<1700000000;
 if(a->rx&&!a->tx)home_text(call_label,a->caller[0]?a->caller:"--");
 else if(a->tx)home_text_fmt(call_label,FMO_CALL_PREFIX "%s",v->call);
 else if(!idle)home_text(call_label,held_call);
 else if(pending_clock)home_text(call_label,"等待校时");
 else {time_t china=utc+8*3600;struct tm t;gmtime_r(&china,&t);char clock[16];strftime(clock,sizeof(clock),"%H:%M:%S",&t);home_text(call_label,clock);}
 home_text_align(call_label,idle?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
 // Select one final font; do not briefly apply the larger clock font on S3.
 const lv_font_t *call_font=pending_clock?&font20:(FMO_WIDTH==240||!idle)?&ui_call:&ui_clock;
 if(lv_obj_get_style_text_font(call_label,LV_PART_MAIN)!=call_font)lv_obj_set_style_text_font(call_label,call_font,0);
 home_y(call_label,FMO_WIDTH==240?55:64);
#ifdef FMO_FULL_SSTV
 // Keep one complete line throughout blink phases, without hidden-object layout changes.
 home_height(call_label,lv_font_get_line_height(FMO_WIDTH==240?&ui_call:call_font));
#endif
 if(lv_label_get_long_mode(call_label)!=LV_LABEL_LONG_SCROLL_CIRCULAR)lv_label_set_long_mode(call_label,LV_LABEL_LONG_SCROLL_CIRCULAR);
 call_indicator_t signal=call_indicator(a->tx,a->rx,ui_flashing_enabled,esp_timer_get_time()/1000);
 ui_call_rgb=signal.color;ui_call_opacity=signal.opacity;
 home_text_color(call_label,lv_color_hex(signal.color),0);
#ifdef FMO_FULL_SSTV
 home_visibility(call_label,true);
 home_text_opa(call_label,signal.opacity,0);
 static unsigned last_call_opacity=256,last_call_color=0;
 if(last_call_opacity!=signal.opacity||last_call_color!=signal.color){
  // Redraw the whole call card on phase changes and when activity ends.
  // Repaint its background plus all overlapping labels, not a glyph-sized dirty strip.
  if(call_card)lv_obj_invalidate(call_card);
  lv_obj_invalidate(call_label);lv_obj_invalidate(local_call);
  last_call_opacity=signal.opacity;last_call_color=signal.color;
 }
#else
 home_text_opa(call_label,255,0);
 if(signal.opacity)home_visibility(call_label,true);else home_visibility(call_label,false);
#endif
 // Header callsign also signals TX; the central remote callsign signals RX.
 home_text_color(local_call,lv_color_hex(a->tx?signal.color:0x7ec9ff),0);
#ifdef FMO_FULL_SSTV
 home_visibility(local_call,true);
 home_text_opa(local_call,a->tx?signal.opacity:255,0);
#else
 home_text_opa(local_call,255,0);
 if(a->tx&&!signal.opacity)home_visibility(local_call,false);else home_visibility(local_call,true);
#endif
 home_text_opa(badge,255,0);

 if(a->tx)home_text(hint,ptt_latch?"再次短按 B 停止":"松开 B 停止");else home_text(hint,idle?"":a->rx?"接收中":v->mqtt_online?"守听中":"中继断开");
 home_text(last_call,a->last_caller[0]?a->last_caller:"--");
 home_text(previous_call,a->previous_caller[0]?a->previous_caller:"--");
 char name[65];name_for_display(name,sizeof(name),r->name[0]?r->name:r->call,relay_glyph);home_text(relay_name,name);home_text_fmt(relay_id,"%lu",(unsigned long)r->uid);
 if(v->profile_message[0])home_text(home_notice,v->profile_message);else if(a->muted)home_text_fmt(home_notice,"%s · 静音",a->notice[0]?a->notice:"守听中");else home_text_fmt(home_notice,"%s · 音量 %d%%",a->notice[0]?a->notice:"守听中",a->volume);for(unsigned i=0;i<3;i++)if(key_layout_letter(ui_key_layout,1)=='A'+i)home_text_color(key_hints[i],lv_color_hex(accent),0);
 if(show_clock&&v->mqtt_online&&!v->profile_message[0]&&(esp_timer_get_time()/5000000)%2==0)home_text_fmt(home_notice,"长按 %c：中继与图片",key_layout_letter(ui_key_layout,0));
 int peak=a->tx?a->mic_peak:a->rx?a->speaker_peak:0;int level=peak/1800;if(level>9)level=9;
 for(int i=0;i<9;i++){int h=i<level?5+(i%3)*4:3;home_pos(bars[i],(FMO_WIDTH==240?180:247)+i*5,(FMO_WIDTH==240?113:119)-h);home_height(bars[i],h);home_bg_color(bars[i],lv_color_hex(i<level?accent:0x35505f),0);}
}
static void go(int next){if(page==SSTV
#ifdef FMO_SSTV_STREAMING
 ||page==GALLERY
#endif
 ){sstv_rx_enable(false);}
 voice_ptt(false);voice_mic_test(false);page=next;
 if(next==SSTV){
#ifdef FMO_FULL_SSTV
 relay_t rx_relay={0};char context[128]="";relays_current(&rx_relay,NULL);if(rx_relay.uid)snprintf(context,sizeof(context),"%s / %lu",rx_relay.name[0]?rx_relay.name:rx_relay.call,(unsigned long)rx_relay.uid);sstv_rx_relay(context);
#endif
 sstv_saved=0;image_notice[0]=0;sstv_status=SSTV_WAITING;sstv_lines=0;sstv_generation=~0u;if(!sstv_rx_enable(true))snprintf(image_notice,sizeof(image_notice),"%s",sstv_store_error()?sstv_store_error():sstv_store_save_error());ensure_image();}
 if(next==RESOURCES)gallery_space_valid=sstv_store_stats(&gallery_space);
 if(next==GALLERY){sstv_rx_init();ensure_image();selection=0;gallery_count=sstv_store_list(gallery_entries);gallery_load();}
 device_management_ap(next==WEB);selection=0;buttons_block(&keys);if(next==RELAYS)selection=relays_current(NULL,NULL);if(next==CA_CONFIRM)ca_submitted=false;if(next==KEY_LAYOUT)selection=ui_key_layout;if(next==MIC)voice_mic_test(true);if(next==IDENTITY||next==CA_LIST)load_ca_profiles();if(next==DISCOVER){discovered_count=discovery_list(discovered_ui,&discovered_revision);discovery_refresh();discovery_notice[0]=0;}}
static void key(int k,key_action_t action,void*ctx){(void)ctx;ui_key_events++;ui_last_key=k;ui_last_action=action;ESP_LOGI("buttons","key=%d action=%d",k,action);
 if(action==KEY_PTT_STOP||action==KEY_CANCEL){voice_ptt(false);return;}
 if(action==KEY_PTT_START){if(page==HOME)voice_ptt(true);return;}
 if(page==KEY_LAYOUT){if(action==KEY_LONG&&k==0){go(HOME);return;}if(action==KEY_SHORT){if(k==0)selection=(selection+KEY_LAYOUT_COUNT-1)%KEY_LAYOUT_COUNT;if(k==2)selection=(selection+1)%KEY_LAYOUT_COUNT;if(k==1){voice_ptt(false);if(save_key_layout(selection)){go(HOME);create_ui();}}}return;}
 if(action==KEY_LONG){if(page==HOME){if(k==0)go(MEDIA);if(k==1)voice_ptt(false);if(k==2)go(MENU);}else if(k==0)go(HOME);return;}
 if(action!=KEY_SHORT)return;
 if(page==HOME){if(k==0)voice_volume(-5);if(k==1&&ptt_latch)voice_toggle_ptt();if(k==2)voice_volume(5);return;}
 if(page==MEDIA){if(k==0)selection=(selection+3)%4;if(k==2)selection=(selection+1)%4;if(k==1)go(selection==0?RELAYS:selection==1?SSTV:selection==2?GALLERY:HOME);return;}
 if(page==SSTV){
  if(k==0){go(MEDIA);return;}
  if(k==2&&sstv_status!=SSTV_RECEIVING){go(SSTV);return;}
  if(k==1&&!sstv_saved){
   if(sstv_status!=SSTV_COMPLETE&&(sstv_status!=SSTV_INTERRUPTED||sstv_lines<2))snprintf(image_notice,sizeof(image_notice),"尚无可保存的图像");
   else if(voice_busy())snprintf(image_notice,sizeof(image_notice),"通联中，请稍后保存");
   else if(sstv_status==SSTV_INTERRUPTED?sstv_rx_save_partial(&sstv_saved):sstv_rx_save(&sstv_saved)){
#ifdef FMO_FULL_SSTV
    /* Re-arm only after durable save succeeds; failure retains the completed image. */
    go(SSTV);
#else
    snprintf(image_notice,sizeof(image_notice),"已保存 #%lu",(unsigned long)sstv_saved);
#endif
   }
   else snprintf(image_notice,sizeof(image_notice),"%s",sstv_store_error()?sstv_store_error():sstv_store_save_error());
  }return;
 }
 if(page==GALLERY){if(k==1){go(MEDIA);return;}if(gallery_count>0){if(k==0)selection=(selection+gallery_count-1)%gallery_count;if(k==2)selection=(selection+1)%gallery_count;gallery_load();}return;}
 if(page==RESOURCES){if(k==1)go(MENU);else selection=1-selection;return;}
 if(page==MENU){if(k==0)selection=(selection+sizeof(items)/sizeof(items[0])-1)%(sizeof(items)/sizeof(items[0]));if(k==2)selection=(selection+1)%(sizeof(items)/sizeof(items[0]));if(k==1){int target=selection;if(target==0)go(RELAYS);else if(target==1)go(VOLUME);else if(target==2)go(IDENTITY);else if(target==3)go(WEB);else if(target==4)voice_tone();else if(target==5)go(MIC);else if(target==6)go(SETTINGS);else if(target==7)go(KEY_LAYOUT);
#ifdef FMO_FULL_SSTV
else if(target==8)go(RESOURCES);
#endif
else go(HOME);}return;}
 if(page==SETTINGS){
 if(k==0){selection=(selection+2)%3;}if(k==2)selection=(selection+1)%3;
 if(k==1){voice_ptt(false);if(selection==0){if(save_settings((rotation+1)%4,breathing,ptt_latch)){lv_disp_set_rotation(NULL,(lv_disp_rot_t)rotation);create_ui();}}
 else if(selection==1){if(FMO_HAS_SIDE_LEDS)save_settings(rotation,!breathing,ptt_latch);else snprintf(settings_notice,sizeof(settings_notice),"本机无灯条，使用呼号闪烁");}
 else if(save_settings(rotation,breathing,!ptt_latch))buttons_set_momentary(&keys,!ptt_latch);}
 return;}
 if(page==RELAYS){int n=relays_count(),pos=relays_order_position(selection);if(k==0){pos=(pos+n)%(n+1);selection=pos==n?-1:relays_order_at(pos);}if(k==2){pos=(pos+1)%(n+1);selection=pos==n?-1:relays_order_at(pos);}if(k==1){if(selection<0)go(DISCOVER);else if(device_select_relay(selection))go(HOME);}return;}
 if(page==DISCOVER){int n=discovered_count+1;if(k==0)selection=(selection+n-1)%n;if(k==2)selection=(selection+1)%n;if(k==1){if(selection==discovered_count){discovery_refresh();discovered_count=discovery_list(discovered_ui,&discovered_revision);selection=0;snprintf(discovery_notice,sizeof(discovery_notice),"空闲时刷新，稍后再查看");}else{const char*error=discovery_select(selection,discovered_revision);if(!error)go(HOME);else snprintf(discovery_notice,sizeof(discovery_notice),"%s",error);}}return;}
 if(page==IDENTITY){if(k==1)go(CA_LIST);return;}
 if(page==CA_LIST){if(!ca_count){if(k==1)load_ca_profiles();return;}if(k==0)selection=(selection+ca_count-1)%ca_count;if(k==2)selection=(selection+1)%ca_count;if(k==1){device_profile_t*p=&ca_profiles[selection];if(p->current&&p->active)snprintf(ca_notice,sizeof(ca_notice),"该体系正在使用");else if(p->current&&!p->has_certificate)go(WEB);else{ca_target=*p;go(CA_CONFIRM);}}return;}
 if(page==CA_CONFIRM){if(ca_submitted)return;if(k==0||k==2){go(CA_LIST);return;}if(k==1){if(device_profile_switch(ca_target.id)){ca_submitted=true;snprintf(ca_notice,sizeof(ca_notice),"切换后自动刷新中继列表");buttons_block(&keys);}else snprintf(ca_notice,sizeof(ca_notice),"通联中或已有请求，请稍后重试");}return;}
 if(page==VOLUME){if(k==0)voice_volume(-5);if(k==2)voice_volume(5);if(k==1)voice_mute();return;}
 if(k==1)go(HOME);
}
static void refresh(void){
 ui_mark(2);device_view_t v;device_web_view(&v);ui_mark(3);voice_view_t a;voice_view(&a);
 int64_t now=esp_timer_get_time();bool active=a.tx||a.rx;
 if(!idle_started||active||was_active){idle_since=now;idle_started=true;}
 was_active=active;show_clock=!active&&now-idle_since>=10000000;
 if(a.tx||!held_call[0])snprintf(held_call,sizeof(held_call),FMO_CALL_PREFIX "%s",v.call);
 if(a.rx&&!a.tx)snprintf(held_call,sizeof(held_call),"%s",a.caller[0]?a.caller:"--");
 ui_mark(4);relay_t relay;int current=relays_current(&relay,NULL);ui_mark(5);
 if(shown!=page){lv_obj_set_pos(brand,12,8);lv_obj_set_width(brand,FMO_WIDTH-24);lv_obj_set_style_text_align(brand,LV_TEXT_ALIGN_LEFT,0);lv_obj_set_width(lines[0],FMO_WIDTH-24);if(sstv_image){if(page==SSTV||page==GALLERY)lv_obj_clear_flag(sstv_image,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(sstv_image,LV_OBJ_FLAG_HIDDEN);}for(int i=0;i<5;i++){lv_label_set_text(lines[i],"");lv_obj_set_pos(lines[i],12,70+i*27);lv_obj_set_style_text_color(lines[i],lv_color_hex(0xecf2f8),0);lv_obj_set_style_text_font(lines[i],(page==RELAYS||page==DISCOVER||page==CA_LIST||page==CA_CONFIRM||page==IDENTITY)?&relay_font:&status_font,0);lv_label_set_long_mode(lines[i],FMO_WIDTH==240?LV_LABEL_LONG_SCROLL_CIRCULAR:LV_LABEL_LONG_DOT);}shown=page;bool is_home=page==HOME;if(is_home)lv_obj_clear_flag(home,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(home,LV_OBJ_FLAG_HIDDEN);lv_obj_t*old[]={brand,title,footer,lines[0],lines[1],lines[2],lines[3],lines[4]};for(unsigned i=0;i<8;i++){if(is_home)lv_obj_add_flag(old[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(old[i],LV_OBJ_FLAG_HIDDEN);}}
 lv_label_set_text_fmt(brand,(page==RELAYS||page==DISCOVER)?"体系：%s":FMO_BOARD_TITLE " · %s",(page==RELAYS||page==DISCOVER)?relays_profile_name():FMO_VERSION);
 lv_obj_set_pos(footer,12,220);lv_obj_set_style_text_font(footer,&status_font,0);lv_obj_set_style_text_color(footer,lv_color_hex(0x70d5ae),0);lv_label_set_text(footer,"长按 A 返回       B 确认");
 if(page==HOME){
  refresh_home(&v,&a,&relay);
 }else if(page==MEDIA){
  lv_label_set_text(title,"中继与图片");const char*choices[]={"选择中继","SSTV 接收",
#ifdef FMO_FULL_SSTV
 "SSTV相册",
#else
 "Gallery 图片相册",
#endif
"返回通联"};
  for(int i=0;i<4;i++){lv_label_set_text_fmt(lines[i],"%s %s",selection==i?">":" ",choices[i]);}
  lv_label_set_text(footer,"A 上项  B 确认  C 下项");
 }else if(page==SSTV){
  lv_label_set_text(title,"SSTV / Robot36");lv_label_set_text_fmt(brand,"当前中继：%s / %lu",relay.call,(unsigned long)relay.uid);
  unsigned before=sstv_generation;
  if(sstv_rx_snapshot(&sstv_status,&sstv_lines,&sstv_generation)){
   ensure_image();if(sstv_image)lv_obj_clear_flag(sstv_image,LV_OBJ_FLAG_HIDDEN);
   if(before!=sstv_generation||sstv_status==SSTV_RECEIVING||sstv_status==SSTV_COMPLETE){image_changed();if(!sstv_saved)image_notice[0]=0;}
  }
  if(image_notice[0])lv_label_set_text(lines[0],image_notice);
#ifdef FMO_FULL_SSTV
  else if(sstv_status==SSTV_RECEIVING)lv_label_set_text_fmt(lines[0],"接收中 %u/240",sstv_lines);
  else lv_label_set_text(lines[0],sstv_status==SSTV_COMPLETE?"接收完成":sstv_status==SSTV_INTERRUPTED?"中断，等待新信号":"等待信号");
#else
  else lv_label_set_text_fmt(lines[0],"%s  %u/240",sstv_status==SSTV_RECEIVING?"接收中":sstv_status==SSTV_COMPLETE?"接收完成":sstv_status==SSTV_INTERRUPTED?"中断，等待新信号":"等待信号",sstv_lines);
#endif
  lv_obj_set_y(lines[0],64);
#ifdef FMO_FULL_SSTV
  lv_obj_add_flag(title,LV_OBJ_FLAG_HIDDEN);lv_label_set_text(brand,"SSTV");lv_obj_set_pos(brand,FMO_WIDTH-50,7);lv_obj_set_width(brand,42);lv_obj_set_style_text_align(brand,LV_TEXT_ALIGN_RIGHT,0);
  lv_obj_set_pos(lines[0],8,7);lv_obj_set_width(lines[0],FMO_WIDTH-68);lv_obj_set_y(footer,lv_disp_get_ver_res(NULL)-19);
#endif
#ifdef FMO_FULL_SSTV
  lv_label_set_text(footer,sstv_status==SSTV_COMPLETE?(sstv_saved?"A 退出  C 继续":"A 退出  B 保存  C 继续"):sstv_status==SSTV_INTERRUPTED?"A退出 B存残图 C重试":"A 退出");
#else
  lv_label_set_text(footer,sstv_status==SSTV_RECEIVING?"A 退出":"A 退出  B 保存  C 重新等待");
#endif
 }else if(page==GALLERY){
  lv_label_set_text_fmt(title,"Gallery  %d/%d",gallery_count>0?selection+1:0,gallery_count>0?gallery_count:0);
  if(image_notice[0])lv_label_set_text(lines[0],image_notice);else if(gallery_count>0)lv_label_set_text_fmt(lines[0],"Robot36 · 图片 #%lu",(unsigned long)gallery_entries[selection].id);
  lv_obj_set_y(lines[0],64);
#ifdef FMO_FULL_SSTV
  if(!image_notice[0]){if(gallery_count>0&&gallery_entries[selection].partial)lv_label_set_text_fmt(lines[0],"残图 %u/240行",gallery_entries[selection].received_lines);else lv_label_set_text(lines[0],"SSTV相册");}
  lv_obj_add_flag(title,LV_OBJ_FLAG_HIDDEN);lv_label_set_text_fmt(brand,"%d/%d",gallery_count>0?selection+1:0,gallery_count>0?gallery_count:0);lv_obj_set_pos(brand,FMO_WIDTH-50,7);lv_obj_set_width(brand,42);lv_obj_set_style_text_align(brand,LV_TEXT_ALIGN_RIGHT,0);lv_obj_set_pos(lines[0],8,7);lv_obj_set_width(lines[0],FMO_WIDTH-68);lv_obj_set_y(footer,lv_disp_get_ver_res(NULL)-19);
#endif
  lv_label_set_text(footer,"A 上张  B 返回  C 下张");
#ifdef FMO_FULL_SSTV
  if(gallery_space_valid){unsigned phase=(unsigned)(now/4000000)%3;if(phase==1)lv_label_set_text_fmt(footer,"%uKB/20张 余约%u张",(unsigned)(gallery_space.total/1024),gallery_space.remaining);if(phase==2)lv_label_set_text(footer,gallery_space.low?"空间不足 请导出并删除":"请及时导出并删除旧图");}
#endif

#ifdef FMO_FULL_SSTV
 }else if(page==RESOURCES){resource_metrics_t m;resource_metrics_read(&m);char metric_text[96];
 if(selection==0){
 lv_label_set_text(title,"运行内存  1/2");
 snprintf(metric_text,sizeof(metric_text),"CPU 忙碌 %.0f%%",(double)m.average);lv_label_set_text(lines[0],m.valid?metric_text:"CPU 采样中");
 snprintf(metric_text,sizeof(metric_text),"高速内存 已用 %.1f%%",m.internal_total?100.0*(m.internal_total-m.internal_free)/m.internal_total:0);lv_label_set_text(lines[1],metric_text);
 snprintf(metric_text,sizeof(metric_text),"剩余 %.1f / %.1f KB",m.internal_free/1024.0,m.internal_total/1024.0);lv_label_set_text(lines[2],metric_text);
 if(m.psram_total)snprintf(metric_text,sizeof(metric_text),"扩展内存 已用 %.1f%%",100.0*(m.psram_total-m.psram_free)/m.psram_total);else snprintf(metric_text,sizeof(metric_text),"扩展内存：无");lv_label_set_text(lines[3],metric_text);
 snprintf(metric_text,sizeof(metric_text),"剩余 %.2f / %.2f MB",m.psram_free/1048576.0,m.psram_total/1048576.0);lv_label_set_text(lines[4],metric_text);
 }else{
 lv_label_set_text(title,"照片存储  2/2");
 if(gallery_space_valid){
 snprintf(metric_text,sizeof(metric_text),"相册已用 %.1f%%",gallery_space.total?100.0*gallery_space.used/gallery_space.total:0);lv_label_set_text(lines[0],metric_text);
 lv_label_set_text_fmt(lines[1],"已用 %u / %u KB",(unsigned)((gallery_space.used+1023)/1024),(unsigned)(gallery_space.total/1024));
 lv_label_set_text_fmt(lines[2],"已存 %u 张 / 最多20张",gallery_space.count);
 lv_label_set_text_fmt(lines[3],"预计还能存约 %u 张",gallery_space.remaining);
 }else{lv_label_set_text(lines[0],"相册暂时无法读取");for(int i=1;i<4;i++)lv_label_set_text(lines[i],"");}
 lv_label_set_text(lines[4],"请及时导出并删除旧图");}
 lv_label_set_text(footer,"A/C 翻页  B 返回");
#endif
 }else if(page==MENU){lv_label_set_text_fmt(title,"设备菜单  %d/2",selection/5+1);for(int i=0;i<5;i++){int n=(selection/5)*5+i;if(n<(int)(sizeof(items)/sizeof(items[0])))lv_label_set_text_fmt(lines[i],"%s %s",n==selection?">":" ",items[n]);else lv_label_set_text(lines[i],"");}lv_label_set_text(footer,"A 上项  B 确认  C 下项 / 长按A返回");
 }else if(page==RELAYS){int n=relays_count();if(selection>=n)selection=current<n?current:-1;int pos=relays_order_position(selection);int total=n+1,start=pos/4*4;lv_label_set_text_fmt(title,"选择中继 %d/%d",pos/4+1,(total+3)/4);for(int i=0;i<4;i++){int p=start+i;lv_obj_set_style_text_color(lines[i],lv_color_hex(p==n?0xffc567:0xecf2f8),0);if(p>=total)lv_label_set_text(lines[i],"");else if(p==n)lv_label_set_text_fmt(lines[i],"%s + 发现中继",selection<0?">":" ");else{int index=relays_order_at(p);relay_t r={0};if(!relays_get(index,&r)){lv_label_set_text(lines[i],"");continue;}char name[65];name_for_display(name,sizeof(name),r.name[0]?r.name:r.call,relay_glyph);lv_label_set_text_fmt(lines[i],"%s %s%s / %s",selection==index?">":" ",current==index?"[当前] ":"",name,r.call);}}lv_label_set_text_fmt(lines[4],"体系：%s",relays_profile_name());lv_label_set_text(footer,"A 上项  B 确认  C 下项 / 长按A返回");
 }else if(page==DISCOVER){relay_t fresh[DISCOVERY_MAX];uint32_t rev;int num=discovery_list(fresh,&rev);if(rev!=discovered_revision||num!=discovered_count){relay_t old={0};bool was_refresh=selection==discovered_count;if(selection<discovered_count)old=discovered_ui[selection];memcpy(discovered_ui,fresh,num*sizeof(relay_t));discovered_count=num;discovered_revision=rev;discovery_notice[0]=0;selection=was_refresh?num:0;for(int i=0;!was_refresh&&i<num;i++)if(fresh[i].uid==old.uid&&!strcmp(fresh[i].fingerprint,old.fingerprint))selection=i;}int total=discovered_count+1,start=selection/4*4;lv_label_set_text_fmt(title,"发现中继 %d/%d",selection/4+1,(total+3)/4);for(int i=0;i<4;i++){int p=start+i;if(p>=total)lv_label_set_text(lines[i],"");else if(p==discovered_count)lv_label_set_text_fmt(lines[i],"%s 刷新列表",selection==p?">":" ");else{char name[65];name_for_display(name,sizeof(name),discovered_ui[p].name,relay_glyph);lv_label_set_text_fmt(lines[i],"%s %s",selection==p?">":" ",name);}}char progress[96];discovery_status(progress,sizeof(progress));lv_label_set_text(lines[4],discovery_notice[0]?discovery_notice:progress);lv_label_set_text(footer,"A 上项  B 选择  C 下项 / 长按A返回");
 }else if(page==KEY_LAYOUT){lv_label_set_text_fmt(title,"按键方案 %d/6",selection+1);
 const char*roles[]={"音量- / 返回","PTT / 确认","音量+ / 菜单"};
 for(int i=0;i<3;i++)lv_label_set_text_fmt(lines[i],"%c：%s",'A'+i,roles[key_layouts[selection][i]]);
 lv_label_set_text(lines[3],"实体按键在上方：左 A 中 B 右 C");lv_label_set_text(lines[4],settings_notice);
 lv_label_set_text(footer,"本页固定：A 上项 B 保存 C 下项");
 }else if(page==SETTINGS){lv_label_set_text(title,"显示与按键");
 lv_label_set_text_fmt(lines[0],"%s 屏幕翻转：%d 度",selection==0?">":" ",rotation*90);
 lv_label_set_text_fmt(lines[1],"%s 收发提示：%s",selection==1?">":" ",FMO_HAS_SIDE_LEDS&&breathing?"呼吸灯":"呼号闪烁");
 lv_label_set_text_fmt(lines[2],"%s PTT 保持：%s",selection==2?">":" ",ptt_latch?"开启":"关闭");
 lv_label_set_text(lines[3],ptt_latch?"短按 B 开始，再按停止":"按住 B 发话，松开停止");lv_label_set_text(lines[4],settings_notice);
 lv_label_set_text(footer,"A 上项 B 更改 C 下项 / 长按A返回");
 }else if(page==VOLUME){lv_label_set_text(title,"音量与静音");lv_label_set_text_fmt(lines[0],"扬声器音量：%d%%",a.volume);lv_label_set_text(lines[1],a.muted?"当前静音":"声音已开启");lv_label_set_text(footer,"A 减小  B 静音  C 增大 / 长按A返回");
 }else if(page==IDENTITY){lv_label_set_text(title,"设备身份");lv_label_set_text_fmt(lines[0],"呼号：" FMO_CALL_PREFIX "%s",v.call);lv_label_set_text(lines[1],v.identity);lv_label_set_text_fmt(lines[2],"当前：%s",ca_current[0]?ca_current:"等待读取");lv_label_set_text(lines[3],"B 切换根 CA");lv_obj_set_style_text_font(lines[3],&font20,0);lv_obj_set_style_text_color(lines[3],lv_color_hex(0xffd166),0);lv_obj_set_pos(lines[3],12,158);lv_label_set_text(lines[4],"切换后自动刷新中继候选");lv_obj_set_pos(lines[4],12,194);lv_label_set_text(footer,"长按 A 返回");
 }else if(page==CA_LIST){int total=ca_count?ca_count:1,start=selection/4*4;lv_label_set_text_fmt(title,"根 CA 列表 %d/%d",selection/4+1,(total+3)/4);for(int i=0;i<4;i++){int n=start+i;if(n>=ca_count)lv_label_set_text(lines[i],"");else{device_profile_t*p=&ca_profiles[n];lv_label_set_text_fmt(lines[i],"%s %s%s",selection==n?">":" ",p->current?"[当前] ":!p->has_certificate?"[仅根] ":"",p->name);lv_obj_set_style_text_color(lines[i],lv_color_hex(p->current?0xffd166:0xecf2f8),0);}}lv_label_set_text(lines[4],ca_notice[0]?ca_notice:"无证书可选择，再去网页申请");lv_label_set_text(footer,"A 上项  B 确认  C 下项 / 长按A返回");
 }else if(page==CA_CONFIRM){lv_label_set_text(title,"确认切换根 CA");lv_label_set_text(lines[0],ca_target.name);lv_label_set_text(lines[1],ca_target.has_certificate?"迁移身份请确保原设备离线":"原设备证书保留，暂停通联");lv_obj_set_style_text_font(lines[1],&status_font,0);lv_label_set_text(lines[2],"切换后自动重启");lv_label_set_text(lines[3],ca_target.has_certificate?"刷新中继列表":"请到网页申请证书");for(int i=2;i<=3;i++){lv_obj_set_style_text_font(lines[i],&font20,0);lv_obj_set_style_text_color(lines[i],lv_color_hex(0xffd166),0);lv_obj_set_pos(lines[i],12,122+(i-2)*32);}lv_obj_set_pos(lines[4],12,189);lv_obj_set_style_text_font(lines[4],&status_font,0);lv_label_set_text(lines[4],ca_submitted?(v.profile_message[0]?v.profile_message:ca_notice):"A/C 取消，返回体系列表");lv_obj_set_pos(footer,12,214);lv_obj_set_style_text_font(footer,&font20,0);lv_obj_set_style_text_color(footer,lv_color_hex(0xffd166),0);lv_label_set_text(footer,ca_submitted?"正在切换，请稍候":"B 确认切换");
 }else if(page==WEB){lv_label_set_text(title,"网页管理 / 配网");lv_label_set_text_fmt(lines[0],"热点：%s",v.ap);lv_label_set_text_fmt(lines[1],"热点密码 / 管理码：%s",v.code);lv_label_set_text(lines[2],"热点网页：192.168.4.1");lv_label_set_text_fmt(lines[3],"局域网：%s",v.online?v.ip:"等待联网");
 }else if(page==MIC){lv_label_set_text(title,"麦克风测试");lv_label_set_text_fmt(lines[0],"输入电平：%d / 32768",a.mic_peak);lv_label_set_text(lines[1],"请对着设备说话，观察数值变化");lv_label_set_text(lines[2],"本页只测量，不向中继发送");lv_label_set_text(lines[3],"B 返回通联");}
}
static void tick(void *arg){(void)arg;lv_tick_inc(1);}
static void verify_fonts(void){
 const lv_font_t *fonts[]={&font20,&status_font,&ui_small};
 unsigned missing=0;
 for(unsigned f=0;f<3;f++)for(unsigned i=0;i<sizeof(ui_font_codepoints)/sizeof(ui_font_codepoints[0]);i++){
  lv_font_glyph_dsc_t dsc;
  if(!lv_font_get_glyph_dsc(fonts[f],&dsc,ui_font_codepoints[i],0)){missing++;ESP_LOGE("ui_fonts","missing font=%u U+%04lx",f,(unsigned long)ui_font_codepoints[i]);}
 }
 for(unsigned c=32;c<127;c++){lv_font_glyph_dsc_t d;if(!lv_font_get_glyph_dsc(&ui_call,&d,c,0))missing++;}
 const unsigned char *sample=(const unsigned char*)"西南集群（绵阳节点）重庆臺灣";while(*sample){uint32_t cp;size_t n=name_codepoint(sample,&cp);if(!n){missing++;break;}if(!relay_glyph(cp))missing++;sample+=n;}
 for(unsigned c=48;c<=58;c++){lv_font_glyph_dsc_t d;if(!lv_font_get_glyph_dsc(&ui_clock,&d,c,0))missing++;}
 ESP_LOGI("ui_fonts","coverage: %u codepoints x 3 sizes plus callsign ASCII, missing=%u",(unsigned)(sizeof(ui_font_codepoints)/sizeof(ui_font_codepoints[0])),missing);
 assert(missing==0);
}
static void create_ui(void){
 lv_img_cache_invalidate_src(&sstv_image_desc);lv_obj_clean(lv_scr_act());sstv_image=NULL;shown=-1;
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x0d151d),0);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
 brand=label(FMO_BOARD_TITLE " · " FMO_VERSION,12,8,&status_font,0x9baab8);
 title=label("正在启动网页后台",12,32,&font20,0xf6ad74);
 for(int i=0;i<5;i++)lines[i]=label("",12,70+i*27,&status_font,0xecf2f8);
 footer=label("正在启动",12,220,&status_font,0x70d5ae);lv_label_set_long_mode(footer,LV_LABEL_LONG_SCROLL_CIRCULAR);lv_label_set_long_mode(brand,LV_LABEL_LONG_DOT);
 build_home();
}
void app_main(void){
 #ifdef CONFIG_FMO_BOARD_XINGZHI
 // Power hold from the reference board. Do not use M5 I2C on GPIO21.
 ESP_ERROR_CHECK(rtc_gpio_hold_dis(FMO_POWER_HOLD));ESP_ERROR_CHECK(rtc_gpio_deinit(FMO_POWER_HOLD));
 gpio_config_t power={.pin_bit_mask=1ULL<<FMO_POWER_HOLD,.mode=GPIO_MODE_OUTPUT};ESP_ERROR_CHECK(gpio_config(&power));ESP_ERROR_CHECK(gpio_set_level(FMO_POWER_HOLD,1));
#endif
 gpio_config_t cfg={.pin_bit_mask=(1ULL<<FMO_KEY_A)|(1ULL<<FMO_KEY_B)|(1ULL<<FMO_KEY_C),.mode=GPIO_MODE_INPUT
#ifdef CONFIG_FMO_BOARD_XINGZHI
 ,.pull_up_en=GPIO_PULLUP_ENABLE
#endif
 };ESP_ERROR_CHECK(gpio_config(&cfg));
 side_leds_init();battery_init();
 lv_init();verify_fonts();lv_port_disp_set_rotation(0);lv_port_disp_init();
 create_ui();
 const esp_timer_create_args_t ta={.callback=tick,.name="lv_tick"};esp_timer_handle_t timer;ESP_ERROR_CHECK(esp_timer_create(&ta,&timer));ESP_ERROR_CHECK(esp_timer_start_periodic(timer,1000));
 lv_timer_handler();
 ESP_LOGI("memory_stage","before_store free=%u largest=%u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
 sstv_store_init(NULL);
 ESP_LOGI("memory_stage","after_store free=%u largest=%u",(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
 esp_err_t result=device_web_start();
 if(result!=ESP_OK){lv_label_set_text(title,"USB 后台启动失败");ESP_LOGE("main","init: %s",esp_err_to_name(result));}
 if(result==ESP_OK){load_settings();lv_disp_set_rotation(NULL,(lv_disp_rot_t)rotation);create_ui();}
 int pins[3]={FMO_KEY_A,FMO_KEY_B,FMO_KEY_C};uint8_t initial=0;for(int i=0;i<3;i++)if(!gpio_get_level(pins[i]))initial|=1<<i;buttons_init(&keys,key_layout_mask(ui_key_layout,initial),key,NULL);buttons_set_momentary(&keys,!ptt_latch);int64_t updated=0,led_updated=0;
 while(1){
#ifdef CONFIG_FMO_BOARD_XINGZHI
 unsigned loop_now=(unsigned)(esp_timer_get_time()/1000),last_loop=atomic_load(&ui_loop_ms);if(last_loop)ui_max(&ui_loop_max_ms,loop_now-last_loop);atomic_store(&ui_loop_ms,loop_now);
#endif
 ui_mark(1);resource_metrics_sample();int64_t now=esp_timer_get_time();uint8_t pressed=0;for(int i=0;i<3;i++)if(!gpio_get_level(pins[i]))pressed|=1<<i;if(result==ESP_OK){if(now-led_updated>=25000){voice_view_t led_voice;voice_view(&led_voice);side_leds_update(FMO_HAS_SIDE_LEDS&&breathing&&led_voice.tx,FMO_HAS_SIDE_LEDS&&breathing&&led_voice.rx,now/1000);uint8_t visible_keys=screen_sleep_step(&screen_sleep,led_voice.tx||led_voice.rx,pressed);(void)visible_keys;ui_screen_sleeping=screen_sleep.sleeping;unsigned brightness=screen_sleep.sleeping?0:display_idle_step(&display_idle,now/1000,led_voice.tx||led_voice.rx||pressed);ui_idle_seconds=(now/1000-display_idle.last_activity)/1000;ui_pressed_mask=pressed;if(brightness!=ui_backlight_percent){disp_driver_set_brightness(brightness);ui_backlight_percent=brightness;}led_updated=now;}uint8_t usable=screen_sleep.sleeping||screen_sleep.consume?0:pressed;buttons_step(&keys,page==KEY_LAYOUT?usable:key_layout_mask(ui_key_layout,usable),now/1000,page==HOME);if(now-updated>150000){
#ifdef CONFIG_FMO_BOARD_XINGZHI
 int64_t refresh_start=esp_timer_get_time();
#endif
 refresh();updated=now;
#ifdef CONFIG_FMO_BOARD_XINGZHI
 ui_max(&ui_refresh_max_ms,(unsigned)((esp_timer_get_time()-refresh_start)/1000));
#endif
 }}
#ifdef CONFIG_FMO_BOARD_XINGZHI
 ui_mark(6);int64_t render_start=esp_timer_get_time();
#endif
#ifdef FMO_SSTV_STREAMING
 if(page!=SSTV&&page!=GALLERY)lv_timer_handler();
 else if(sstv_rx_display_lock()){lv_timer_handler();sstv_rx_display_unlock();}
#else
 lv_timer_handler();
#endif
#ifdef CONFIG_FMO_BOARD_XINGZHI
 ui_max(&ui_render_max_ms,(unsigned)((esp_timer_get_time()-render_start)/1000));ui_mark(0);
#endif
 vTaskDelay(pdMS_TO_TICKS(10));}
}
