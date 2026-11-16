/* One producer (management worker), one consumer (audio task), fixed shared RAM.
 * File I/O never runs in the paced audio loop. Release waits for an in-flight read. */
#include "sstv_m5_workspace.h"
#include "esp_timer.h"
#define PREFETCH_ROWS 11u
static _Atomic(sstv_reader_t*) prefetch_reader;
static atomic_flag prefetch_busy=ATOMIC_FLAG_INIT;
static bool prefetch_start(sstv_reader_t*r){
 if(atomic_load(&prefetch_reader))return false;
 bool claimed=false;
 for(unsigned i=0;i<100&&!claimed;i++){claimed=sstv_workspace_claim(SSTV_WORKSPACE_TX);if(!claimed)vTaskDelay(pdMS_TO_TICKS(10));}
 if(!claimed)return false;
 r->prefetched=true;
 for(unsigned i=0;i<PREFETCH_ROWS;i++){
  if(!reader_row_direct(r,i,sstv_m5_workspace.tx_rows[i]))return false;
  atomic_store(&r->produced,i+1);
 }
 atomic_store(&prefetch_reader,r);return true;
}
static void prefetch_stop(sstv_reader_t*r){
 if(!r->prefetched)return;
 atomic_store(&prefetch_reader,NULL);
 while(atomic_flag_test_and_set(&prefetch_busy))vTaskDelay(pdMS_TO_TICKS(1));
 r->prefetched=false;sstv_workspace_release(SSTV_WORKSPACE_TX);
 atomic_flag_clear(&prefetch_busy);
}
bool sstv_store_prefetch_tick(void){
 if(atomic_flag_test_and_set(&prefetch_busy))return true;
 sstv_reader_t*r=atomic_load(&prefetch_reader);bool active=r!=NULL;
 if(r&&!atomic_load(&r->read_failed)){
  unsigned n=atomic_load(&r->produced),used=atomic_load(&r->consumed);
  if(n<240&&n-used<PREFETCH_ROWS){
   if(reader_row_direct(r,n,sstv_m5_workspace.tx_rows[n%PREFETCH_ROWS]))atomic_store(&r->produced,n+1);
   else atomic_store(&r->read_failed,true);
  }
 }
 atomic_flag_clear(&prefetch_busy);return active;
}
static bool prefetch_row(sstv_reader_t*r,unsigned row,uint8_t*pixels){
 if(!pixels||row>=240||row!=atomic_load(&r->consumed))return false;
 int64_t deadline=esp_timer_get_time()+100000;
 while(atomic_load(&r->produced)<=row){
  if(atomic_load(&r->read_failed)||esp_timer_get_time()>=deadline)return false;
  vTaskDelay(pdMS_TO_TICKS(1));
 }
 memcpy(pixels,sstv_m5_workspace.tx_rows[row%PREFETCH_ROWS],640);
 atomic_store(&r->consumed,row+1);return true;
}
