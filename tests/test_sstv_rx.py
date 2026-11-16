"""Exercise production opt-in/exit/session queue controls; RTOS scheduling is mocked."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
stub=r'''
#pragma once
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
static jmp_buf worker_done;
static int run_worker,one_timeout,semaphore_depth,mock_lock_busy;
static int64_t mock_now=123456;
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) (++*(p))
#define portEXIT_CRITICAL(p) (--*(p))
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_SPIRAM 2
#define MALLOC_CAP_8BIT 4
typedef struct {unsigned count,size,cap,is_static;unsigned char *data;} Q;
typedef int StaticSemaphore_t;
static inline void*xSemaphoreCreateMutexStatic(StaticSemaphore_t*s){return s;}
typedef unsigned StackType_t;typedef int StaticTask_t;
typedef Q StaticQueue_t;
static inline void*xTaskCreateStaticPinnedToCore(void(*fn)(void*),const char*n,unsigned size,void*p,int priority,StackType_t*stack,StaticTask_t*control,int core){if(core!=1)abort();(void)fn;(void)n;(void)size;(void)p;(void)priority;(void)stack;return control;}
typedef Q* QueueHandle_t;typedef void* SemaphoreHandle_t;
static inline Q*xQueueCreate(unsigned cap,unsigned size){Q*q=calloc(1,sizeof(*q));q->cap=cap;q->size=size;q->data=calloc(cap,size);return q;}
static inline Q*xQueueCreateStatic(unsigned cap,unsigned size,uint8_t*data,Q*q){memset(q,0,sizeof(*q));q->cap=cap;q->size=size;q->data=data;q->is_static=1;return q;}
static inline void*heap_caps_calloc(size_t n,size_t size,int caps){(void)caps;return calloc(n,size);}
static inline int xQueueSend(Q*q,const void*p,unsigned wait){(void)wait;if(q->count==q->cap)return 0;memcpy(q->data+q->count++*q->size,p,q->size);return 1;}
static inline int xQueueReceive(Q*q,void*p,unsigned wait){(void)wait;if(!run_worker)return 0;if(!q->count){if(one_timeout){one_timeout=0;return 0;}longjmp(worker_done,1);}memcpy(p,q->data,q->size);q->count--;memmove(q->data,q->data+q->size,q->count*q->size);return 1;}
static inline void xQueueReset(Q*q){q->count=0;}
static inline void vQueueDelete(Q*q){if(!q->is_static){free(q->data);free(q);}}
static inline void*xSemaphoreCreateMutex(void){return malloc(1);}
static inline int xSemaphoreTake(void*s,unsigned wait){(void)wait;if(!s||mock_lock_busy)return 0;semaphore_depth++;return 1;}
static inline void xSemaphoreGive(void*s){(void)s;semaphore_depth--;}
static inline void vSemaphoreDelete(void*s){free(s);}
static inline int xTaskCreate(void(*fn)(void*),const char*n,unsigned size,void*p,int priority,void*handle){(void)fn;(void)n;(void)size;(void)p;(void)priority;(void)handle;return 1;}
static inline size_t heap_caps_get_free_size(int cap){(void)cap;
#ifdef CONFIG_FMO_BOARD_XINGZHI
return 20000;
#else
return 1000000;
#endif
}
static inline size_t heap_caps_get_largest_free_block(int caps){(void)caps;return 12000;}
static inline int64_t esp_timer_get_time(void){return mock_now;}
'''
ui=(root/'ui-preview-firmware/main/main.c').read_text()
transition=ui[ui.index('static void go(int next)'):ui.index(' voice_ptt(false);voice_mic_test(false);page=next;')].replace('go(int next)','release_workspace_for_page(int next)')+'}\n'
test=r'''
#include "freertos/FreeRTOS.h"
#ifdef CONFIG_IDF_TARGET_ESP32
#define calloc(...) NULL
#endif
#include "sstv_rx.c"
#ifdef CONFIG_IDF_TARGET_ESP32
#undef calloc
#endif
bool voice_busy(void){return false;}
#include <assert.h>
#include <stdio.h>
static unsigned writes;
#ifdef FMO_SSTV_STREAMING
bool sstv_stream_preview_rows(unsigned first,unsigned count,uint16_t*out){for(unsigned i=0;i<count*240;i++)out[i]=(first*240+i)&65535;return true;}
bool sstv_stream_prepare(void){return true;}bool sstv_stream_begin(const char*r){(void)r;return true;}
bool sstv_stream_rows(unsigned row,const uint8_t*p){(void)row;(void)p;return true;}
bool sstv_stream_finish_partial(uint32_t at){(void)at;return true;}
bool sstv_stream_finish(uint32_t at){(void)at;return true;}
void sstv_stream_discard(void){}
bool sstv_stream_save(uint32_t*id){writes++;*id=7;return true;}
#endif
bool sstv_store_save_partial(const uint8_t*p,uint32_t t,uint32_t*id,const char*r,unsigned lines){(void)p;(void)t;(void)r;assert(lines==20);writes++;*id=8;return true;}
bool sstv_store_save_relay(const uint8_t*p,uint32_t t,uint32_t*id,const char*relay){(void)relay;assert(t==1700000123);assert(p[0]==123);writes++;*id=7;return true;}
/* WORKSPACE_TRANSITION */
int main(void){int16_t pcm[640]={0};uint32_t id=0;
#ifdef FMO_SSTV_STREAMING
assert(sstv_workspace_claim(SSTV_WORKSPACE_GALLERY));assert(!sstv_rx_init()&&init_error==4);sstv_workspace_release(SSTV_WORKSPACE_GALLERY);
#endif
sstv_rx_feed(pcm,640,1,2,3);assert(!queue);assert(sstv_rx_init());assert(!atomic_load(&enabled));sstv_rx_feed(pcm,640,1,2,3);assert(!queue->count);assert(!sstv_rx_save(&id));
assert(sstv_rx_enable(true));
#ifdef FMO_SSTV_STREAMING
uint8_t preview_line[SSTV_PREVIEW_WIDTH+2];memset(preview_line,0xa5,sizeof(preview_line));memset(preview+3*SSTV_PREVIEW_WIDTH,0x42,SSTV_PREVIEW_WIDTH);assert(sstv_rx_preview_row(3,preview_line+1));assert(!semaphore_depth&&preview_line[0]==0xa5&&preview_line[sizeof(preview_line)-1]==0xa5);for(unsigned i=1;i<=SSTV_PREVIEW_WIDTH;i++)assert(preview_line[i]==0x42);mock_lock_busy=1;assert(sstv_rx_preview_row(3,preview_line+1));assert(!preview_mux);mock_lock_busy=0;assert(!sstv_rx_preview_row(SSTV_PREVIEW_HEIGHT,preview_line+1));assert(!semaphore_depth);
memset(preview,0x5a,SSTV_PREVIEW_BYTES);assert(sstv_rx_enable(true));for(unsigned i=0;i<SSTV_PREVIEW_BYTES;i++)assert(!preview[i]);
// Ordinary broadband audio must update DSP state without painting any pixels.
uint32_t noise=7;for(unsigned frame=0;frame<50;frame++){
 for(unsigned i=0;i<640;i++){noise=noise*1664525u+1013904223u;pcm[i]=(int16_t)(noise>>16);}
 sstv_rx_feed(pcm,640,1,2,3);run_worker=1;if(!setjmp(worker_done))worker(NULL);
}
assert(decoder->status==SSTV_WAITING&&!decoder->lines);for(unsigned i=0;i<SSTV_PREVIEW_BYTES;i++)assert(!preview[i]);
memset(pcm,0,sizeof(pcm));assert(sstv_rx_enable(true));
uint8_t native[1280];for(unsigned i=0;i<640;i++){native[i*2]=i;native[i*2+1]=i>>8;}
unsigned first,count;uint16_t hi[240];preview_publish(0,native);assert(sstv_rx_preview_dirty(&first,&count)&&first==0&&count==2);
assert(sstv_rx_preview_high_row(0,hi));for(unsigned x=0;x<240;x++)assert(hi[x]==x*320/240);
preview_publish(2,native);assert(high_skipped==1);assert(sstv_rx_preview_high_row(1,hi));for(unsigned x=0;x<240;x++)assert(hi[x]==320+x*320/240);
assert(!sstv_rx_preview_dirty(&first,&count)&&high_drawn==2);
assert(!sstv_rx_preview_tick()); // Never reread storage during live reception.
atomic_store(&enabled,false);decoder->status=SSTV_COMPLETE;replay_active=true;replay_next=0;
for(unsigned y=0;y<180;y+=2){sstv_rx_preview_tick();assert(sstv_rx_preview_dirty(&first,&count)&&first==y&&count==2);
 for(unsigned j=0;j<2;j++){assert(sstv_rx_preview_high_row(y+j,hi));for(unsigned x=0;x<240;x++)assert(hi[x]==((y+j)*240+x));}
 assert(!sstv_rx_preview_dirty(&first,&count));
}
assert(high_replayed==180&&!high_errors&&!sstv_rx_preview_tick());assert(sstv_rx_enable(true));
#endif
#ifndef FMO_SSTV_STREAMING
unsigned generation=~0u,lines=0;sstv_status_t snapshot_status;
memset(decoder->image,0x5a,SSTV_BYTES);decoder->generation++;assert(sstv_rx_snapshot(&snapshot_status,&lines,&generation));
assert(!memcmp(preview
#ifndef FMO_FULL_SSTV
+1024
#endif
,decoder->image,SSTV_BYTES));
#endif
for(int i=0;i<4;i++){sstv_rx_feed(pcm,640,1,2,3);}
assert(queue->count==4);unsigned old=serial;sstv_rx_feed(pcm,640,1,2,3);assert(serial==old+1);
decoder->status=SSTV_RECEIVING;assert(sstv_rx_enable(false));assert(!atomic_load(&enabled)&&!queue->count);
#ifdef FMO_SSTV_STREAMING
assert(!atomic_load(&ready)&&!decoder&&!preview);
#else
assert(decoder->status==SSTV_INTERRUPTED);
#endif
sstv_rx_feed(pcm,640,1,2,3);assert(!queue->count);
old=atomic_load(&session);assert(sstv_rx_enable(true));assert(atomic_load(&session)>old&&decoder->status==SSTV_WAITING);assert(!sstv_rx_save(&id));
for(int i=0;i<4;i++){sstv_rx_feed(pcm,640,1,2,3);}
run_worker=1;if(!setjmp(worker_done))worker(NULL);
assert(diagnostics.samples==2560&&diagnostics.stream_breaks==0);
// Delayed queued PCM survives jitter; explicit loss and timeout do not.
assert(sstv_rx_enable(true));sstv_rx_feed(pcm,640,1,2,3);mock_now+=260000;
if(!setjmp(worker_done))worker(NULL);
assert(diagnostics.samples==640&&diagnostics.drops==0&&diagnostics.stream_breaks==0);
#ifdef FMO_FULL_SSTV
assert(sstv_rx_enable(true));sstv_rx_feed(pcm,640,1,2,3);mock_now+=700000;
if(!setjmp(worker_done))worker(NULL);
assert(diagnostics.samples==640&&diagnostics.drops==0);
#endif
assert(sstv_rx_enable(true));sstv_rx_feed(pcm,640,1,2,3);one_timeout=1;
if(!setjmp(worker_done))worker(NULL);
assert(diagnostics.timeouts==1);
assert(sstv_rx_enable(true));sstv_rx_feed(pcm,640,1,2,3);mock_now+=SSTV_QUEUE_WAIT_MS*1000+1;
if(!setjmp(worker_done))worker(NULL);
assert(diagnostics.drops==1&&diagnostics.samples==0);

assert(sstv_rx_enable(true));sstv_rx_feed(pcm,640,1,2,3);sstv_rx_break();sstv_rx_feed(pcm,640,1,2,3);sstv_rx_feed(pcm,640,2,2,3);
if(!setjmp(worker_done))worker(NULL);
assert(diagnostics.samples==1920&&diagnostics.stream_breaks==2);
decoder->status=SSTV_COMPLETE;completed_at=1700000123;decoder->image[0]=123;assert(sstv_rx_save(&id)&&id==7&&writes==1);assert(sstv_rx_enable(false));
#ifdef FMO_SSTV_STREAMING
assert(!atomic_load(&ready)&&!decoder&&!preview);assert(sstv_workspace_claim(SSTV_WORKSPACE_GALLERY));memset(&sstv_m5_workspace,0xa5,sizeof(sstv_m5_workspace));sstv_workspace_release(SSTV_WORKSPACE_GALLERY);assert(sstv_rx_enable(true));assert(decoder&&preview&&atomic_load(&enabled));assert(sstv_rx_enable(false));
#else
assert(decoder->status==SSTV_COMPLETE);
#endif

assert(sstv_rx_enable(true));assert(!sstv_rx_save_partial(&id));decoder->status=SSTV_RECEIVING;decoder->lines=20;assert(!sstv_rx_save_partial(&id));decoder->status=SSTV_INTERRUPTED;assert(sstv_rx_save_partial(&id));assert(writes==2);assert(sstv_rx_enable(false));
#ifdef FMO_SSTV_STREAMING
assert(sstv_rx_init());page=HOME;release_workspace_for_page(GALLERY);assert(!atomic_load(&ready));assert(sstv_workspace_claim(SSTV_WORKSPACE_GALLERY));page=GALLERY;release_workspace_for_page(SSTV);assert(sstv_workspace_claim(SSTV_WORKSPACE_RX));sstv_workspace_release(SSTV_WORKSPACE_RX);
#endif
vQueueDelete(queue);
#ifndef FMO_SSTV_STREAMING
vSemaphoreDelete(lock);
#endif
#ifndef FMO_SSTV_STREAMING
free(preview);free(decoder);
#endif

puts("PASS SSTV opt-in: disabled boot, bounded nonblocking queue, overflow invalidation, immediate exit, fresh session and complete-only save");}
'''
with tempfile.TemporaryDirectory() as td:
 d=Path(td);(d/'stubs.h').write_text(stub);(d/'freertos').mkdir()
 for name in ['freertos/FreeRTOS.h','freertos/task.h','freertos/queue.h','freertos/semphr.h','esp_heap_caps.h','esp_timer.h','sdkconfig.h']:(d/name).write_text('#include "stubs.h"\n')
 (d/'esp_err.h').write_text('typedef int esp_err_t;\n')
 (d/'test.c').write_text(test.replace('/* WORKSPACE_TRANSITION */','#ifdef FMO_SSTV_STREAMING\nenum{HOME,GALLERY,SSTV};static int page;\n'+transition+'#endif'))
 for flags in ([],['-DCONFIG_FMO_BOARD_XINGZHI'],['-DCONFIG_IDF_TARGET_ESP32']):
  subprocess.run(['cc',*flags,'-std=c11','-Wall','-Wextra','-Werror','-I'+td,'-I'+str(root/'ui-preview-firmware/main'),str(d/'test.c'),str(root/'ui-preview-firmware/main/sstv_decoder.c'),'-lm','-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
