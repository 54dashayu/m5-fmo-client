#include "sstv_rx.h"
#include "voice.h"
#include "sdkconfig.h"
#include "sstv_store.h"
#include "sstv_m5_workspace.h"
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>
typedef struct { int16_t pcm[640];unsigned n,serial,session;uint32_t uid,base,epoch;int64_t time; } chunk_t;
static sstv_decoder_t *decoder;
static uint8_t *preview;
static QueueHandle_t queue;
#ifdef FMO_FULL_SSTV
static StaticQueue_t queue_control;
static uint8_t *queue_storage;
#ifdef FMO_SSTV_STREAMING
sstv_m5_workspace_t sstv_m5_workspace;
atomic_int sstv_m5_workspace_owner;
static StaticSemaphore_t lock_control;
// Protect only tiny preview copies, never DSP, storage or LCD rendering.
static portMUX_TYPE preview_mux=portMUX_INITIALIZER_UNLOCKED;
static unsigned high_first,high_count,high_read,high_drawn,high_skipped,high_replayed,high_errors;
static unsigned replay_next;
static bool replay_active;
static void preview_clear(void){portENTER_CRITICAL(&preview_mux);memset(preview,0,SSTV_PREVIEW_BYTES);high_first=high_count=high_read=high_drawn=high_skipped=high_replayed=high_errors=0;replay_active=false;replay_next=0;portEXIT_CRITICAL(&preview_mux);}
static bool worker_started;
static uint8_t static_queue[4*sizeof(chunk_t)];
static StackType_t worker_stack[4096/sizeof(StackType_t)];
static StaticTask_t worker_control;
#endif
#endif
static unsigned init_error;
static SemaphoreHandle_t lock;
/* Only the voice task writes serial. */
static unsigned serial;
static atomic_uint dropped,max_feed_gap,snapshot_count,snapshot_misses;
/* Written only by the audio producer; session changes reset its clock. */
static int64_t last_feed_time;
static unsigned feed_session;
#ifdef FMO_FULL_SSTV
#define SSTV_QUEUE_WAIT_MS 1500
#else
#define SSTV_QUEUE_WAIT_MS 350
#endif
bool sstv_rx_active(void);
static sstv_diagnostics_t diagnostics;
static uint32_t completed_at;
static char reception_relay[128];
void sstv_rx_relay(const char *relay){snprintf(reception_relay,sizeof(reception_relay),"%s",relay?relay:"");}
static atomic_bool ready,enabled;
bool sstv_rx_active(void){return atomic_load(&enabled);}
static atomic_flag initializing=ATOMIC_FLAG_INIT;
static atomic_uint session;
#ifdef FMO_SSTV_STREAMING
static bool stream_begin(void *ctx){(void)ctx;preview_clear();return sstv_stream_begin(reception_relay);}
static void preview_publish(unsigned first,const uint8_t *pixels){
 unsigned y=(first*180+239)/240,end=((first+2)*180+239)/240;
 unsigned count=end-y;
 portENTER_CRITICAL(&preview_mux);bool busy=high_count;if(busy)high_skipped+=count;portEXIT_CRITICAL(&preview_mux);
 if(busy)return; /* A slow display may skip preview rows, never stall audio. */
 for(unsigned i=0;i<count;i++){
  unsigned source=(y+i)*240/180-first;
  for(unsigned x=0;x<240;x++){unsigned at=(source*320+x*320/240)*2;sstv_m5_workspace.rx.high_rows[i][x]=pixels[at]|pixels[at+1]<<8;}
 }
 portENTER_CRITICAL(&preview_mux);high_first=y;high_count=count;high_read=0;portEXIT_CRITICAL(&preview_mux);
}
static bool stream_rows(void *ctx,unsigned first,const uint8_t *pixels){
 (void)ctx;int64_t start=esp_timer_get_time();bool ok=sstv_stream_rows(first,pixels);
 unsigned elapsed=esp_timer_get_time()-start;if(elapsed>diagnostics.max_store_us)diagnostics.max_store_us=elapsed;if(elapsed>80000)diagnostics.store_stalls++;
 if(!ok)return false;
 preview_publish(first,pixels);
 for(unsigned y=(first*SSTV_PREVIEW_HEIGHT+239)/240;y<SSTV_PREVIEW_HEIGHT&&y*240/SSTV_PREVIEW_HEIGHT<first+2;y++){
  unsigned source_y=y*240/SSTV_PREVIEW_HEIGHT-first;
  uint8_t row[SSTV_PREVIEW_WIDTH];
  for(unsigned x=0;x<SSTV_PREVIEW_WIDTH;x++){unsigned at=(source_y*320+x*320/SSTV_PREVIEW_WIDTH)*2,v=pixels[at]|pixels[at+1]<<8;
   unsigned r=(v>>11)*255/31,g=((v>>5)&63)*255/63,b=(v&31)*255/31;
   row[x]=(r&224)|((g>>3)&28)|(b>>6);
  }
  portENTER_CRITICAL(&preview_mux);memcpy(preview+y*SSTV_PREVIEW_WIDTH,row,sizeof(row));portEXIT_CRITICAL(&preview_mux);
 }
 return true;
}
#endif
static void decoder_reset(void){
 sstv_decoder_init(decoder);
#ifdef FMO_SSTV_STREAMING
 preview_clear();decoder->begin=stream_begin;decoder->rows=stream_rows;
#endif
}
#ifdef FMO_SSTV_STREAMING
bool sstv_rx_preview_row(unsigned row,uint8_t *out){
 if(!out||row>=SSTV_PREVIEW_HEIGHT)return false;
 portENTER_CRITICAL(&preview_mux);
 bool ok=atomic_load(&ready)&&preview;
 if(ok)memcpy(out,preview+row*SSTV_PREVIEW_WIDTH,SSTV_PREVIEW_WIDTH);
 portEXIT_CRITICAL(&preview_mux);return ok;
}
/* UI reads at most 480 bytes while holding the tiny preview lock. */
bool sstv_rx_preview_high_row(unsigned row,uint16_t*out){
 if(!out)return false;
 portENTER_CRITICAL(&preview_mux);
 bool ok=atomic_load(&ready)&&high_count&&row>=high_first&&row<high_first+high_count;
 if(ok){unsigned at=row-high_first;memcpy(out,sstv_m5_workspace.rx.high_rows[at],480);high_read|=1u<<at;}
 portEXIT_CRITICAL(&preview_mux);return ok;
}
bool sstv_rx_preview_dirty(unsigned *first,unsigned *count){
 portENTER_CRITICAL(&preview_mux);
 if(high_count&&high_read==((1u<<high_count)-1)){high_drawn+=high_count;high_count=0;high_read=0;}
 bool ok=atomic_load(&ready)&&high_count;
 if(ok){*first=high_first;*count=high_count;}
 portEXIT_CRITICAL(&preview_mux);return ok;
}
/* Existing management task; no LVGL calls and no live-reception file reads. */
bool sstv_rx_preview_tick(void){
 if(!atomic_load(&ready)||atomic_load(&enabled)||voice_busy())return false;
 if(xSemaphoreTake(lock,0)!=pdTRUE)return false;
 if(!atomic_load(&ready)||atomic_load(&enabled)||decoder->status!=SSTV_COMPLETE){xSemaphoreGive(lock);return false;}
 portENTER_CRITICAL(&preview_mux);bool active=replay_active,busy=high_count;portEXIT_CRITICAL(&preview_mux);
 if(active&&!busy){unsigned count=180-replay_next;if(count>2)count=2;
  bool ok=sstv_stream_preview_rows(replay_next,count,&sstv_m5_workspace.rx.high_rows[0][0]);
  portENTER_CRITICAL(&preview_mux);
  if(ok){high_first=replay_next;high_count=count;high_read=0;replay_next+=count;high_replayed+=count;}
  else high_errors++;
  if(!ok||replay_next==180)replay_active=false;
  active=replay_active;portEXIT_CRITICAL(&preview_mux);
 }
 xSemaphoreGive(lock);return active;
}

#endif
bool sstv_rx_display_lock(void){return atomic_load(&ready)&&xSemaphoreTake(lock,0)==pdTRUE;}
void sstv_rx_display_unlock(void){xSemaphoreGive(lock);}
static void worker(void *unused){
    (void)unused;chunk_t c;unsigned last_serial=0;uint32_t uid=0,base=0,epoch=0;bool seen=false;
    for(;;){bool got=xQueueReceive(queue,&c,pdMS_TO_TICKS(SSTV_QUEUE_WAIT_MS))==pdTRUE;
        xSemaphoreTake(lock,portMAX_DELAY);
        if(!atomic_load(&enabled)){seen=false;}
        else if(got&&c.session!=atomic_load(&session)){seen=false;}
        else if(!got){if(seen)diagnostics.timeouts++;sstv_decoder_break(decoder);seen=false;}
        else if(esp_timer_get_time()-c.time>SSTV_QUEUE_WAIT_MS*1000){diagnostics.drops++;sstv_decoder_break(decoder);seen=false;}
        else{
            bool discontinuity=seen&&(c.serial!=last_serial||c.uid!=uid||c.base!=base||c.epoch!=epoch);
/* Transport indexes restart per packet; use stream identity and explicit loss. */
            if(!seen||discontinuity){if(discontinuity)diagnostics.stream_breaks++;sstv_decoder_break(decoder);}
            int64_t started=esp_timer_get_time();
            sstv_decoder_feed(decoder,c.pcm,c.n);
            unsigned elapsed=esp_timer_get_time()-started;if(elapsed>diagnostics.max_process_us)diagnostics.max_process_us=elapsed;
            diagnostics.samples+=c.n;for(unsigned i=0;i<c.n;i++)if(c.pcm[i]>=32760||c.pcm[i]<=-32760)diagnostics.clipped++;
            if(decoder->status==SSTV_COMPLETE){completed_at=(uint32_t)time(NULL);
#ifdef FMO_SSTV_STREAMING
 if(!sstv_stream_finish(completed_at))decoder->status=SSTV_INTERRUPTED;
 else {portENTER_CRITICAL(&preview_mux);replay_active=high_skipped!=0;replay_next=0;portEXIT_CRITICAL(&preview_mux);}
#endif
 atomic_store(&enabled,false);}
            seen=true;last_serial=c.serial;uid=c.uid;base=c.base;epoch=c.epoch;
        }
        xSemaphoreGive(lock);
    }
}
bool sstv_rx_init(void){
    if(atomic_load(&ready))return true;
    if(atomic_flag_test_and_set(&initializing))return false;
    init_error=1;
#ifdef FMO_SSTV_STREAMING
    if(!sstv_workspace_claim(SSTV_WORKSPACE_RX)){init_error=4;atomic_flag_clear(&initializing);return false;}
    decoder=&sstv_m5_workspace.rx.decoder;preview=sstv_m5_workspace.rx.preview;memset(preview,0,SSTV_PREVIEW_BYTES);queue_storage=static_queue;
    if(!lock)lock=xSemaphoreCreateMutexStatic(&lock_control);
    if(!queue)queue=xQueueCreateStatic(4,sizeof(chunk_t),queue_storage,&queue_control);
#elif defined(FMO_FULL_SSTV)
    decoder=heap_caps_calloc(1,sizeof(*decoder),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    preview=heap_caps_calloc(1,SSTV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    queue_storage=heap_caps_calloc(4,sizeof(chunk_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!decoder||!preview||!queue_storage)goto failed;
    init_error=2;lock=xSemaphoreCreateMutex();
    queue=xQueueCreateStatic(4,sizeof(chunk_t),queue_storage,&queue_control);
#else
    decoder=calloc(1,sizeof(*decoder));preview=calloc(1,1024+SSTV_PIXELS);
    lock=xSemaphoreCreateMutex();queue=xQueueCreate(4,sizeof(chunk_t));
#endif
    if(!decoder||!preview||!lock||!queue)goto failed;
    decoder_reset();
    /* LVGL indexed palette is BGRA8888, independent of RGB565 byte order. */
    #ifndef FMO_FULL_SSTV
    for(unsigned p=0;p<256;p++){preview[p*4]=(p&3)*255/3;preview[p*4+1]=((p>>2)&7)*255/7;preview[p*4+2]=(p>>5)*255/7;preview[p*4+3]=255;}
    #endif
#ifndef FMO_FULL_SSTV
    if(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<32768)goto failed;
#endif
    init_error=3;
#ifdef FMO_SSTV_STREAMING
    if(!worker_started){if(!xTaskCreateStaticPinnedToCore(worker,"sstv_rx",sizeof(worker_stack),NULL,2,worker_stack,&worker_control,1))goto failed;worker_started=true;}
#else
    if(xTaskCreate(worker,"sstv_rx",4096,NULL,2,NULL)!=pdPASS)goto failed;
#endif
    init_error=0;atomic_store(&ready,true);atomic_flag_clear(&initializing);return true;
failed:
#ifndef FMO_SSTV_STREAMING
    if(queue)vQueueDelete(queue);
    queue=NULL;
#if defined(FMO_FULL_SSTV) && !defined(FMO_SSTV_STREAMING)
    free(queue_storage);queue_storage=NULL;
#endif
    if(lock)vSemaphoreDelete(lock);
    lock=NULL;
#endif
#ifdef FMO_SSTV_STREAMING
    sstv_workspace_release(SSTV_WORKSPACE_RX);
#else
    free(decoder);free(preview);
#endif
    decoder=NULL;preview=NULL;atomic_flag_clear(&initializing);return false;
}
void sstv_rx_break(void){serial++;}
void sstv_rx_feed(const int16_t*pcm,size_t n,uint32_t uid,uint32_t base,uint32_t epoch){
    if(!atomic_load(&ready)||!atomic_load(&enabled)||!n||n>640)return;
    chunk_t c={.n=n,.serial=serial,.session=atomic_load(&session),.uid=uid,.base=base,.epoch=epoch,.time=esp_timer_get_time()};
    if(feed_session==c.session&&last_feed_time){unsigned gap=(unsigned)(c.time-last_feed_time);if(gap>atomic_load(&max_feed_gap))atomic_store(&max_feed_gap,gap);}
    feed_session=c.session;last_feed_time=c.time;
    memcpy(c.pcm,pcm,n*sizeof(*pcm));
    if(xQueueSend(queue,&c,0)!=pdTRUE){serial++;atomic_fetch_add(&dropped,1);}
}
uint8_t *sstv_rx_pixels(void){return atomic_load(&ready)?preview:NULL;}
bool sstv_rx_snapshot(sstv_status_t*status,unsigned*lines,unsigned*generation){
    if(!atomic_load(&ready))return false;
    if(xSemaphoreTake(lock,pdMS_TO_TICKS(20))!=pdTRUE){atomic_fetch_add(&snapshot_misses,1);return false;}
    atomic_fetch_add(&snapshot_count,1);
    if(!atomic_load(&ready)){xSemaphoreGive(lock);return false;}
    *status=decoder->status;*lines=decoder->lines;
    if(*generation!=decoder->generation){
#ifndef FMO_SSTV_STREAMING
memcpy(preview
#ifndef FMO_FULL_SSTV
        +1024
#endif
        ,decoder->image,SSTV_BYTES);
#endif
*generation=decoder->generation;}
    xSemaphoreGive(lock);return true;
}

bool sstv_rx_enable(bool on){
    if(on&&voice_busy())return false;
    if(on&&!sstv_rx_init())return false;
#ifdef FMO_SSTV_STREAMING
    if(on&&!sstv_stream_prepare())return false;
#endif
    if(!atomic_load(&ready))return true;
    atomic_store(&enabled,false);atomic_fetch_add(&session,1);
    xSemaphoreTake(lock,portMAX_DELAY);xQueueReset(queue);
    if(on){completed_at=0;memset(&diagnostics,0,sizeof(diagnostics));atomic_store(&dropped,0);atomic_store(&max_feed_gap,0);atomic_store(&snapshot_count,0);atomic_store(&snapshot_misses,0);unsigned revision=decoder->generation+1;decoder_reset();decoder->generation=revision;}
    else {sstv_decoder_break(decoder);
#ifdef FMO_SSTV_STREAMING
 sstv_stream_discard();portENTER_CRITICAL(&preview_mux);atomic_store(&ready,false);decoder=NULL;preview=NULL;portEXIT_CRITICAL(&preview_mux);sstv_workspace_release(SSTV_WORKSPACE_RX);
#endif
    }
    atomic_store(&enabled,on);xSemaphoreGive(lock);return true;
}
bool sstv_rx_save(uint32_t *id){
    if(!atomic_load(&ready)||!id||xSemaphoreTake(lock,pdMS_TO_TICKS(100))!=pdTRUE)return false;
    bool ok=atomic_load(&ready)&&decoder->status==SSTV_COMPLETE&&
#ifdef FMO_SSTV_STREAMING
 sstv_stream_save(id);
#else
 sstv_store_save_relay(decoder->image,completed_at,id,reception_relay);
#endif
    xSemaphoreGive(lock);return ok;
}

bool sstv_rx_save_partial(uint32_t *id){
 if(!atomic_load(&ready)||!id||xSemaphoreTake(lock,pdMS_TO_TICKS(100))!=pdTRUE)return false;
 bool ok=false;
 if(atomic_load(&ready)&&decoder->status==SSTV_INTERRUPTED&&decoder->lines>=2&&decoder->lines<240){
  bool was_enabled=atomic_exchange(&enabled,false);uint32_t at=(uint32_t)time(NULL);
#ifdef FMO_SSTV_STREAMING
  ok=sstv_stream_finish_partial(at)&&sstv_stream_save(id);
#else
  unsigned rows=decoder->lines&~1u;
  memset(decoder->image+rows*SSTV_WIDTH*SSTV_BPP,0,(SSTV_HEIGHT-rows)*SSTV_WIDTH*SSTV_BPP);
  ok=sstv_store_save_partial(decoder->image,at,id,reception_relay,rows);
#endif
  if(!ok)atomic_store(&enabled,was_enabled);
 }
 xSemaphoreGive(lock);return ok;
}

bool sstv_rx_diagnostics(sstv_diagnostics_t*out){
    memset(out,0,sizeof(*out));out->ready=atomic_load(&ready);out->enabled=atomic_load(&enabled);
    out->init_error=init_error;out->internal_free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);out->internal_largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    if(!out->ready)return true;
    if(xSemaphoreTake(lock,pdMS_TO_TICKS(20))!=pdTRUE)return false;
    if(!atomic_load(&ready)){xSemaphoreGive(lock);return false;}
    *out=diagnostics;out->ready=true;out->enabled=atomic_load(&enabled);out->drops+=atomic_load(&dropped);out->max_feed_gap_us=atomic_load(&max_feed_gap);out->snapshots=atomic_load(&snapshot_count);out->snapshot_misses=atomic_load(&snapshot_misses);
#ifdef FMO_SSTV_STREAMING
    for(unsigned i=0;i<SSTV_PREVIEW_BYTES;i++)if(preview[i])out->preview_nonzero++;
    portENTER_CRITICAL(&preview_mux);out->preview_high_rows=high_drawn;out->preview_skipped=high_skipped;out->preview_replay_rows=high_replayed;out->preview_read_errors=high_errors;portEXIT_CRITICAL(&preview_mux);
#else
    for(unsigned i=0;i<SSTV_BYTES;i+=97)if(preview[i])out->preview_nonzero++;
#endif
    out->init_error=init_error;out->internal_free=heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);out->internal_largest=heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    out->lines=decoder->lines;out->status=decoder->status;out->frequency=(int)decoder->smooth;
    out->leader_max=decoder->leader_max;out->vis_starts=decoder->vis_starts;out->vis_code=decoder->vis_code;
    xSemaphoreGive(lock);return true;
}
