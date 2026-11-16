"""Run the real card-management API against simulated storage; never a real card."""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'ui-preview-firmware/main/device_web.c').read_text();code=s[s.index('static esp_err_t gallery_storage('):s.index('static esp_err_t dispatch(')]
pre=r'''
#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
static int nv;
typedef struct{cJSON*input,*output;}request_t;
static bool allowed,busy,receiving,sd,ready=true;static int formats,retries,selects,failures;static bool commit_fail;
static bool auth(request_t*r){(void)r;return allowed;}static bool voice_busy(void){return busy;}static bool sstv_rx_active(void){return receiving;}
static bool sstv_store_on_sd(void){return sd;}static bool sstv_sd_ready(void){return ready;}static bool sstv_sd_supported(void){return true;}static const char*sstv_sd_error(void){return "ESP_ERR_INVALID_CRC";}
static bool sstv_sd_space(uint64_t*t,uint64_t*u){*t=16000000000ULL;*u=1000000;return true;}
static cJSON*body(request_t*r){return cJSON_Duplicate(r->input,true);}static const char*js(cJSON*j,const char*k){cJSON*v=cJSON_GetObjectItem(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static int fail(request_t*r,const char*s){(void)r;(void)s;failures++;return 0;}static int success(request_t*r,const char*s){(void)r;(void)s;return 0;}static int response(request_t*r,cJSON*j){r->output=j;return 0;}
static const char*sstv_store_select(bool s){selects++;if(s&&!ready)return "not ready";sd=s;return NULL;}
static int card_job_status(request_t*r){(void)r;return 0;}
static const char*card_job_start(bool format){if(format)formats++;else retries++;return NULL;}
static int nvs_set_u8(int n,const char*k,uint8_t v){(void)n;(void)k;(void)v;return 0;}static int nvs_commit(int n){(void)n;return commit_fail?-1:0;}
'''
tail=r'''
int main(void){request_t r={0};gallery_storage(&r,"gallery_card_format");assert(!formats&&!retries&&!selects&&!failures);
allowed=true;gallery_storage(&r,"gallery_storage");assert(!formats&&!retries);assert(cJSON_GetObjectItem(r.output,"sdTotal")->valuedouble==16000000000.0);cJSON_Delete(r.output);
r.input=cJSON_Parse("{\"confirm\":\"wrong\"}");gallery_storage(&r,"gallery_card_format");assert(!formats&&failures==1);cJSON_Delete(r.input);
r.input=cJSON_Parse("{\"confirm\":\"ERASE_TF_CARD\"}");busy=true;gallery_storage(&r,"gallery_card_format");assert(!formats);busy=false;receiving=true;gallery_storage(&r,"gallery_card_format");assert(!formats);receiving=false;gallery_storage(&r,"gallery_card_format");assert(formats==1);cJSON_Delete(r.input);
r.input=cJSON_Parse("{\"medium\":\"sd\"}");ready=false;gallery_storage(&r,"gallery_storage_select");assert(!sd);ready=true;commit_fail=true;gallery_storage(&r,"gallery_storage_select");assert(!sd);commit_fail=false;gallery_storage(&r,"gallery_storage_select");assert(sd);cJSON_Delete(r.input);r.input=NULL;gallery_storage(&r,"gallery_card_retry");assert(retries==1&&formats==1);
puts("PASS card API: auth, format confirmation, busy/RX rejection, 64-bit capacity, failed selection and persistence rollback; simulated format only");}
'''
c=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+code+tail);subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(c),str(p),str(c/'cJSON.c'),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
