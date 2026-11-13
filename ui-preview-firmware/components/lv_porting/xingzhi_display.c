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
#include <stdatomic.h>
#include <string.h>
#include "esp_memory_utils.h"
atomic_uint ui_flush_sent,ui_flush_done,ui_flush_errors,ui_flush_non_dma,ui_flush_last_error;
#define DISPLAY_PIXELS (FMO_WIDTH*40)
static lv_color_t *dma_pixels;
static esp_lcd_panel_handle_t panel;
static lv_disp_drv_t drv;
static bool update=true;
static bool done(esp_lcd_panel_io_handle_t io,esp_lcd_panel_io_event_data_t *ev,void *ctx){
 (void)io;(void)ev;atomic_fetch_add(&ui_flush_done,1);lv_disp_flush_ready(ctx);return false;
}
static void flush(lv_disp_drv_t *d,const lv_area_t *a,lv_color_t *pixels){
 if(!update){lv_disp_flush_ready(d);return;}
 /* LVGL may pass external-memory pixels (e.g. rotated regions). Never
  * depend on SPI allocating a temporary internal DMA buffer while audio is
  * active. The next flush cannot run until done() releases this transfer. */
 size_t count=(size_t)(a->x2-a->x1+1)*(a->y2-a->y1+1);
 assert(count<=DISPLAY_PIXELS);
 if(!esp_ptr_dma_capable(pixels))atomic_fetch_add(&ui_flush_non_dma,1);
 memcpy(dma_pixels,pixels,count*sizeof(*pixels));
 atomic_fetch_add(&ui_flush_sent,1);
 esp_err_t e=esp_lcd_panel_draw_bitmap(panel,a->x1,a->y1,a->x2+1,a->y2+1,dma_pixels);
 if(e!=ESP_OK){atomic_fetch_add(&ui_flush_errors,1);atomic_store(&ui_flush_last_error,(unsigned)e);lv_disp_flush_ready(d);}
}
void disp_driver_set_brightness(int percent){
 if(percent<0)percent=0;
 if(percent>100)percent=100;
 ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,(FMO_LCD_BACKLIGHT_INVERTED?100-percent:percent)*1023/100);
 ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
}
void lv_port_disp_set_rotation(uint16_t rotation){(void)rotation;}
void disp_enable_update(void){update=true;}
void disp_disable_update(void){update=false;}
void lv_port_disp_init(void){
 ledc_timer_config_t timer={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,.timer_num=LEDC_TIMER_0,.freq_hz=20000,.clk_cfg=LEDC_AUTO_CLK};
 ESP_ERROR_CHECK(ledc_timer_config(&timer));
 ledc_channel_config_t light={.gpio_num=FMO_BACKLIGHT,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=FMO_LCD_BACKLIGHT_INVERTED?1023:0};
 ESP_ERROR_CHECK(ledc_channel_config(&light));
 spi_bus_config_t bus={.mosi_io_num=FMO_LCD_MOSI,.miso_io_num=-1,.sclk_io_num=FMO_LCD_SCLK,.quadwp_io_num=-1,.quadhd_io_num=-1,.max_transfer_sz=FMO_WIDTH*40*2};
 ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO));
 esp_lcd_panel_io_handle_t io;
 esp_lcd_panel_io_spi_config_t cfg={.cs_gpio_num=FMO_LCD_CS,.dc_gpio_num=FMO_LCD_DC,.spi_mode=FMO_LCD_SPI_MODE,.pclk_hz=FMO_LCD_CLOCK_HZ,.trans_queue_depth=2,.lcd_cmd_bits=8,.lcd_param_bits=8,.on_color_trans_done=done,.user_ctx=&drv};
 ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST,&cfg,&io));
 esp_lcd_panel_dev_config_t dev={.reset_gpio_num=FMO_LCD_RESET,.rgb_ele_order=FMO_LCD_BGR?LCD_RGB_ELEMENT_ORDER_BGR:LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16};
 ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io,&dev,&panel));
 ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
 ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel,FMO_LCD_INVERT));
 ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel,FMO_LCD_SWAP_XY));ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel,FMO_LCD_MIRROR_X,FMO_LCD_MIRROR_Y));
 ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel,FMO_LCD_GAP_X,FMO_LCD_GAP_Y));
 ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
 static lv_disp_draw_buf_t buf;
 lv_color_t *a=heap_caps_malloc(DISPLAY_PIXELS*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 lv_color_t *b=heap_caps_malloc(DISPLAY_PIXELS*sizeof(lv_color_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 dma_pixels=heap_caps_malloc(DISPLAY_PIXELS*sizeof(lv_color_t),MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
 assert(a&&b&&dma_pixels);lv_disp_draw_buf_init(&buf,a,b,FMO_WIDTH*40);
 lv_disp_drv_init(&drv);drv.sw_rotate=1;drv.hor_res=FMO_WIDTH;drv.ver_res=FMO_HEIGHT;drv.flush_cb=flush;drv.draw_buf=&buf;lv_disp_drv_register(&drv);
 disp_driver_set_brightness(100);
}
