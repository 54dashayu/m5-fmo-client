#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "device_web.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
extern const lv_font_t font20, status_font, ui_small, ui_call;
#include "fonts/coverage.h"
#include "buttons.h"
#include "voice.h"
#include "relays.h"
static lv_obj_t *title,*lines[5],*footer,*brand;
static lv_obj_t *home,*net,*caption,*badge,*call_label,*hint,*relay_name,*relay_id,*relay_info,*home_notice,*b_hint,*bars[9];
static buttons_t keys;
unsigned ui_key_events;int ui_last_key=-1,ui_last_action=-1;
enum {HOME,MENU,RELAYS,VOLUME,IDENTITY,WEB,MIC};
static int page=HOME,selection=0;
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
 home_label("FMO / M5",12,8,104,&status_font,0xd4e4eb);
 net=home_label("Wi-Fi · 未联网",124,9,184,&ui_small,0xb4c7d2);lv_obj_set_style_text_align(net,LV_TEXT_ALIGN_RIGHT,0);
 lv_obj_t*card=box(home,10,32,300,110,0x14232d,12);lv_obj_set_style_border_width(card,1,0);lv_obj_set_style_border_color(card,lv_color_hex(0x263b48),0);
 caption=home_label("我的呼号",23,43,180,&ui_small,0xb4c7d2);
 badge=home_label("未接入",240,42,58,&ui_small,0xa8b5bf);lv_obj_set_style_text_align(badge,LV_TEXT_ALIGN_CENTER,0);lv_obj_set_style_bg_opa(badge,LV_OPA_COVER,0);lv_obj_set_style_bg_color(badge,lv_color_hex(0x263139),0);lv_obj_set_style_radius(badge,9,0);lv_obj_set_style_pad_ver(badge,3,0);
 call_label=home_label("M.BH1JSS",22,65,274,&ui_call,0xeaf4f7);
 hint=home_label("等待重新接入",23,116,216,&ui_small,0xc2d3de);
 for(int i=0;i<9;i++)bars[i]=box(home,247+i*5,124,3,3,0x58d6b2,1);
 relay_name=home_label("",12,151,245,&status_font,0xe0ebf1);
 relay_id=home_label("",259,155,49,&status_font,0xb4c7d2);lv_obj_set_style_text_align(relay_id,LV_TEXT_ALIGN_RIGHT,0);
 relay_info=home_label("",12,169,296,&ui_small,0xa9bfcd);
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
 lv_label_set_text(net,!v->online?"Wi-Fi · 未联网":v->mqtt_online?"Wi-Fi · 已连接":"Wi-Fi 已连接 · 中继断开");
 lv_label_set_text(caption,a->rx&&!a->tx?"正在接收 · 对方呼号":"我的呼号");
 lv_label_set_text(badge,a->tx?"发话中":a->rx?"接收中":v->mqtt_online?"守听中":"未接入");
 lv_obj_set_style_text_color(badge,lv_color_hex(accent),0);lv_obj_set_style_bg_color(badge,lv_color_hex(tint),0);
 if(a->rx&&!a->tx)lv_label_set_text(call_label,a->caller[0]?a->caller:"--");else lv_label_set_text_fmt(call_label,"M.%s",v->call);
 // Long legal callsigns still fit the card instead of being cropped.
 lv_obj_set_style_text_font(call_label,strlen(lv_label_get_text(call_label))>12?&font20:&ui_call,0);
 if(a->tx)lv_label_set_text(hint,"再次短按 B 停止");else if(a->rx)lv_label_set_text_fmt(hint,"本机 M.%s",v->call);else lv_label_set_text(hint,v->mqtt_online?"频道空闲 · 随时呼叫":"等待重新接入");
 lv_label_set_text(relay_name,r->name[0]?r->name:r->call);lv_label_set_text_fmt(relay_id,"%lu",(unsigned long)r->uid);
 if(a->muted)lv_label_set_text_fmt(relay_info,"%s · 静音",v->mqtt_online?"中继已接入":"等待接入");else lv_label_set_text_fmt(relay_info,"%s · 音量 %d%%",v->mqtt_online?"中继已接入":"等待接入",a->volume);
 lv_label_set_text(home_notice,a->notice[0]?a->notice:"短按 B 开始通联");lv_label_set_text(b_hint,a->tx?"B  停止发话":"B  开始发话");lv_obj_set_style_text_color(b_hint,lv_color_hex(accent),0);
 int peak=a->tx?a->mic_peak:a->rx?a->speaker_peak:0;int level=peak/1800;if(level>9)level=9;
 for(int i=0;i<9;i++){int h=i<level?5+(i%3)*4:3;lv_obj_set_pos(bars[i],247+i*5,127-h);lv_obj_set_height(bars[i],h);lv_obj_set_style_bg_color(bars[i],lv_color_hex(i<level?accent:0x35505f),0);}
}
static void go(int next){voice_ptt(false);voice_mic_test(false);page=next;device_management_ap(next==WEB);selection=0;buttons_block(&keys);if(next==RELAYS)selection=relays_current(NULL,NULL);if(next==MIC)voice_mic_test(true);}
static void key(int k,key_action_t action,void*ctx){(void)ctx;ui_key_events++;ui_last_key=k;ui_last_action=action;ESP_LOGI("buttons","key=%d action=%d",k,action);
 if(action==KEY_PTT_STOP||action==KEY_CANCEL){voice_ptt(false);return;}
 if(action==KEY_PTT_START){if(page==HOME)voice_ptt(true);return;}
 if(action==KEY_LONG){if(page==HOME){if(k==0)go(RELAYS);if(k==1)voice_ptt(false);if(k==2)go(MENU);}else if(k==0)go(HOME);return;}
 if(action!=KEY_SHORT)return;
 if(page==HOME){if(k==0)voice_volume(-5);if(k==1)voice_toggle_ptt();if(k==2)voice_volume(5);return;}
 if(page==MENU){if(k==0)selection=(selection+6)%7;if(k==2)selection=(selection+1)%7;if(k==1){int target=selection;if(target==0)go(RELAYS);else if(target==1)go(VOLUME);else if(target==2)go(IDENTITY);else if(target==3)go(WEB);else if(target==4)voice_tone();else if(target==5)go(MIC);else go(HOME);}return;}
 if(page==RELAYS){int n=relays_count();if(k==0)selection=(selection+n-1)%n;if(k==2)selection=(selection+1)%n;if(k==1&&device_select_relay(selection))go(HOME);return;}
 if(page==VOLUME){if(k==0)voice_volume(-5);if(k==2)voice_volume(5);if(k==1)voice_mute();return;}
 if(k==1)go(HOME);
}
static void refresh(void){
 device_view_t v;device_web_view(&v);voice_view_t a;voice_view(&a);relay_t relay;int current=relays_current(&relay,NULL);
 static int shown=-1;if(shown!=page){for(int i=0;i<5;i++)lv_label_set_text(lines[i],"");shown=page;bool is_home=page==HOME;if(is_home)lv_obj_clear_flag(home,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(home,LV_OBJ_FLAG_HIDDEN);lv_obj_t*old[]={brand,title,footer,lines[0],lines[1],lines[2],lines[3],lines[4]};for(unsigned i=0;i<8;i++){if(is_home)lv_obj_add_flag(old[i],LV_OBJ_FLAG_HIDDEN);else lv_obj_clear_flag(old[i],LV_OBJ_FLAG_HIDDEN);}}
 lv_label_set_text(footer,"长按 A 返回       B 确认");
 if(page==HOME){
  refresh_home(&v,&a,&relay);
 }else if(page==MENU){lv_label_set_text(title,"设备菜单");for(int i=0;i<5;i++){int n=(selection/5)*5+i;if(n<7)lv_label_set_text_fmt(lines[i],"%s %s",n==selection?">":" ",items[n]);else lv_label_set_text(lines[i],"");}lv_label_set_text(footer,"A 上项  B 确认  C 下项 / 长按A返回");
 }else if(page==RELAYS){lv_label_set_text(title,"选择中继");for(int i=0;i<relays_count();i++){relay_t r;relays_get(i,&r);lv_label_set_text_fmt(lines[i],"%s %s / %lu %s",selection==i?">":" ",r.call,(unsigned long)r.uid,current==i?"当前":"");}lv_label_set_text(lines[4],"在网页添加中继；通联中不能切换");lv_label_set_text(footer,"A 上项  B 接入  C 下项 / 长按A返回");
 }else if(page==VOLUME){lv_label_set_text(title,"音量与静音");lv_label_set_text_fmt(lines[0],"扬声器音量：%d%%",a.volume);lv_label_set_text(lines[1],a.muted?"当前静音":"声音已开启");lv_label_set_text(footer,"A 减小  B 静音  C 增大 / 长按A返回");
 }else if(page==IDENTITY){lv_label_set_text(title,"设备身份");lv_label_set_text_fmt(lines[0],"呼号：M.%s",v.call);lv_label_set_text(lines[1],v.identity);lv_label_set_text(lines[2],"认证使用证书中的裸呼号");lv_label_set_text(lines[3],"私钥保留在本机");
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
 ESP_LOGI("ui_fonts","coverage: %u codepoints x 3 sizes plus callsign ASCII, missing=%u",(unsigned)(sizeof(ui_font_codepoints)/sizeof(ui_font_codepoints[0])),missing);
 assert(missing==0);
}
void app_main(void){
 gpio_config_t cfg={.pin_bit_mask=(1ULL<<39)|(1ULL<<38)|(1ULL<<37),.mode=GPIO_MODE_INPUT};ESP_ERROR_CHECK(gpio_config(&cfg));
 lv_init();verify_fonts();lv_port_disp_set_rotation(0);lv_port_disp_init();
 lv_obj_set_style_bg_color(lv_scr_act(),lv_color_hex(0x0d151d),0);lv_obj_clear_flag(lv_scr_act(),LV_OBJ_FLAG_SCROLLABLE);
 brand=label("FMO / M5 FIRE  ·  0.7.0",12,8,&status_font,0x9baab8);
 title=label("正在启动网页后台",12,32,&font20,0xf6ad74);
 for(int i=0;i<5;i++)lines[i]=label("",12,70+i*27,&status_font,0xecf2f8);
 footer=label("正在启动",12,220,&status_font,0x70d5ae);
 build_home();
 const esp_timer_create_args_t ta={.callback=tick,.name="lv_tick"};esp_timer_handle_t timer;ESP_ERROR_CHECK(esp_timer_create(&ta,&timer));ESP_ERROR_CHECK(esp_timer_start_periodic(timer,1000));
 lv_timer_handler();esp_err_t result=device_web_start();
 if(result!=ESP_OK){lv_label_set_text(title,"USB 后台启动失败");ESP_LOGE("main","init: %s",esp_err_to_name(result));}
 int pins[3]={39,38,37};uint8_t initial=0;for(int i=0;i<3;i++)if(!gpio_get_level(pins[i]))initial|=1<<i;buttons_init(&keys,initial,key,NULL);int64_t updated=0;
 while(1){int64_t now=esp_timer_get_time();uint8_t pressed=0;for(int i=0;i<3;i++)if(!gpio_get_level(pins[i]))pressed|=1<<i;if(result==ESP_OK){buttons_step(&keys,pressed,now/1000,page==HOME);if(now-updated>150000){refresh();updated=now;}}lv_timer_handler();vTaskDelay(pdMS_TO_TICKS(10));}
}
