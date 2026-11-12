#include "board.h"
#include "battery.h"
#include "side_leds.h"
#include "battery_curve.h"
#include "esp_adc/adc_oneshot.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdatomic.h>
#include <string.h>
static adc_oneshot_unit_handle_t battery_adc;
static atomic_int reading=ATOMIC_VAR_INIT(-1);
static void sample_battery(void *arg){
 (void)arg;int samples[3]={0},count=0,cursor=0,failures=0;
 for(;;){
  int raw;esp_err_t err=adc_oneshot_read(battery_adc,FMO_BATTERY_CHANNEL,&raw);
  if(err==ESP_OK&&raw>=0&&raw<=4095){
   failures=0;samples[cursor]=raw;cursor=(cursor+1)%3;if(count<3)count++;
   int sum=0;for(int i=0;i<count;i++)sum+=samples[i];
   int percent=xingzhi_battery_percent(sum/count);
   bool charging=gpio_get_level(FMO_CHARGING_GPIO)==1&&percent<100;
   atomic_store(&reading,percent+(charging?256:0));
  }else if(++failures>=5){atomic_store(&reading,-1);count=cursor=0;}
  vTaskDelay(pdMS_TO_TICKS(1000));
 }
}
void battery_init(void){
 gpio_config_t gpio={.pin_bit_mask=1ULL<<FMO_CHARGING_GPIO,.mode=GPIO_MODE_INPUT};
 esp_err_t err=gpio_config(&gpio);
 adc_oneshot_unit_init_cfg_t unit={.unit_id=ADC_UNIT_2};
 if(err==ESP_OK)err=adc_oneshot_new_unit(&unit,&battery_adc);
 adc_oneshot_chan_cfg_t channel={.atten=ADC_ATTEN_DB_12,.bitwidth=ADC_BITWIDTH_12};
 if(err==ESP_OK)err=adc_oneshot_config_channel(battery_adc,FMO_BATTERY_CHANNEL,&channel);
 if(err==ESP_OK&&xTaskCreate(sample_battery,"battery",2048,NULL,1,NULL)==pdPASS)return;
 if(battery_adc){adc_oneshot_del_unit(battery_adc);battery_adc=NULL;}
 ESP_LOGW("battery","Battery monitor unavailable; level remains unknown");
}
void battery_view(int *percent,bool *charging){int r=atomic_load(&reading);*percent=r<0?-1:r&255;*charging=r>=0&&(r&256);}
void side_leds_init(void) {}
void side_leds_update(bool tx,bool rx,int64_t ms){(void)tx;(void)rx;(void)ms;}
void side_leds_view(side_leds_view_t *out){memset(out,0,sizeof(*out));}
