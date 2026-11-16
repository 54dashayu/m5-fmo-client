from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];m=r/'ui-preview-firmware/main';cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON';source=(m/'device_web.c').read_text();fn=source[source.index('static bool station_call_locked('):source.index('static esp_err_t identity_reset_page(')]
pre=r'''
#include "station_settings.h"
#include "board_identity.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdatomic.h>
typedef int esp_err_t;
#define ESP_OK 0
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
typedef struct{bool authorized;cJSON*input,*output;}request_t;
static int nv,fail_commit;static bool busy;static char callsign[8]="BH1JSS",*bundle,blocked[20],voice_call[13];static struct{char call[8];}view;static station_settings_t station,persisted;static atomic_bool station_prefix=true;static int writes;
static struct{bool enabled;int port,passcode;char host[64];}tx_config;
static void station_tx_status(cJSON*j){(void)j;}
static bool station_tx_update(bool enabled,bool immediate){(void)enabled;(void)immediate;return true;}
static int nvs_get_str(int n,const char*k,char*out,size_t*size){(void)n;(void)out;*size=8;return !strcmp(k,blocked)?0:-1;}
static int nvs_set_str(int n,const char*k,const char*v){(void)n;(void)k;(void)v;writes++;return 0;}
static int nvs_set_blob(int n,const char*k,const void*v,size_t size){(void)n;(void)k;assert(size==sizeof(station));persisted=*(const station_settings_t*)v;writes++;return 0;}
static int nvs_commit(int n){(void)n;return fail_commit?-1:0;}
static bool auth(request_t*r){return r->authorized;}
static bool voice_busy(void){return busy;}
static void voice_display_call(const char*c){strcpy(voice_call,c);}
static bool wire_callsign(const char*c){return c&&(!strcmp(c,"BH1JSS")||!strcmp(c,"BH1ABC"));}
static cJSON*body(request_t*r){return cJSON_Duplicate(r->input,true);}
static const char*js(cJSON*j,const char*k){cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static int response(request_t*r,cJSON*j){cJSON_Delete(r->output);r->output=j;return 0;}
static int fail(request_t*r,const char*s){cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",false);cJSON_AddStringToObject(j,"error",s);return response(r,j);}
static int success(request_t*r,const char*s){cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",true);cJSON_AddStringToObject(j,"message",s);return response(r,j);}
static void input(request_t*r,const char*s){cJSON_Delete(r->input);r->input=cJSON_Parse(s);}
static bool ok(request_t*r){return cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(r->output,"ok"));}
'''
tail=r'''
int main(void){station_defaults(&station);request_t r={0};input(&r,"{\"action\":\"callsign\",\"callsign\":\"BH1ABC\"}");station_page(&r,true);assert(!writes);r.authorized=true;
 const char*keys[]={"cert_backup","cert_bh","cert_bi","enroll_req","enroll_bh","enroll_bi","cert_p0","cert_p7"};
 for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++){strcpy(blocked,keys[i]);station_page(&r,true);assert(!ok(&r)&&!writes&&!strcmp(callsign,"BH1JSS"));}
 blocked[0]=0;bundle="certificate";station_page(&r,true);assert(!ok(&r)&&!writes);bundle=NULL;busy=true;station_page(&r,true);assert(!ok(&r)&&!writes);busy=false;
 fail_commit=1;station_page(&r,true);assert(!ok(&r)&&!strcmp(callsign,"BH1JSS"));fail_commit=0;station_page(&r,true);assert(ok(&r)&&!strcmp(callsign,"BH1ABC")&&!strcmp(view.call,"BH1ABC"));
 input(&r,"{\"action\":\"settings\",\"prefix\":false,\"aprsSsid\":9,\"latitude\":null,\"longitude\":null,\"frequencyMHz\":0,\"antennaHeightM\":0,\"rig\":\"IC-705\",\"antenna\":\"GP\",\"remark\":\"\"}");
 fail_commit=1;station_page(&r,true);assert(!ok(&r)&&station.prefix&&atomic_load(&station_prefix));fail_commit=0;station_page(&r,true);assert(ok(&r)&&!station.prefix&&!atomic_load(&station_prefix)&&station.aprs_ssid==9&&!strcmp(voice_call,"BH1ABC"));assert(!memcmp(&station,&persisted,sizeof(station)));
 station_page(&r,false);assert(!strcmp(js(r.output,"callsign"),"BH1ABC"));assert(cJSON_GetObjectItemCaseSensitive(r.output,"aprsSsid")->valueint==9);
 cJSON_Delete(r.input);cJSON_Delete(r.output);puts("PASS station API auth, all cert/enrollment locks, busy guard, failed commits, save/readback and voice identity update");}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+fn+tail)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(m),'-I'+str(cj),'-I'+str(r/'ui-preview-firmware/components/fmo_board'),str(p),str(m/'station_settings.c'),str(cj/'cJSON.c'),'-lm','-o',td+'/test'],check=True);subprocess.run([td+'/test'],check=True)
