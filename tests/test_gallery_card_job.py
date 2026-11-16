"""Exercise the production asynchronous job; no physical card operations."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
pre=r'''
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include "cJSON.h"
typedef int esp_err_t;
#define ESP_OK 0
#define pdPASS 1
typedef struct {cJSON *output;}request_t;
static bool busy,rx,allowed=true;
static atomic_bool card_worker_ready=true;
static unsigned stage,calls;static const char *store_error;

static bool voice_busy(void){return busy;}static bool sstv_rx_active(void){return rx;}
static bool auth(request_t*r){(void)r;return allowed;}
static int response(request_t*r,cJSON*j){r->output=j;return 0;}
static const char*sstv_store_card_manage(bool format){(void)format;calls++;stage=4;return store_error;}
static unsigned sstv_sd_manage_stage(void){return stage;}
static void sstv_sd_manage_reset(void){stage=0;}
'''
post=r'''
int main(void){request_t r={0};stage=4;assert(!card_job_start(true));assert(stage==0&&atomic_load(&card_job_active)&&!calls);
assert(card_job_start(true));assert(atomic_load(&card_job_id)==1);
for(stage=0;stage<4;stage++){card_job_status(&r);assert(cJSON_IsTrue(cJSON_GetObjectItem(r.output,"active")));assert(cJSON_GetObjectItem(r.output,"completedStages")->valueint==(int)stage);cJSON_Delete(r.output);}
card_job_tick();assert(!atomic_load(&card_job_active)&&calls==1);card_job_status(&r);assert(cJSON_IsTrue(cJSON_GetObjectItem(r.output,"success")));cJSON_Delete(r.output);
assert(!card_job_start(false));busy=true;card_job_tick();assert(calls==1&&!card_job_ok);busy=false;
store_error="write failed";assert(!card_job_start(true));card_job_tick();assert(!card_job_ok);card_job_status(&r);assert(!cJSON_IsTrue(cJSON_GetObjectItem(r.output,"success")));cJSON_Delete(r.output);
card_worker_ready=false;assert(card_job_start(true));assert(!atomic_load(&card_job_active));allowed=false;r.output=NULL;card_job_status(&r);assert(!r.output);
puts("PASS card job: nonblocking start, live stages, duplicate exclusion, success, busy, write failure, worker readiness and authentication");}
'''
c=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+(r/'ui-preview-firmware/main/gallery_card_job.inc').read_text()+post)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(c),str(p),str(c/'cJSON.c'),'-o',td+'/test'],check=True)
 subprocess.run([td+'/test'],check=True)
