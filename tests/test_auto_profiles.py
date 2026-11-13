"""Run actual automatic-CA storage, validation, routing and reboot intent with host stubs."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];main=root/'ui-preview-firmware/main';cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
pre=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "auto_ca_policy.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 9
#define ESP_ERR_NVS_NOT_FOUND 1
#define portMAX_DELAY 0
#define xSemaphoreTake(a,b) ((void)0)
#define xSemaphoreGive(a) ((void)0)
typedef struct{char name[65],call[8],host[64],fingerprint[44];uint32_t uid;int port;}relay_t;
typedef struct{cJSON*input,*output;bool usb;}request_t;
static uint8_t selected_ca=1;static int nv,commits_fail,write_fail,switched,busy;static bool identity_restart_pending,auto_connection_blocked;
static char *bundle;static char callsign[8]="BH1JSS";static uint8_t pk[32];
static atomic_uint auto_ca_mask;static atomic_int auto_relay_pending,ui_profile_request;static relay_t auto_relay_target;
static struct{char profile_message[96];}view;static int64_t auto_message_until;
static time_t wall=1700000100;static time_t fake_time(time_t*p){if(p)*p=wall;return wall;}
#define time fake_time
static int64_t esp_timer_get_time(void){return 1000000;}
static void crypto_wipe(void*p,size_t n){memset(p,0,n);}
static void crypto_ed25519_key_pair(uint8_t*sk,uint8_t*pub,const uint8_t*seed){memset(sk,0,64);memcpy(pub,seed,32);}
static int mbedtls_sha256(const uint8_t*p,size_t n,uint8_t*out,int mode){(void)mode;uint32_t h=2166136261;for(size_t i=0;i<n;i++)h=(h^p[i])*16777619;for(int i=0;i<32;i++)out[i]=(h>>((i%4)*8))&255;return 0;}
static struct{char key[20];uint8_t value[6200];size_t size;}store[32];
static int slot(const char*k,bool create){for(int i=0;i<32;i++)if(!strcmp(store[i].key,k))return i;for(int i=0;create&&i<32;i++)if(!store[i].key[0]){strcpy(store[i].key,k);return i;}return -1;}
static int nvs_set_blob(int h,const char*k,const void*v,size_t n){(void)h;if(write_fail)return 9;int i=slot(k,true);assert(i>=0&&n<=6200);memcpy(store[i].value,v,n);store[i].size=n;return 0;}
static int nvs_get_blob(int h,const char*k,void*v,size_t*n){(void)h;int i=slot(k,false);if(i<0)return 1;if(v){assert(*n>=store[i].size);memcpy(v,store[i].value,store[i].size);}*n=store[i].size;return 0;}
static int nvs_get_str(int h,const char*k,char*v,size_t*n){return nvs_get_blob(h,k,v,n);}
static int nvs_set_str(int h,const char*k,const char*v){return nvs_set_blob(h,k,v,strlen(v)+1);}
static int nvs_set_u32(int h,const char*k,uint32_t v){return nvs_set_blob(h,k,&v,4);}
static int nvs_get_u8(int h,const char*k,uint8_t*v){size_t n=1;return nvs_get_blob(h,k,v,&n);}
static int nvs_erase_key(int h,const char*k){(void)h;int i=slot(k,false);if(i<0)return 1;memset(&store[i],0,sizeof(store[i]));return 0;}
static int nvs_commit(int h){(void)h;if(commits_fail){commits_fail--;return 9;}return 0;}
static const char*js(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static const char*root_key(const cJSON*r){return js(cJSON_GetObjectItem(r,"subject"),"publicKey");}
static bool root_selectable(const char*k){return k&&strcmp(k,"blocked");}
static bool profile_exists(int id){return id>=1&&id<=3;}
static int root_slot(cJSON*r,bool create,const char**e){(void)create;(void)e;const char*k=root_key(r);return !k?0:!strcmp(k,"BH")?1:!strcmp(k,"TES")?2:!strcmp(k,"CUSTOM")?3:0;}
static void profile_bank_keys(int id,char*c,char*s){snprintf(c,16,"cert%d",id);snprintf(s,16,"seed%d",id);}
static const char*wire_validate_bundle(const cJSON*b,const uint8_t*pub,const char*call,time_t now){return cJSON_IsTrue(cJSON_GetObjectItem(b,"valid"))&&js(b,"call")&&!strcmp(js(b,"call"),call)&&js(b,"key")&&pub[0]==(uint8_t)js(b,"key")[0]&&cJSON_GetNumberValue(cJSON_GetObjectItem(b,"expires"))>now?NULL:"invalid certificate";}
static esp_err_t fail(request_t*r,const char*s){r->output=cJSON_CreateObject();cJSON_AddStringToObject(r->output,"error",s);return 0;}
static esp_err_t success(request_t*r,const char*s){r->output=cJSON_CreateObject();cJSON_AddStringToObject(r->output,"message",s);return 0;}
static bool voice_busy(void){return busy;}
static void voice_connected(bool b){(void)b;}
static relay_t saved[10];static int saved_count,chosen=-1;static bool relay_fail;
static int relays_count(void){return saved_count;}
static bool relays_get(int i,relay_t*r){if(i<0||i>=saved_count)return false;*r=saved[i];return true;}
static bool relays_select(int i){if(relay_fail)return false;chosen=i;return true;}
static const char*relays_add(cJSON*j){if(relay_fail)return "store failed";relay_t*r=&saved[saved_count++];snprintf(r->name,65,"%s",js(j,"name"));snprintf(r->call,8,"%s",js(j,"callsign"));snprintf(r->host,64,"%s",js(j,"host"));snprintf(r->fingerprint,44,"%s",js(j,"fingerprint"));r->uid=cJSON_GetNumberValue(cJSON_GetObjectItem(j,"uid"));r->port=cJSON_GetNumberValue(cJSON_GetObjectItem(j,"port"));return NULL;}
static char accepted[10][44];static int accepted_count;
static int discovery_accepted_roots(const relay_t*t,char out[10][44]){(void)t;memcpy(out,accepted,sizeof(accepted));return accepted_count;}
static esp_err_t rollback_page(request_t*r){int id=js(r->input,"provider")?(!strcmp(js(r->input,"provider"),"bh1jss")?1:2):(int)cJSON_GetNumberValue(cJSON_GetObjectItem(r->input,"profileId"));switched=id;uint8_t v=id;nvs_set_blob(nv,"profile_next",&v,1);identity_restart_pending=true;return success(r,"switching");}
'''
tail=r'''
static void bank(int id,bool good,int expires){char ck[16],sn[16],raw[512];profile_bank_keys(id,ck,sn);snprintf(raw,sizeof(raw),"{\"rootCert\":{\"subject\":{\"publicKey\":\"%s\"}},\"valid\":%s,\"call\":\"BH1JSS\",\"key\":\"%c\",\"expires\":%d}",id==1?"BH":id==2?"TES":"CUSTOM",good?"true":"false",'A'+id,expires);nvs_set_str(nv,ck,raw);uint8_t seed[32]={0};seed[0]='A'+id;nvs_set_blob(nv,sn,seed,32);}
static void active(int id){free(bundle);char ck[16],sn[16];profile_bank_keys(id,ck,sn);size_t n=0;nvs_get_str(nv,ck,NULL,&n);bundle=malloc(n);nvs_get_str(nv,ck,bundle,&n);n=32;nvs_get_blob(nv,sn,pk,&n);nvs_set_blob(nv,"seed",pk,32);selected_ca=id;identity_restart_pending=false;}
static bool configure(const char*json){request_t r={.input=cJSON_Parse(json),.usb=true};auto_profiles_save(&r,r.input);bool ok=!js(r.output,"error");cJSON_Delete(r.input);cJSON_Delete(r.output);return ok;}
int main(void){
 bank(1,true,1800000000);bank(2,true,1800000000);bank(3,false,1800000000);active(1);
 assert(auto_profile_valid(1,NULL,NULL)&&auto_profile_valid(2,NULL,NULL));assert(!auto_profile_valid(3,NULL,NULL));
 assert(configure("{\"profiles\":[1,2]}"));assert(atomic_load(&auto_ca_mask)==3&&!switched);
 assert(!configure("{\"profiles\":[1,3]}"));assert(!configure("{\"profiles\":[2,2]}"));assert(!configure("{\"profiles\":[2.5]}"));assert(atomic_load(&auto_ca_mask)==3);
 commits_fail=1;assert(!configure("{\"profiles\":[]}"));assert(atomic_load(&auto_ca_mask)==3);
 busy=1; // caller's roots endpoint rejects busy requests; routing also checks independently.
 relay_t target={.name="Passport",.call="BI4TES",.host="example.net",.fingerprint="fingerprint",.uid=1845,.port=1883};strcpy(accepted[0],"TES");accepted_count=1;auto_route_relay(&target);assert(!switched);busy=0;
 write_fail=1;auto_route_relay(&target);assert(!switched&&selected_ca==1);write_fail=0;
 auto_route_relay(&target);assert(switched==2&&selected_ca==1);assert(slot("auto_target",false)>=0&&slot("cert1",false)>=0);
 // Pending switch survives interruption without selecting an unrelated default.
 auto_resume_relay();assert(auto_connection_blocked&&slot("auto_target",false)>=0);
 active(2);nvs_erase_key(nv,"profile_next");saved_count=0;auto_resume_relay();assert(!auto_connection_blocked&&chosen==0&&saved[0].uid==1845&&slot("auto_target",false)<0);
 switched=0;accepted_count=2;strcpy(accepted[1],"BH");auto_route_relay(&target);assert(!switched&&selected_ca==2);
 assert(configure("{\"profiles\":[]}"));assert(!atomic_load(&auto_ca_mask));auto_route_relay(&target);assert(!switched);
 bank(1,true,1600000000);assert(!auto_profile_valid(1,NULL,NULL));bank(1,true,1800000000);uint8_t bad[32]={0};nvs_set_blob(nv,"seed1",bad,32);assert(!auto_profile_valid(1,NULL,NULL));
 wall=0;assert(!auto_profile_valid(2,NULL,NULL));wall=1700000100;
 cJSON*out=cJSON_CreateObject(),*rows=cJSON_AddArrayToObject(out,"profiles"),*row=cJSON_CreateObject();cJSON_AddNumberToObject(row,"id",2);cJSON_AddItemToArray(rows,row);auto_profiles_describe(out);assert(cJSON_IsTrue(cJSON_GetObjectItem(row,"autoAvailable")));cJSON_Delete(out);
 atomic_store(&auto_relay_pending,1);auto_relay_target=target;auto_relay_tick();assert(!atomic_load(&auto_relay_pending));
 (void)ui_profile_request;free(bundle);puts("PASS actual auto-CA validation, multi-select, failed commits, disabled/expired/wrong-key banks, interrupted switch, exact target resume and current-profile priority");
}
'''
with tempfile.TemporaryDirectory() as td:
 p=Path(td)/'test.c';p.write_text(pre+(main/'auto_profiles.inc').read_text()+tail)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(main),'-I'+str(cj),str(p),str(cj/'cJSON.c'),'-o',td+'/test'],check=True)
 subprocess.run([td+'/test'],check=True)
