#include "battery.h"
#include <stdatomic.h>
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
static atomic_int reading=ATOMIC_VAR_INIT(-1);
static void sample(void *arg){(void)arg;for(;;){uint8_t level=0,status=0,reg=0x78;esp_err_t e=i2c_master_write_read_device(I2C_NUM_0,0x75,&reg,1,&level,1,pdMS_TO_TICKS(25));reg=0x70;if(e==ESP_OK)e=i2c_master_write_read_device(I2C_NUM_0,0x75,&reg,1,&status,1,pdMS_TO_TICKS(25));int percent=battery_decode(level);atomic_store(&reading,e==ESP_OK&&percent>=0?percent+((status&0x08)?256:0):-1);vTaskDelay(pdMS_TO_TICKS(5000));}}
void battery_init(void){i2c_config_t cfg={.mode=I2C_MODE_MASTER,.sda_io_num=21,.scl_io_num=22,.sda_pullup_en=GPIO_PULLUP_ENABLE,.scl_pullup_en=GPIO_PULLUP_ENABLE,.master.clk_speed=100000};esp_err_t e=i2c_param_config(I2C_NUM_0,&cfg);if(e==ESP_OK)e=i2c_driver_install(I2C_NUM_0,I2C_MODE_MASTER,0,0,0);if(e!=ESP_OK){ESP_LOGW("battery","I2C unavailable: %s",esp_err_to_name(e));return;}if(xTaskCreate(sample,"battery",2048,NULL,1,NULL)!=pdPASS){i2c_driver_delete(I2C_NUM_0);ESP_LOGW("battery","Monitor allocation failed");}}
void battery_view(int *percent,bool *charging){int r=atomic_load(&reading);*percent=r<0?-1:r&255;*charging=r>=0&&(r&256);}
