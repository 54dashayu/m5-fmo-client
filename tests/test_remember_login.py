from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];s=(root/'ui-preview-firmware/main/device_web.c').read_text()
helpers=s[s.index('static int remembered_index('):s.index('static cJSON *body(')]
login=s[s.index('static esp_err_t login_page('):s.index('\nextern unsigned ui_key_events')]
pre=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "cJSON.h"
typedef int esp_err_t;typedef void httpd_req_t;
#define ESP_OK 0
#define HTTPD_401_UNAUTHORIZED 401
static char remembered[4][44],durable[4][44],pending[4][44],token[44],code[17]="12345678",header[48];static int nv,fail_commit,nonce;
typedef struct{bool usb;httpd_req_t*http;cJSON*input,*output;}request_t;
static int64_t esp_timer_get_time(void){static int64_t t=0;return t+=3000000;}
static int nvs_set_blob(int n,const char*k,const void*p,size_t len){(void)n;(void)k;assert(len==sizeof(pending));memcpy(pending,p,len);return 0;}
static int nvs_commit(int n){(void)n;if(fail_commit)return 1;memcpy(durable,pending,sizeof(durable));return 0;}
static size_t httpd_req_get_hdr_value_len(void*r,const char*k){(void)r;(void)k;return strlen(header);}
static int httpd_req_get_hdr_value_str(void*r,const char*k,char*out,size_t len){(void)r;(void)k;snprintf(out,len,"%s",header);return 0;}
static void httpd_resp_send_err(void*r,int n,const char*s){(void)r;(void)n;(void)s;}
static void httpd_resp_set_status(void*r,const char*s){(void)r;(void)s;}
static void esp_fill_random(void*p,size_t n){memset(p,++nonce,n);}
static void wire_b64(const uint8_t*p,size_t n,char*out){(void)n;memset(out,'A'+p[0],43);out[43]=0;}
static void crypto_wipe(void*p,size_t n){memset(p,0,n);}
static int response(request_t*r,cJSON*j){cJSON_Delete(r->output);r->output=j;return 0;}
static int fail(request_t*r,const char*s){(void)s;return response(r,cJSON_CreateFalse());}
static int success(request_t*r,const char*s){(void)s;return response(r,cJSON_CreateTrue());}
static cJSON*body(request_t*r){return cJSON_Duplicate(r->input,true);}
static const char*js(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
'''
tail=r'''
int main(void){memset(token,'S',43);request_t r={.http=(void*)1};assert(!auth(&r));r.input=cJSON_Parse("{\"code\":\"wrong\",\"remember\":true}");login_page(&r);assert(cJSON_IsFalse(r.output));cJSON_Delete(r.input);r.input=cJSON_Parse("{\"code\":\"12345678\",\"remember\":true}");login_page(&r);const char*issued=js(r.output,"token");assert(issued&&strlen(issued)==43);strcpy(header,issued);assert(auth(&r));memset(remembered,0,sizeof(remembered));memcpy(remembered,durable,sizeof(remembered));memset(token,'T',43);assert(auth(&r));logout_page(&r);assert(!auth(&r));memcpy(remembered,durable,sizeof(remembered));assert(!auth(&r));fail_commit=1;login_page(&r);assert(cJSON_IsFalse(r.output));assert(remembered[0][0]==0);fail_commit=0;login_page(&r);char first[44];strcpy(first,js(r.output,"token"));for(int i=0;i<4;i++)login_page(&r);strcpy(header,first);assert(!auth(&r));strcpy(header,js(r.output,"token"));assert(auth(&r));cJSON_Delete(r.input);cJSON_Delete(r.output);puts("PASS remembered login, reboot, logout, failed persistence and bounded eviction");return 0;}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(pre+helpers+login+tail);cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(cj),str(p),str(cj/'cJSON.c'),'-lm','-o',d+'/test'],check=True);subprocess.run([d+'/test'],check=True)
