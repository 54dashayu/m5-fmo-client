from pathlib import Path
import tempfile,subprocess
source=Path(__file__).resolve().parents[1]/'ui-preview-firmware/main'
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'freertos').mkdir()
 (p/'sdkconfig.h').write_text('#define CONFIG_FMO_BOARD_XINGZHI 1\n')
 (p/'freertos/FreeRTOS.h').write_text('#include <stdint.h>\ntypedef int portMUX_TYPE;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(x) ((void)(x))\n#define portEXIT_CRITICAL(x) ((void)(x))\n#define pdFALSE 0\n#define eInvalid 0\n')
 (p/'freertos/task.h').write_text('typedef struct { uint64_t ulRunTimeCounter; } TaskStatus_t;\nint xTaskGetIdleTaskHandleForCore(int);\nvoid vTaskGetInfo(int,TaskStatus_t*,int,int);\n')
 (p/'esp_timer.h').write_text('int64_t esp_timer_get_time(void);\n')
 (p/'esp_heap_caps.h').write_text('#define MALLOC_CAP_INTERNAL 1\n#define MALLOC_CAP_8BIT 2\n#define MALLOC_CAP_SPIRAM 4\nunsigned heap_caps_get_total_size(int);\nunsigned heap_caps_get_free_size(int);\nunsigned heap_caps_get_largest_free_block(int);\n')
 (p/'test.c').write_text('''#include <assert.h>
#include <math.h>
#include "resource_metrics.c"
static int64_t clock_us=1000000;static uint64_t counters[2]={5000000000ULL,5000000000ULL};
int64_t esp_timer_get_time(void){return clock_us;}
int xTaskGetIdleTaskHandleForCore(int c){return c;}
void vTaskGetInfo(int c,TaskStatus_t*t,int a,int b){t->ulRunTimeCounter=counters[c];}
unsigned heap_caps_get_total_size(int c){return c==3?300000:8000000;}
unsigned heap_caps_get_free_size(int c){return c==3?20000:7000000;}
unsigned heap_caps_get_largest_free_block(int c){return 8000;}
int main(void){resource_metrics_t m;resource_metrics_sample();resource_metrics_read(&m);assert(!m.valid);clock_us+=2000000;counters[0]+=1000000;counters[1]+=1500000;resource_metrics_sample();resource_metrics_read(&m);assert(m.valid&&m.window_ms==2000&&fabs(m.cpu[0]-50)<.01&&fabs(m.cpu[1]-25)<.01&&fabs(m.average-37.5)<.01);assert(m.internal_free==20000&&m.psram_total==8000000);clock_us+=100000;resource_metrics_sample();resource_metrics_read(&m);assert(m.window_ms==2000);clock_us+=1900000;counters[0]+=2100000;resource_metrics_sample();resource_metrics_read(&m);assert(m.cpu[0]==0&&m.cpu[1]==100);}
''')
 subprocess.run(['cc','-I'+str(p),'-I'+str(source),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
 print('PASS: production sampler, 64-bit counters, per-core averages, 2-second gate, clamp and memory totals')
