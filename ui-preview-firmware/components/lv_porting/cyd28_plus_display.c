#include "lv_port_disp.h"
#include "cyd28_plus_touch.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <string.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
static spi_device_handle_t touch;
static esp_lcd_panel_handle_t panel;
static lv_disp_drv_t display_driver;
static bool transfer_done(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *ev,void *ctx){
 (void)io;(void)ev;lv_disp_flush_ready(ctx);return false;
}
static void flush(lv_disp_drv_t*d,const lv_area_t*a,lv_color_t*p){
 ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel,a->x1,a->y1,a->x2+1,a->y2+1,p));
}
void lv_port_disp_set_rotation(uint16_t r){(void)r;}
static int requested_brightness=100,probe_light=-1;
static void apply_brightness(void){
 // Physical A/B: both backlight-off and steady-high remove speaker crackle.
 // Never output PWM on this Plus board, including idle dim and wake transitions.
 int level=probe_light>=0?probe_light:(requested_brightness>0);
 ESP_ERROR_CHECK(ledc_stop(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,level));
}
void disp_driver_set_brightness(int p){if(p<0)p=0;if(p>100)p=100;requested_brightness=p;apply_brightness();}
void cyd_display_probe_light(int level){probe_light=level;apply_brightness();}

void lv_port_disp_init(void){
 gpio_config_t pins={.pin_bit_mask=1ULL<<21,.mode=GPIO_MODE_OUTPUT};ESP_ERROR_CHECK(gpio_config(&pins));gpio_set_level(21,0);
 spi_bus_config_t bus={.mosi_io_num=13,.miso_io_num=12,.sclk_io_num=14,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=320*4*2};
 ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));
 esp_lcd_panel_io_handle_t io;
 esp_lcd_panel_io_spi_config_t config={.cs_gpio_num=15,.dc_gpio_num=2,.spi_mode=0,.pclk_hz=10000000,.trans_queue_depth=1,.lcd_cmd_bits=8,.lcd_param_bits=8,.on_color_trans_done=transfer_done,.user_ctx=&display_driver};
 ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST,&config,&io));
 esp_lcd_panel_dev_config_t dev_config={.reset_gpio_num=-1,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_BGR,.bits_per_pixel=16};
 ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io,&dev_config,&panel));
 ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
 ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,true));ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel,true,false));ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,false));ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
 static lv_disp_draw_buf_t buf;lv_color_t *pixels=heap_caps_malloc(320*4*sizeof(lv_color_t),MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);assert(pixels);
 lv_disp_draw_buf_init(&buf,pixels,NULL,320*4);lv_disp_drv_init(&display_driver);display_driver.hor_res=320;display_driver.ver_res=240;display_driver.draw_buf=&buf;display_driver.flush_cb=flush;lv_disp_drv_register(&display_driver);
 spi_device_interface_config_t dev;
 bus=(spi_bus_config_t){.mosi_io_num=32,.miso_io_num=39,.sclk_io_num=25,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=8};
 ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_DISABLED));dev=(spi_device_interface_config_t){.clock_speed_hz=1000000,.mode=0,.spics_io_num=33,.queue_size=1};ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST,&dev,&touch));pins=(gpio_config_t){.pin_bit_mask=1ULL<<36,.mode=GPIO_MODE_INPUT};ESP_ERROR_CHECK(gpio_config(&pins));
 ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,.timer_num=LEDC_TIMER_0,.freq_hz=5000,.clk_cfg=LEDC_AUTO_CLK};ESP_ERROR_CHECK(ledc_timer_config(&timer));
 ledc_channel_config_t channel={.gpio_num=21,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=0};ESP_ERROR_CHECK(ledc_channel_config(&channel));disp_driver_set_brightness(100);
}
static int adc(uint8_t c){uint8_t tx[]={c,0,0},rx[3]={0};spi_transaction_t t={.length=24,.tx_buffer=tx,.rx_buffer=rx};if(spi_device_polling_transmit(touch,&t)!=ESP_OK)return -1;return ((rx[1]<<8)|rx[2])>>3;}
static int median(int a,int b,int c){if(a>b){int t=a;a=b;b=t;}if(b>c)b=c;return a>b?a:b;}
static bool loaded,calibrated,held;static unsigned step;static int32_t calibration[6];static int last_x,last_y;static int64_t began;
static lv_obj_t *overlay,*marker,*message;
static void target(void){static const int x[]={30,290,30},y[]={30,30,180};lv_obj_set_pos(marker,x[step]-10,y[step]-10);lv_label_set_text_fmt(message,"Touch + then release (%u/3)",step+1);}
void cyd_touch_recalibrate(void){calibrated=false;loaded=true;step=0;held=false;}
bool cyd_touch_poll(int*x,int*y,bool*down){
 if(!loaded){nvs_handle_t nv;size_t n=sizeof(calibration);if(nvs_open("cyd_touch",NVS_READONLY,&nv)==ESP_OK){calibrated=nvs_get_blob(nv,"points",calibration,&n)==ESP_OK&&n==sizeof(calibration);nvs_close(nv);int a,b;calibrated=calibrated&&cyd_map(calibration,calibration[0],calibration[1],&a,&b);}loaded=true;}
 if(!calibrated&&!overlay){overlay=lv_obj_create(lv_layer_top());lv_obj_set_size(overlay,320,240);lv_obj_set_pos(overlay,0,0);lv_obj_set_style_bg_color(overlay,lv_color_hex(0x102030),0);lv_obj_set_style_pad_all(overlay,0,0);lv_obj_set_style_text_color(overlay,lv_color_hex(0xffffff),0);lv_obj_set_style_bg_opa(overlay,LV_OPA_COVER,0);lv_obj_clear_flag(overlay,LV_OBJ_FLAG_SCROLLABLE);marker=lv_label_create(overlay);lv_label_set_text(marker,"+");message=lv_label_create(overlay);lv_obj_set_pos(message,10,95);target();}
 bool pen=!gpio_get_level(36);int rx=0,ry=0;
 if(pen){int xx[3],yy[3];for(int i=0;i<3;i++){xx[i]=adc(0xd0);yy[i]=adc(0x90);if(xx[i]<0||yy[i]<0)return false;}rx=median(xx[0],xx[1],xx[2]);ry=median(yy[0],yy[1],yy[2]);if(rx<100||rx>4000||ry<100||ry>4000)return false;
 if(!held){held=true;began=esp_timer_get_time();last_x=rx;last_y=ry;}if(abs(rx-last_x)>80||abs(ry-last_y)>80)began=esp_timer_get_time();last_x=rx;last_y=ry;
 }
 if(!calibrated){if(!pen&&held){held=false;if(esp_timer_get_time()-began<100000)return false;calibration[step*2]=last_x;calibration[step*2+1]=last_y;if(++step==3){int a,b;if(!cyd_map(calibration,calibration[0],calibration[1],&a,&b)){step=0;target();return false;}nvs_handle_t nv;if(nvs_open("cyd_touch",NVS_READWRITE,&nv)==ESP_OK){esp_err_t e=nvs_set_blob(nv,"points",calibration,sizeof(calibration));if(e==ESP_OK)e=nvs_commit(nv);nvs_close(nv);if(e!=ESP_OK){step=0;target();return false;}}else{step=0;target();return false;}calibrated=true;lv_obj_del(overlay);overlay=NULL;}else target();}return false;}
 *down=pen;if(pen)return cyd_map(calibration,rx,ry,x,y);if(held){held=false;return cyd_map(calibration,last_x,last_y,x,y);}return false;
}
