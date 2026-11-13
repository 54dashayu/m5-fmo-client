#include "resource_metrics.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static resource_metrics_t snapshot;
void resource_metrics_sample(void) {
 static int64_t previous_time;
 static uint64_t previous_idle[2];
 int64_t now=esp_timer_get_time();
 if(previous_time && now-previous_time<2000000) return;
 resource_metrics_t next={0};
 for(int core=0;core<2;core++) {
  TaskStatus_t task={0};
  vTaskGetInfo(xTaskGetIdleTaskHandleForCore(core),&task,pdFALSE,eInvalid);
  uint64_t idle=task.ulRunTimeCounter;
  if(previous_time && idle>=previous_idle[core]) {
   double fraction=(double)(idle-previous_idle[core])/(now-previous_time);
   if(fraction>1) fraction=1;
   next.cpu[core]=(float)(100*(1-fraction));
  }
  previous_idle[core]=idle;
 }
 next.valid=previous_time!=0; next.window_ms=previous_time?(now-previous_time)/1000:0;
 next.average=(next.cpu[0]+next.cpu[1])/2; previous_time=now;
 next.internal_total=heap_caps_get_total_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 next.internal_free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 next.internal_largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
 next.psram_total=heap_caps_get_total_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 next.psram_free=heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 portENTER_CRITICAL(&lock); snapshot=next; portEXIT_CRITICAL(&lock);
}
void resource_metrics_read(resource_metrics_t *out) {
 portENTER_CRITICAL(&lock); *out=snapshot; portEXIT_CRITICAL(&lock);
}
