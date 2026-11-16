#include "sstv_sd.h"
#include "sdkconfig.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#include "driver/sdspi_host.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "lvgl_spi_conf.h"
#include "disp_spi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/stat.h>
#include <stdatomic.h>
static sdmmc_card_t *card;
static bool attempted;
static const char *mounted_path;
static esp_err_t last_error;
static atomic_bool pause_requested,pause_ack;
static atomic_uint manage_stage;
unsigned sstv_sd_manage_stage(void){return atomic_load(&manage_stage);}
void sstv_sd_manage_reset(void){atomic_store(&manage_stage,0);}
bool sstv_sd_pause_display(void){bool pause=atomic_load(&pause_requested);atomic_store(&pause_ack,pause);return pause;}
static bool pause_ui(void){atomic_store(&pause_requested,true);for(unsigned i=0;i<200;i++){if(atomic_load(&pause_ack)){disp_wait_for_pending_transactions();return true;}vTaskDelay(pdMS_TO_TICKS(10));}atomic_store(&pause_requested,false);return false;}
static void resume_ui(void){atomic_store(&pause_requested,false);}
static const char *mount_card(bool allow_format){
 sdmmc_host_t host=SDSPI_HOST_DEFAULT();host.slot=TFT_SPI_HOST;host.max_freq_khz=400;
 sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT();slot.host_id=TFT_SPI_HOST;slot.gpio_cs=4;
 esp_vfs_fat_sdmmc_mount_config_t cfg={.format_if_mount_failed=allow_format,.max_files=3,.allocation_unit_size=16384};
 last_error=esp_vfs_fat_sdspi_mount("/sdcard",&host,&slot,&cfg,&card);
 if(last_error!=ESP_OK){card=NULL;ESP_LOGW("sstv_sd","mount failed: %s",esp_err_to_name(last_error));return NULL;}
 struct stat st;
 if(stat("/sdcard/fmo-sstv",&st)!=0){if(mkdir("/sdcard/fmo-sstv",0775)!=0)last_error=ESP_FAIL;}else if(!S_ISDIR(st.st_mode))last_error=ESP_FAIL;
 if(last_error!=ESP_OK)return NULL;
 mounted_path="/sdcard/fmo-sstv";ESP_LOGI("sstv_sd","TF mounted at 400 kHz");return mounted_path;
}
const char *sstv_sd_mount(void){if(attempted)return mounted_path;attempted=true;return mount_card(false);}
void lvgl_spi_bus_ready(void){(void)sstv_sd_mount();}
bool sstv_sd_supported(void){return true;}
bool sstv_sd_ready(void){return mounted_path!=NULL;}
const char *sstv_sd_error(void){return esp_err_to_name(last_error);}
bool sstv_sd_space(uint64_t *total,uint64_t *used){uint64_t free_bytes;if(!card||esp_vfs_fat_info("/sdcard",total,&free_bytes)!=ESP_OK)return false;*used=*total-free_bytes;return true;}
bool sstv_sd_manage(bool format){
 atomic_store(&manage_stage,0);
 if(!pause_ui()){last_error=ESP_ERR_TIMEOUT;return false;}
 atomic_store(&manage_stage,1);
 bool ok=true;
 // Keep a mounted card registered: the format API unmounts/remounts its filesystem.
 if(!format||!card||!mounted_path){
  if(card){last_error=esp_vfs_fat_sdcard_unmount("/sdcard",card);if(last_error!=ESP_OK){resume_ui();return false;}card=NULL;}
  mounted_path=NULL;attempted=true;ok=mount_card(format)!=NULL;
 }
 atomic_store(&manage_stage,2);
 if(ok&&format){last_error=esp_vfs_fat_sdcard_format("/sdcard",card);ok=last_error==ESP_OK;if(ok){struct stat st;ok=stat("/sdcard/fmo-sstv",&st)==0?S_ISDIR(st.st_mode):mkdir("/sdcard/fmo-sstv",0775)==0;if(!ok)last_error=ESP_FAIL;}}
 if(ok){atomic_store(&manage_stage,3);uint64_t total,used;ok=sstv_sd_space(&total,&used)&&total>0&&used<=total;if(!ok)last_error=ESP_FAIL;}
 if(!ok)mounted_path=NULL;else{mounted_path="/sdcard/fmo-sstv";atomic_store(&manage_stage,4);}
 resume_ui();return ok;
}
#else
#include <stddef.h>
const char *sstv_sd_mount(void){return NULL;}
bool sstv_sd_space(uint64_t *total,uint64_t *used){(void)total;(void)used;return false;}
bool sstv_sd_supported(void){return false;}
bool sstv_sd_ready(void){return false;}
const char *sstv_sd_error(void){return "NOT_SUPPORTED";}
bool sstv_sd_manage(bool format){(void)format;return false;}
bool sstv_sd_pause_display(void){return false;}
unsigned sstv_sd_manage_stage(void){return 0;}
void sstv_sd_manage_reset(void){}
#endif
