"""Exercise the actual Xiaozhi save worker with delayed completion and failures."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/main.c').read_text()
a=s.index('/* Publish the result only');b=s.index('\n#endif',a);worker=s[a:b]
g=s.index(' if(page==SSTV&&atomic_load(&sstv_save_work))return;');guard=s[g:s.index('\n',g)]
pre=r'''
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
enum {SSTV=1,SSTV_COMPLETE=2,SSTV_INTERRUPTED=3};
#define pdPASS 1
static int page=SSTV,sstv_status=SSTV_COMPLETE,creates,writes;
static bool create_ok=true,save_ok=true;
static char image_notice[112];static void (*pending)(void*);
static int xTaskCreate(void (*fn)(void*),const char*n,unsigned stack,void*a,int priority,void*h){(void)n;(void)stack;(void)a;(void)priority;(void)h;creates++;if(!create_ok)return 0;pending=fn;return 1;}
static void vTaskDelete(void*a){(void)a;}
static bool sstv_rx_save(uint32_t*id){writes++;if(save_ok)*id=42;return save_ok;}
static bool sstv_rx_save_partial(uint32_t*id){writes++;if(save_ok)*id=43;return save_ok;}
static const char*sstv_store_error(void){return NULL;}
'''
tail=r'''
int main(void){
key();assert(creates==1&&writes==0&&atomic_load(&sstv_save_work)==1);
for(int i=0;i<20;i++)key();assert(creates==1&&writes==0);
pending(NULL);assert(writes==1&&sstv_save_ok&&sstv_save_id==42&&atomic_load(&sstv_save_work)==2);
key();assert(creates==1);atomic_store(&sstv_save_work,0);
save_ok=false;key();pending(NULL);assert(!sstv_save_ok&&sstv_save_id==0);assert(!strstr(sstv_save_message,"相册已满"));atomic_store(&sstv_save_work,0);
save_ok=true;sstv_status=SSTV_INTERRUPTED;key();pending(NULL);assert(sstv_save_ok&&sstv_save_id==43);atomic_store(&sstv_save_work,0);
create_ok=false;key();assert(!atomic_load(&sstv_save_work));assert(strstr(image_notice,"保存任务未启动"));
puts("PASS delayed save: no duplicate jobs before/after completion, success/failure/partial and task allocation failure");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(pre+worker+'\nstatic void key(void){'+guard+'sstv_save_begin();}\n'+tail)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',d+'/test'],check=True);subprocess.run([d+'/test'],check=True)
