#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "disp_driver.h"
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
#include "battery.h"
#include "side_leds.h"
#include "relays.h"
#include "discovery.h"
#include "utf8_name.h"
static bool relay_glyph(uint32_t c){lv_font_glyph_dsc_t d;return lv_font_get_glyph_dsc(&relay_font,&d,c,0);}
static lv_obj_t *title,*lines[5],*footer,*brand;
static lv_obj_t *home,*battery_label,*battery_fill,*local_call,*net,*caption,*badge,*call_label,*hint,*relay_name,*relay_id,*home_notice,*last_call,*previous_call,*b_hint,*bars[9];
static buttons_t keys;
static display_idle_t display_idle;
unsigned ui_backlight_percent=100,ui_idle_seconds,ui_pressed_mask;
unsigned ui_key_events;int ui_last_key=-1,ui_last_action=-1;
enum {HOME,MENU,RELAYS,VOLUME,IDENTITY,WEB,MIC,DISCOVER,CA_LIST,CA_CONFIRM};
static int page=HOME,selection=0;
static device_profile_t ca_profiles[DEVICE_PROFILE_MAX],ca_target;
static int ca_count;
static bool ca_submitted;
static char ca_notice[96],ca_current[97];
static void load_ca_profiles(void){ca_count=device_profiles(ca_profiles);ca_notice[0]=0;ca_current[0]=0;if(ca_count<0){ca_count=0;snprintf(ca_notice,sizeof(ca_notice),"通联或后台忙，请稍后按 B 刷新");}for(int i=0;i<ca_count;i++)if(ca_profiles[i].current)snprintf(ca_current,sizeof(ca_current),"%s",ca_profiles[i].name);}

static relay_t discovered_ui[DISCOVERY_MAX];static int discovered_count;static uint32_t discovered_revision;static char discovery_notice[96];
static bool idle_started=false,was_active=false,show_clock=false;
static int64_t idle_since;
static char held_call[16];
static const char*items[]={"选择中继","音量与静音","设备身份","网页管理","扬声器测试","麦克风测试","返回通联"};
static void set_text(lv_obj_t*o,const char*s){if(strcmp(lv_label_get_text(o),s))lv_label_set_text(o,s);}
static void set_text_fmt(lv_obj_t*o,const char*fmt,...){char text[256];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);set_text(o,text);}
#define lv_label_set_text set_text
#define lv_label_set_text_fmt set_text_fmt
static lv_obj_t *label(const char *text,int x,int y,const lv_font_t *font,uint32_t color){
 lv_obj_t *o=lv_label_create(lv_scr_act());lv_label_set_text(o,text);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,296);lv_obj_set_style_text_font(o,font,0);lv_obj_set_style_text_color(o,lv_color_hex(color),0);return o;
}
static lv_obj_t *box(lv_obj_t*p,int x,int y,int w,int h,uint32_t bg,int radius){
 lv_obj_t*o=lv_obj_create(p);lv_obj_remove_style_all(o);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);lv_obj_set_style_bg_color(o,lv_color_hex(bg),0);lv_obj_set_style_radius(o,radius,0);lv_obj_clear_flag(o,LV_OBJ_FLAG_SCROLLABLE);return o;
}
static lv_obj_t *home_label(const char*t,int x,int y,int w,const lv_font_t*f,uint32_t c){
 lv_obj_t*o=lv_label_create(home);lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);lv_obj_set_style_text_font(o,f,0);lv_obj_set_style_text_color(o,lv_color_hex(c),0);lv_label_set_long_mode(o,LV_LABEL_LONG_DOT);lv_label_set_text(o,t);return o;
}
static void build_home(void){
 home=box(lv_scr_act(),0,0,320,240,0x0b151c,0);
 home_label("FMO / M5",12,8,72,&status_font,0xd4e4eb);
 local_call=home_label("--",90,8,102,&status_font,0x7ec9ff);
 lv_label_set_long_mode(local_call,LV_LABEL_LONG_SCROLL_CIRCULAR);
 net=home_label("Wi-Fi",198,9,38,&ui_small,0xb4c7d2);lv_obj_set_style_text_align(net,LV_TEXT_ALIGN_RIGHT,0);
 lv_obj_t*battery_box=box(home,242,11,22,12,0x0b151c,2);lv_obj_set_style_border_width(battery_box,1,0);lv_obj_set_style_border_color(battery_box,lv_color_hex(0x9cb2bf),0);box(home,264,14,2,6,0x9cb2bf,1);battery_fill=box(home,244,13,18,8,0x58d6b2,1);battery_label=home_label("--",269,9,40,&ui_small,0xb4c7d2);
 lv_obj_t*card=box(home,10,32,300,93,0x14232d,12);lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0x263b48),0);
 caption=home_label("我的呼号",23,43,180,&ui_small,0xb4c7d2);
 badge=home_label("未接入",240,42,58,&ui_small,0xa8b5bf);lv_obj_set_style_text_align(badge,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_bg_opa(badge,LV_OPA_COVER,0);lv_obj_set_style_bg_color(badge,lv_color_hex(0x263139),0);lv_obj_set_style_radius(badge,9,0);lv_obj_set_style_pad_ver(badge,3,0);
 call_label=home_label("M.BH1JSS",22,64,276,&ui_call,0xffd166);
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
 lv_obj_t*o=home_label("A  音量 -",0,211,106,&ui_small,0xd4e4eb);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
 b_hint=home_label("B  开始发话",106,211,108,&ui_small,0x58d6b2);lv_obj_set_style_text_align(b_hint,LV_TEXT_ALIGN_CENTER,0);
 o=home_label("C  音量 +",214,211,106,&ui_small,0xd4e4eb);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
 const char*sub[]={"长按选中继","短按开始 / 停止","长按进菜单"};for(int i=0;i<3;i++){o=home_label(sub[i],i*107,225,106,&ui_small,0xa9bfcd);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);}
 lv_obj_add_flag(home,LV_OBJ_FLAG_HIDDEN);
}
static void refresh_home(const device_view_t*v,const voice_view_t*a,const relay_t*r){
 uint32_t accent=a->tx?0xffb778:a->rx?0x7ec9ff:v->mqtt_online?0x58d6b2:0xa8b5bf;
 uint32_t tint=a->tx?0x3b2a20:a->rx?0x193348:v->mqtt_online?0x12352f:0x263139;
 int percent;bool charging;battery_view(&percent,&charging);if(percent<0)lv_label_set_text(battery_label,"--");else lv_label_set_text_fmt(battery_label,"%d%%%s",percent,charging?"+":"");if(percent<=0)lv_obj_add_flag(battery_fill,LV_OBJ_FLAG_HIDDEN);else{lv_obj_clear_flag(battery_fill,LV_OBJ_FLAG_HIDDEN);lv_obj_set_width(battery_fill,(18*percent+99)/100);}lv_obj_set_style_bg_color(battery_fill,lv_color_hex(charging?0x5ceaff:percent<=25?0xff7766:0x58d6b2),0);
 lv_label_set_text_fmt(local_call,"M.%s",v->call);
 lv_label_set_text(net,v->online?"Wi-Fi":"未联网");
 lv_obj_set_style_text_color(net,lv_color_hex(v->online?0x58d6b2:0xffb778),0);
 lv_label_set_text(caption,a->rx&&!a->tx?"当前通联 · 对方呼号":a->tx?"当前通联 · 本机发话":show_clock?"北京时间":"最近通联");
 lv_label_set_text(badge,a->tx?"发话中":a->rx?"接收中":v->mqtt_online?"守听中":"未接入");
 lv_obj_set_style_text_color(badge,lv_color_hex(accent),0);lv_obj_set_style_bg_color(badge,lv_color_hex(tint),0);
 time_t utc=time(NULL);bool idle=show_clock;bool pending_clock=idle&&utc<1700000000;
 if(a->rx&&!a->tx)lv_label_set_text(call_label,a->caller[0]?a->caller:"--");
 else if(a->tx)lv_label_set_text_fmt(call_label,"M.%s",v->call);
 else if(!idle)lv_label_set_text(call_label,held_call);
 else if(pending_clock)lv_label_set_text(call_label,"等待校时");
 else {time_t china=utc+8*3600;struct tm t;gmtime_r(&china,&t);char clock[16];strftime(clock,sizeof(clock),"%H:%M:%S",&t);lv_label_set_text(call_label,clock);}
 lv_obj_set_style_text_align(call_label,idle?LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
 // Keep the large callsign font; scroll only when it exceeds the card width.
 lv_obj_set_style_text_font(call_label,pending_clock?&font20:idle?&ui_clock:&ui_call,0);
 lv_obj_set_y(call_label,64);
 lv_label_set_long_mode(call_label,LV_LABEL_LONG_SCROLL_CIRCULAR);
 lv_obj_set_style_text_color(call_label,lv_color_hex(0xffd166),0);
 if(a->tx)lv_label_set_text(hint,"再次短按 B 停止");else lv_label_set_text(hint,idle?"":a->rx?"接收中":v->mqtt_online?"守听中":"中继断开");
 lv_label_set_text(last_call,a->last_caller[0]?a->last_caller:"--");
 lv_label_set_text(previous_call,a->previous_caller[0]?a->previous_caller:"--");
 char name[65];name_for_display(name,sizeof(name),r->name[0]?r->name:r->call,relay_glyph);lv_label_set_text(relay_name,name);lv_label_set_text_fmt(relay_id,"%lu",(unsigned long)r->uid);
 if(a->muted)lv_label_set_text_fmt(home_notice,"%s · 静音",a->notice[0]?a->notice:"守听中");else lv_label_set_text_fmt(home_notice,"%s · 音量 %d%%",a->notice[0]?a->notice:"守听中",a->volume);lv_label_set_text(b_hint,a->tx?"B  停止发话":"B  开始发话");lv_obj_set_style_text_color(b_hint,lv_color_hex(accent),0);
 int peak=a->tx?a->mic_peak:a->rx?a->speaker_peak:0;int level=peak/1800;if(level>9)level=9;
 for(int i=0;i<9;i++){int h=i<level?5+(i%3)*4:3;lv_obj_set_pos(bars[i],247+i*5,119-h);lv_obj_set_height(bars[i],h);lv_obj_set_style_bg_color(bars[i],lv_color_hex(i<level?accent:0x35505f),0);}
}
static void go(int next){voice_ptt(false);voice_mic_test(false);page=next;device_management_ap(next==WEB);selection=0;buttons_block(&keys);if(next==RELAYS)selection=relays_current(NULL,NULL);if(next==CA_CONFIRM)ca_submitted=false;if(next==MIC)voice_mic_test(true);if(next==IDENTITY||next==CA_LIST)load_ca_profiles();if(next==DISCOVER){discovered_count=discovery_list(discovered_ui,&discovered_revision);discovery_refresh();discovery_notice[0]=0;}}
static void key(int k,key_action_t action,void*ctx){(void)ctx;ui_key_events++;ui_last_key=k;ui_last_action=action;ESP_LOGI("buttons","key=%d action=%d",k,action);
 if(action==KEY_PTT_STOP||action==KEY_CANCEL){voice_ptt(false);return;}
 if(action==KEY_PTT_START){if(page==HOME)voice_ptt(true);return;}
 if(action==KEY_LONG){if(page==HOME){if(k==0)go(RELAYS);if(k==1)voice_ptt(false);if(k==2)go(MENU);}else if(k==0)go(HOME);return;}
 if(action!=KEY_SHORT)return;
 if(page==HOME){if(k==0)voice_volume(-5);if(k==1)voice_toggle_ptt();if(k==2)voice_volume(5);return;}
 if(page==MENU){if(k==0)selection=(selection+6)%7;if(k==2)selection=(selection+1)%7;if(k==1){int target=selection;if(target==0)go(RELAYS);else if(target==1)go(VOLUME);else if(target==2)go(IDENTITY);else if(target==3)go(WEB);else if(target==4)voice_tone();else if(target==5)go(MIC);else go(HOME);}return;}
 if(page==RELAYS){int n=relays_count(),pos=relays_order_position(selection);if(k==0){pos=(pos+n)%(n+1);selection=pos==n?-1:relays_order_at(pos);}if(k==2){pos=(pos+1)%(n+1);selection=pos==n?-1:relays_order_at(pos);}if(k==1){if(selection<0)go(DISCOVER);else if(device_select_relay(selection))go(HOME);}return;}
 if(page==DISCOVER){int n=discovered_count+1;if(k==0)selection=(selection+n-1)%n;if(k==2)selection=(selection+1)%n;if(k==1){if(selection==discovered_count){discovery_refresh();discovered_count=discovery_list(discovered_ui,&discovered_revision);selection=0;snprintf(discovery_notice,sizeof(discovery_notice),"空闲时刷新，稍后再查看");}else{const char*error=discovery_select(selection,discovered_revision);if(!error)go(HOME);else snprintf(discovery_notice,sizeof(discovery_notice),"%s",error);}}return;}
 if(page==IDENTITY){if(k==1)go(CA_LIST);return;}
 if(page==CA_LIST){if(!ca_count){if(k==1)load_ca_profiles();return;}if(k==0)selection=(selection+ca_count-1)%ca_count;if(k==2)selection=(selection+1)%ca_count;if(k==1){device_profile_t*p=&ca_profiles[selection];if(p->current&&p->active)snprintf(ca_notice,sizeof(ca_notice),"该体系正在使用");else if(p->current&&!p->has_certificate)go(WEB);else{ca_target=*p;go(CA_CONFIRM);}}return;}
 if(page==CA_CONFIRM){if(ca_submitted)return;if(k==0||k==2){go(CA_LIST);return;}if(k==1){if(device_profile_switch(ca_target.id)){ca_submitted=true;snprintf(ca_notice,sizeof(ca_notice),"切换后自动刷新中继列表");buttons_block(&keys);}else snprintf(ca_notice,sizeof(ca_notice),"通联中或已有请求，请稍后重试");}return;}
 if(page==VOLUME){if(k==0)voice_volume(-5);if(k==2)voice_volume(5);if(k==1)voice_mute();return;}
 if(k==1)go(HOME);
}
static void refresh(void){
 device_view_t v;device_web_view(&v);voice_view_t a;voice_view(&a);
 int64_t now=esp_timer_get_time();bool active=a.tx||a.rx;
 if(!idle_started||active||was_active){idle_since=now;idle_started=true;}
 was_active=active;show_clock=!active&&now-idle_since>=10000000;
 if(a.tx||!held_call[0])snprintf(held_call,sizeof(held_call),"M.%s",v.call);
 if(a.rx&&!a.tx)snprintf(held_call,sizeof(held_call),"%s",a.caller[0]?a.caller:"--");
 relay_t relay;int current=relays_current(&relay,NULL);
 static int shown=-1;if(shown!=page){for(int i=0;i<5;i++){lv_label_set_text(lines[i],"");lv_obj_set_pos(lines[i],12,70+i*27);lv_obj_set_style_text_color(lines[i],lv_color_hex(0xecf2f8),0);lv_obj_set_style_text_font(lines[i],(page==RELAYS||page==DISCOVER||page==CA_LIST||page==CA_CONFIRM||page==IDENTITY)?&relay_font:&status_font,0);lv_label_set_long_mode(lines[i],LV_LABEL_LONG_DOT);}shown=page;bool is_home=page==HOME;if(is_home)lv_obj_clear_flag(home,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(home,LV_OBJ_FLAG_HIDDEN);lv_obj_t*old[]={brand,title,footer,lines[0],lines[1],lines[2],lines[3],lines[4]};for(unsigned i=0;i<8;i++){if(is_home)lv_obj_add_flag(old[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(old[i],LV_OBJ_FLAG_HIDDEN);}}
 lv_label_set_text_fmt(brand,(page==RELAYS||page==DISCOVER)?"体系：%s":"FMO / M5 FIRE · %s",(page==RELAYS||page==DISCOVER)?relays_profile_name():"0.7.29");
 lv_obj_set_pos(footer,12,220);lv_obj_set_style_text_font(footer,&status_font,0);lv_obj_set_style_text_color(footer,lv_color_hex(0x70d5ae),0);lv_label_set_text(footer,"长按 A 返回       B 确认");
 if(page==HOME){
  refresh_home(&v,&a,&relay);
 }else if(page==MENU){lv_label_set_text_fmt(title,"设备菜单  %d/2",selection/5+1);for(int i=0;i<5;i++){int n=(selection/5)*5+i;if(n<7)lv_label_set_text_fmt(lines[i],"%s %s",n==selection?">":" ",items[n]);else lv_label_set_text(lines[i],"");}lv_label_set_text(footer,"A 上项  B 确认  C 下项 / 长按A返回");
 }else if(page==RELAYS){int n=relays_count();if(selection>=n)selection=current<n?current:-1;int pos=relays_order_position(selection);int total=n+1,start=pos/4*4;lv_label_set_text_fmt(title,"选择中继 %d/%d",pos/4+1,(total+3)/4);for(int i=0;i<4;i++){int p=start+i;lv_obj_set_style_text_color(lines[i],lv_color_hex(p==n?0xffc567:0xecf2f8),0);if(p>=total)lv_label_set_text(lines[i],"");else if(p==n)lv_label_set_text_fmt(lines[i],"%s + 发现中继",selection<0?">":" ");else{int index=relays_order_at(p);relay_t r={0};if(!relays_get(index,&r)){lv_label_set_text(lines[i],"");continue;}char name[65];name_for_display(name,sizeof(name),r.name[0]?r.name:r.call,relay_glyph);lv_label_set_text_fmt(lines[i],"%s %s%s / %s",selection==index?">":" ",current==index?"[当前] ":"",name,r.call);}}lv_label_set_text_fmt(lines[4],"体系：%s",relays_profile_name());lv_label_set_text(footer,"A 上项  B 确认  C 下项 / 长按A返回");
 }else if(page==DISCOVER){relay_t fresh[DISCOVERY_MAX];uint32_t rev;int num=discovery_list(fresh,&rev);if(rev!=discovered_revision||num!=discovered_count){relay_t old={0};bool was_refresh=selection==discovered_count;if(selection<discovered_count)old=discovered_ui[selection];memcpy(discovered_ui,fresh,num*sizeof(relay_t));discovered_count=num;discovered_revision=rev;discovery_notice[0]=0;selection=was_refresh?num:0;for(int i=0;!was_refresh&&i<num;i++)if(fresh[i].uid==old.uid&&!strcmp(fresh[i].fingerprint,old.fingerprint))selection=i;}int total=discovered_count+1,start=selection/4*4;lv_label_set_text_fmt(title,"发现中继 %d/%d",selection/4+1,(total+3)/4);for(int i=0;i<4;i++){int p=start+i;if(p>=total)lv_label_set_text(lines[i],"");else if(p==discovered_count)lv_label_set_text_fmt(lines[i],"%s 刷新列表",selection==p?">":" ");else{char name[65];name_for_display(name,sizeof(name),discovered_ui[p].name,relay_glyph);lv_label_set_text_fmt(lines[i],"%s %s",selection==p?">":" ",name);}}char progress[96];discovery_status(progress,sizeof(progress));lv_label_set_text(lines[4],discovery_notice[0]?discovery_notice:progress);lv_label_set_text(footer,"A 上项  B 选择  C 下项 / 长按A返回");
 }else if(page==VOLUME){lv_label_set_text(title,"音量与静音");lv_label_set_text_fmt(lines[0],"扬声器音量：%d%%",a.volume);lv_label_set_text(lines[1],a.muted?"当前静音":"声音已开启");lv_label_set_text(footer,"A 减小  B 静音  C 增大 / 长按A返回");
 }else if(page==IDENTITY){lv_label_set_text(title,"设备身份");lv_label_set_text_fmt(lines[0],"呼号：M.%s",v.call);lv_label_set_text(lines[1],v.identity);lv_label_set_text_fmt(lines[2],"当前：%s",ca_current[0]?ca_current:"等待读取");lv_label_set_text(lines[3],"B 切换根 CA");lv_obj_set_style_text_font(lines[3],&font20,0);lv_obj_set_style_text_color(lines[3],lv_color_hex(0xffd166),0);lv_obj_set_pos(lines[3],12,158);lv_label_set_text(lines[4],"切换后自动刷新中继候选");lv_obj_set_pos(lines[4],12,194);lv_label_set_text(footer,"长按 A 返回");
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
void app_main(void){
 gpio_config_t cfg={.pin_bit_mask=(1ULL<<39)|(1ULL<<38)|(1ULL<<37),.mode=GPIO_MODE_INPUT};ESP_ERROR_CHECK(gpio_config(&cfg));
 side_leds_init();battery_init();
 lv_init();verify_fonts();lv_port_disp_set_rotation(0);lv_port_disp_init();
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x0d151d),0);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
 brand=label("FMO / M5 FIRE  ·  0.7.29",12,8,&status_font,0x9baab8);
 title=label("正在启动网页后台",12,32,&font20,0xf6ad74);
 for(int i=0;i<5;i++)lines[i]=label("",12,70+i*27,&status_font,0xecf2f8);
 footer=label("正在启动",12,220,&status_font,0x70d5ae);
 build_home();
 const esp_timer_create_args_t ta={.callback=tick,.name="lv_tick"};esp_timer_handle_t timer;ESP_ERROR_CHECK(esp_timer_create(&ta,&timer));ESP_ERROR_CHECK(esp_timer_start_periodic(timer,1000));
 lv_timer_handler();esp_err_t result=device_web_start();
 if(result!=ESP_OK){lv_label_set_text(title,"USB 后台启动失败");ESP_LOGE("main","init: %s",esp_err_to_name(result));}
 int pins[3]={39,38,37};uint8_t initial=0;for(int i=0;i<3;i++)if(!gpio_get_level(pins[i]))initial|=1<<i;buttons_init(&keys,initial,key,NULL);int64_t updated=0,led_updated=0;
 while(1){int64_t now=esp_timer_get_time();uint8_t pressed=0;for(int i=0;i<3;i++)if(!gpio_get_level(pins[i]))pressed|=1<<i;if(result==ESP_OK){if(now-led_updated>=25000){voice_view_t led_voice;voice_view(&led_voice);side_leds_update(led_voice.tx,led_voice.rx,now/1000);unsigned brightness=display_idle_step(&display_idle,now/1000,led_voice.tx||led_voice.rx||pressed);ui_idle_seconds=(now/1000-display_idle.last_activity)/1000;ui_pressed_mask=pressed;if(brightness!=ui_backlight_percent){disp_driver_set_brightness(brightness);ui_backlight_percent=brightness;}led_updated=now;}buttons_step(&keys,pressed,now/1000,page==HOME);if(now-updated>150000){refresh();updated=now;}}lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(10));}
}
