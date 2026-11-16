"""Run production fixed-ring producer/consumer, jitter, failure and close interlock."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
pre=r'''
#define _POSIX_C_SOURCE 200809L
#include <stdatomic.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include "sstv_m5_workspace.h"
sstv_m5_workspace_t sstv_m5_workspace;atomic_int sstv_m5_workspace_owner;
typedef struct {bool prefetched;atomic_uint produced,consumed;atomic_bool read_failed;}sstv_reader_t;
#define pdMS_TO_TICKS(x) (x)
static void sleep_us(unsigned us){struct timespec delay={us/1000000,(us%1000000)*1000};while(nanosleep(&delay,&delay)&&errno==EINTR){}}
static void vTaskDelay(unsigned ms){sleep_us(ms*1000);}
static int64_t esp_timer_get_time(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
static atomic_bool running,hold_read,inside_read,close_done;
static atomic_int fail_at=-1;
static bool reader_row_direct(void*ctx,unsigned row,uint8_t*p){(void)ctx;
 if(atomic_load(&hold_read)){atomic_store(&inside_read,true);while(atomic_load(&hold_read))sleep_us(100);}
 if((int)row==atomic_load(&fail_at))return false;
 sleep_us(row%30==0?4000:100);memset(p,(int)row,640);return true;
}
#include "sstv_prefetch.inc"
static void*worker(void*p){(void)p;while(atomic_load(&running)){sstv_store_prefetch_tick();sleep_us(100);}return NULL;}
static void*closer(void*p){prefetch_stop(p);atomic_store(&close_done,true);return NULL;}
int main(void){
 sstv_reader_t r={0};uint8_t pixels[640];pthread_t task;
 assert(prefetch_start(&r));assert(atomic_load(&r.produced)==11);assert(sizeof(sstv_m5_workspace.tx_rows)==7040);
 atomic_store(&running,true);pthread_create(&task,NULL,worker,NULL);
 for(unsigned row=0;row<240;row++){assert(prefetch_row(&r,row,pixels));for(unsigned i=0;i<640;i++)assert(pixels[i]==row);sleep_us(1500);}
 assert(!prefetch_row(&r,240,pixels));prefetch_stop(&r);assert(!atomic_load(&sstv_m5_workspace_owner));
 atomic_store(&running,false);pthread_join(task,NULL);
 r=(sstv_reader_t){0};assert(prefetch_start(&r));for(unsigned row=0;row<11;row++)assert(prefetch_row(&r,row,pixels));assert(!prefetch_row(&r,11,pixels));prefetch_stop(&r);
 r=(sstv_reader_t){0};assert(prefetch_start(&r));assert(prefetch_row(&r,0,pixels));atomic_store(&fail_at,11);sstv_store_prefetch_tick();assert(atomic_load(&r.read_failed));prefetch_stop(&r);atomic_store(&fail_at,-1);
 r=(sstv_reader_t){0};assert(prefetch_start(&r));assert(prefetch_row(&r,0,pixels));atomic_store(&hold_read,true);atomic_store(&running,true);pthread_create(&task,NULL,worker,NULL);while(!atomic_load(&inside_read))sleep_us(100);
 pthread_t close_task;pthread_create(&close_task,NULL,closer,&r);sleep_us(5000);assert(!atomic_load(&close_done));assert(atomic_load(&sstv_m5_workspace_owner)==SSTV_WORKSPACE_TX);atomic_store(&hold_read,false);pthread_join(close_task,NULL);assert(!atomic_load(&sstv_m5_workspace_owner));atomic_store(&running,false);pthread_join(task,NULL);
 puts("PASS production SSTV prefetch: fixed 7040 bytes, ordered 240 rows, read jitter, starvation, I/O failure, close waits for producer before workspace reuse");
}
'''
with tempfile.TemporaryDirectory() as td:
 d=Path(td);(d/'esp_timer.h').write_text('');(d/'test.c').write_text(pre)
 subprocess.run(['cc','-std=c11','-DCONFIG_IDF_TARGET_ESP32','-Wall','-Wextra','-Werror','-I'+td,'-I'+str(root/'ui-preview-firmware/main'),str(d/'test.c'),'-pthread','-o',td+'/test'],check=True)
 subprocess.run([td+'/test'],check=True,timeout=10)
