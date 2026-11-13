"""Run the real gallery HTTP handler with real storage and cJSON, mocked transport."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'ui-preview-firmware/main/device_web.c').read_text()
handler=s[s.index('#ifdef FMO_SSTV_STREAMING\nstatic bool gallery_chunk('):s.index('static esp_err_t dispatch(')].replace('static esp_err_t wall_probe(request_t*r);','')
pre=r'''
#include "sstv_store.h"
#include "cJSON.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
typedef struct{void*http;cJSON*input,*output;bool usb;}request_t;
static bool authorized,busy;static int failed,sent;static uint8_t image[SSTV_BYTES],received[SSTV_BYTES];
static bool auth(request_t*r){(void)r;return authorized;}
static bool voice_busy(void){return busy;}
static cJSON*body(request_t*r){return cJSON_Duplicate(r->input,true);}
static int fail(request_t*r,const char*s){(void)r;(void)s;failed++;return 0;}
static int success(request_t*r,const char*s){(void)r;(void)s;return 0;}
static int response(request_t*r,cJSON*j){r->output=j;return 0;}
static int httpd_resp_set_type(void*r,const char*s){(void)r;(void)s;return 0;}
static int httpd_resp_set_hdr(void*r,const char*s,const char*v){(void)r;(void)s;(void)v;return 0;}
#ifndef FMO_SSTV_STREAMING
static int httpd_resp_send(void*r,const char*p,size_t n){(void)r;assert(n==SSTV_BYTES);memcpy(received,p,n);sent++;return 0;}
#else
static size_t offset;
static int httpd_resp_send_chunk(void*r,const char*p,size_t n){(void)r;if(n){assert(offset+n<=sizeof(received));memcpy(received+offset,p,n);offset+=n;}else{assert(offset==SSTV_BYTES);sent++;offset=0;}return 0;}
#endif
'''
tail=r'''
int main(void){char dir[]="/tmp/sstv-api-XXXXXX";assert(mkdtemp(dir));assert(!sstv_store_init(dir));memset(image,0x92,sizeof(image));uint32_t id;assert(sstv_store_save(image,123,&id));
request_t r={.http=(void*)1};r.input=cJSON_CreateObject();cJSON_AddNumberToObject(r.input,"id",id);
gallery_page(&r,"gallery");gallery_page(&r,"gallery_image");gallery_page(&r,"gallery_delete");assert(!r.output&&!sent&&!failed);assert(sstv_store_load(id,received));
authorized=true;gallery_page(&r,"gallery");assert(cJSON_GetArraySize(cJSON_GetObjectItem(r.output,"images"))==1);cJSON_Delete(r.output);
gallery_page(&r,"gallery_image");assert(sent==1&&!memcmp(image,received,sizeof(image)));
busy=true;gallery_page(&r,"gallery_delete");assert(failed==1&&sstv_store_load(id,received));busy=false;
r.usb=true;gallery_page(&r,"gallery_image");assert(failed==2&&sent==1);r.usb=false;
const char*bad[]={"{}","{\"id\":0}","{\"id\":-1}","{\"id\":1.5}","{\"id\":4294967296}","{\"id\":\"../private\"}"};
for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++){cJSON_Delete(r.input);r.input=cJSON_Parse(bad[i]);int f=failed;gallery_page(&r,"gallery_image");assert(failed==f+1&&sent==1);}
cJSON_Delete(r.input);r.input=cJSON_Parse("{\"id\":1}");gallery_page(&r,"gallery_delete");assert(!sstv_store_load(1,received));cJSON_Delete(r.input);rmdir(dir);
puts("PASS gallery handler: login required, exact pixels, bounded numeric IDs, no paths, busy deletion and USB restriction");}
'''
cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'api.c';p.write_text(pre+handler+tail)
 for flags in ([],['-DCONFIG_FMO_BOARD_XINGZHI'],['-DCONFIG_IDF_TARGET_ESP32']):
  subprocess.run(['cc',*flags,'-DSSTV_HOST_TEST','-Wall','-Wextra','-Werror','-I'+str(root/'ui-preview-firmware/main'),'-I'+str(cjson),str(p),str(root/'ui-preview-firmware/main/sstv_store.c'),str(cjson/'cJSON.c'),'-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
