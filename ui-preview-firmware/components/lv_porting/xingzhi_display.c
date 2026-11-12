#include "board.h"
#include "fmo_display.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_heap_caps.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
static esp_lcd_panel_handle_t panel;
static lv_disp_drv_t drv;
static bool update=true;
static bool done(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *ev,void *ctx){
 (void)io;(void)ev;lv_disp_flush_ready(ctx);return false;
}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *pixels){
 if(!update){lv_disp_flush_ready(d);return;}
 esp_err_t e=esp_lcd_panel_draw_bitmap(panel,a->x1,a->y1,a->x2+1,a->y2+1,pixels);
 if(e!=ESP_OK)lv_disp_flush_ready(d);
}
void disp_driver_set_brightness(int percent){
 if(percent<0)percent=0;
 if(percent>100)percent=100;
 ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,percent*1023/100);
 ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
}
void lv_port_disp_set_rotation(uint16_t rotation){(void)rotation;}
void disp_enable_update(void){update=true;}
void disp_disable_update(void){update=false;}
void lv_port_disp_init(void){
 ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,.timer_num=LEDC_TIMER_0,.freq_hz=20000,.clk_cfg=LEDC_AUTO_CLK};
 ESP_ERROR_CHECK(ledc_timer_config(&timer));
 ledc_channel_config_t light={.gpio_num=FMO_BACKLIGHT,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=0};
 ESP_ERROR_CHECK(ledc_channel_config(&light));
 spi_bus_config_t bus={.mosi_io_num=FMO_LCD_MOSI,.miso_io_num=-1,.sclk_io_num=FMO_LCD_SCLK,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=240*40*2};
 ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO));
 esp_lcd_panel_io_handle_t io;
 esp_lcd_panel_io_spi_config_t cfg={.cs_gpio_num=FMO_LCD_CS,.dc_gpio_num=FMO_LCD_DC,.spi_mode=3,.pclk_hz=40000000,.trans_queue_depth=2,.lcd_cmd_bits=8,.lcd_param_bits=8,.on_color_trans_done=done,.user_ctx=&drv};
 ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST,&cfg,&io));
 esp_lcd_panel_dev_config_t dev={.reset_gpio_num=FMO_LCD_RESET,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16};
 ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io,&dev,&panel));
 ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
 ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,true));
 ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,false));ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel,false,false));
 ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
 static lv_disp_draw_buf_t buf;
 lv_color_t *a=heap_caps_malloc(240*40*sizeof(lv_color_t),MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
 lv_color_t *b=heap_caps_malloc(240*40*sizeof(lv_color_t),MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
 assert(a&&b);lv_disp_draw_buf_init(&buf,a,b,240*40);
 lv_disp_drv_init(&drv);drv.hor_res=240;drv.ver_res=240;drv.flush_cb=flush;drv.draw_buf=&buf;lv_disp_drv_register(&drv);
 disp_driver_set_brightness(100);
}
