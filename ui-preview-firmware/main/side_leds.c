#include "side_leds.h"
#include "led_strip.h"
#include "esp_log.h"
#include <stddef.h>
#ifdef ESP_PLATFORM
#include "soc/gpio_struct.h"
#endif
// Same Espressif led_strip 3.0.3 backend/configuration as the M5 Core V2.0
// companion. Do not reconfigure GPIO15 after the driver routes RMT to it.
#define PIXELS 10
static led_strip_handle_t strip;
static side_leds_view_t status;
static int previous=-1,last_color=-1,last_level=-1;
static const uint8_t breath[64]={1,1,1,1,2,2,3,4,4,5,6,7,8,9,10,11,12,14,15,16,17,18,19,20,21,21,22,23,23,24,24,24,24,24,24,24,23,23,22,21,21,20,19,18,17,16,15,14,13,11,10,9,8,7,6,5,4,4,3,2,2,1,1,1};
static int64_t changed_at;
void side_leds_view(side_leds_view_t*out){*out=status;
#ifdef ESP_PLATFORM
 out->route=GPIO.func_out_sel_cfg[15].func_sel;out->open_drain=GPIO.pin[15].pad_driver;
#endif
}
static bool show(int color,int level){
 esp_err_t e=ESP_OK;
 for(int i=0;i<PIXELS&&e==ESP_OK;i++)e=led_strip_set_pixel(strip,i,color==2?level:0,color==1?level:0,0);
 if(e==ESP_OK)e=led_strip_refresh(strip);
 status.error=e;
 if(e==ESP_OK){status.writes++;status.color=color;status.brightness=level;return true;}
 status.failures++;return false;
}
void side_leds_init(void){
 led_strip_config_t cfg={.strip_gpio_num=15,.max_leds=PIXELS,.led_model=LED_MODEL_WS2812,.color_component_format=LED_STRIP_COLOR_COMPONENT_FMT_GRB,.flags={.invert_out=false}};
 led_strip_rmt_config_t rmt={.clk_src=RMT_CLK_SRC_DEFAULT,.resolution_hz=10000000,.mem_block_symbols=256,.flags={.with_dma=false}};
 esp_err_t e=led_strip_new_rmt_device(&cfg,&rmt,&strip);
 status.ready=e==ESP_OK;status.error=e;
 if(status.ready){show(0,0);ESP_LOGI("side_leds","FIRE GPIO15 GRB x10 ready via led_strip; brightness=24/255");}
 else ESP_LOGW("side_leds","LED unavailable: %s; audio remains enabled",esp_err_to_name(e));
}
void side_leds_update(bool tx,bool rx,int64_t now_ms){
 if(!status.ready)return;
 int mode=tx?2:rx?1:0;
 if(mode!=previous){previous=mode;changed_at=now_ms;}
 int color=mode;int level=mode?breath[((now_ms-changed_at)/25)%64]:0;
 if((color!=last_color||level!=last_level)&&show(color,level)){last_color=color;last_level=level;}
}
