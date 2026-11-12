from pathlib import Path
import tempfile,subprocess,os
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 d=Path(td);(d/'freertos').mkdir()
 (d/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_NO_MEM 2\n#define ESP_ERR_INVALID_STATE 3\n#define ESP_ERR_NVS_NOT_FOUND 4\n')
 (d/'freertos/FreeRTOS.h').write_text('#pragma once\n#define portMAX_DELAY 0\n')
 (d/'freertos/semphr.h').write_text('#pragma once\ntypedef int SemaphoreHandle_t;\n#define xSemaphoreCreateMutex() 1\n#define xSemaphoreTake(a,b) ((void)0)\n#define xSemaphoreGive(a) ((void)0)\n')
 (d/'nvs.h').write_text('''#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
typedef int nvs_handle_t;
#define NVS_READWRITE 1
#define NVS_READONLY 0
int nvs_open(const char*,int,nvs_handle_t*);
void nvs_close(nvs_handle_t);
int nvs_get_blob(nvs_handle_t,const char*,void*,size_t*);
int nvs_set_blob(nvs_handle_t,const char*,const void*,size_t);
int nvs_get_str(nvs_handle_t,const char*,char*,size_t*);
int nvs_get_u8(nvs_handle_t,const char*,uint8_t*);
int nvs_commit(nvs_handle_t);
int nvs_erase_key(nvs_handle_t,const char*);
''')
 source=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nvs.h"
#include "cert_wire.h"
typedef struct{int ns;char key[16];void*data;size_t size;}cell;
static cell cells[256];static int writes,fail_at=-1,fail_commits;
int nvs_open(const char*s,int mode,nvs_handle_t*out){(void)mode;*out=!strcmp(s,"m5relays")?1:2;return 0;}
void nvs_close(nvs_handle_t h){(void)h;}
static cell*find(int ns,const char*k){for(int i=0;i<256;i++)if(cells[i].data&&cells[i].ns==ns&&!strcmp(cells[i].key,k))return &cells[i];return NULL;}
int nvs_set_blob(int ns,const char*k,const void*p,size_t n){if(++writes==fail_at)return ESP_FAIL;cell*c=find(ns,k);if(!c){for(int i=0;i<256;i++)if(!cells[i].data){c=&cells[i];break;}}if(!c)return ESP_FAIL;void*b=malloc(n);assert(b);memcpy(b,p,n);free(c->data);c->data=b;c->ns=ns;c->size=n;snprintf(c->key,sizeof(c->key),"%s",k);return 0;}
int nvs_get_blob(int ns,const char*k,void*p,size_t*n){cell*c=find(ns,k);if(!c)return ESP_ERR_NVS_NOT_FOUND;if(p){if(*n<c->size)return ESP_FAIL;memcpy(p,c->data,c->size);}*n=c->size;return 0;}
int nvs_get_str(int ns,const char*k,char*p,size_t*n){return nvs_get_blob(ns,k,p,n);}
int nvs_get_u8(int ns,const char*k,uint8_t*out){size_t n=1;return nvs_get_blob(ns,k,out,&n);}
int nvs_commit(int ns){(void)ns;if(fail_commits){fail_commits--;return ESP_FAIL;}return 0;}
int nvs_erase_key(int ns,const char*k){cell*c=find(ns,k);if(!c)return ESP_ERR_NVS_NOT_FOUND;free(c->data);memset(c,0,sizeof(*c));return 0;}
bool wire_callsign(const char*s){return s&&strlen(s)>=3&&strlen(s)<=6;}
bool wire_unb64(const char*s,uint8_t*out,size_t n){(void)out;return s&&strlen(s)==43&&n==32;}
#include "relays.c"
static cJSON*make(int uid){cJSON*j=cJSON_CreateObject();cJSON_AddStringToObject(j,"name","测试中继");cJSON_AddStringToObject(j,"callsign","BH1JSS");cJSON_AddStringToObject(j,"host","example.net");cJSON_AddStringToObject(j,"fingerprint","AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");cJSON_AddNumberToObject(j,"port",1883);cJSON_AddNumberToObject(j,"uid",uid);return j;}
static void restart(void){free(entries);entries=NULL;count=selected=0;temporary_active=false;assert(relays_init()==0);}
int main(void){
 // Legacy JSON migration retains four entries and selected relay, but not stale evidence.
 cJSON*j=cJSON_CreateObject(),*a=cJSON_AddArrayToObject(j,"relays");for(int i=1;i<=4;i++)cJSON_AddItemToArray(a,make(i));cJSON_AddNumberToObject(j,"selected",2);char*old=cJSON_PrintUnformatted(j);assert(!nvs_set_blob(1,"list",old,strlen(old)+1));free(old);cJSON_Delete(j);
 assert(relays_init()==0);assert(count==4&&relays_current(NULL,NULL)==2);relay_t r;assert(relays_get(2,&r)&&r.uid==3);assert(!relays_verified(&r));assert(find(1,"list"));
 for(int i=5;i<=24;i++){j=make(i);assert(!relays_add(j));cJSON_Delete(j);}assert(count==24);restart();assert(count==24&&selected==2);assert(relays_get(23,&r)&&r.uid==24);
 j=make(24);assert(relays_add(j));cJSON_Delete(j);assert(count==24);
 // Index-write failure leaves original list valid after reboot.
 j=make(25);fail_at=writes+2;assert(relays_add(j));cJSON_Delete(j);fail_at=-1;assert(count==24);restart();assert(count==24);
 j=make(25);fail_at=writes+1;assert(relays_add(j));fail_at=-1;cJSON_Delete(j);assert(count==24);
 assert(relays_select(23));assert(relays_current(&r,NULL)==23&&r.uid==24);relays_connection(&r,true);assert(relays_verified(&r));assert(relays_order_at(0)==23&&relays_order_position(23)==0);
 j=relays_json_page(0);assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(j,"relays"))==8);assert(cJSON_GetObjectItemCaseSensitive(j,"total")->valueint==24);assert(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(j,"relays"),0),"index")->valueint==23);cJSON_Delete(j);
 j=relays_json_page(16);assert(cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(j,"relays"))==8);cJSON_Delete(j);
 relay_t victim;assert(relays_get(0,&victim));fail_at=writes+1;assert(relays_remove(0,&victim));fail_at=-1;assert(count==24&&selected==23);assert(!relays_remove(0,&victim));assert(count==23&&selected==22);assert(relays_current(&r,NULL)==22&&r.uid==24);assert(relays_verified(&r));assert(relays_remove(22,&r));assert(relays_remove(0,&victim));
 j=make(24);cJSON_ReplaceItemInObject(j,"name",cJSON_CreateString("新中继名称"));assert(!relays_update_name(j));cJSON_Delete(j);assert(relays_current(&r,NULL)==22&&!strcmp(r.name,"新中继名称"));
 relays_clear_evidence();assert(!relays_verified(&r));restart();assert(count==23&&selected==22);assert(relays_current(&r,NULL)==22&&r.uid==24);assert(!strcmp(r.name,"新中继名称"));
 relay_t temp=r;temp.uid=999;relays_use_temporary(&temp);assert(relays_current(&r,NULL)==-1&&r.uid==999);fail_at=writes+1;assert(!relays_select(0));fail_at=-1;assert(relays_current(&r,NULL)==-1&&r.uid==999);assert(relays_select(0));assert(relays_current(&r,NULL)==0&&r.uid==2);
 // Committed index is authoritative; an orphan record is never displayed.
 assert(store_record(99,&temp));restart();assert(count==23);j=relays_json_page(999);assert(cJSON_GetObjectItemCaseSensitive(j,"offset")->valueint==16);cJSON_Delete(j);

 // Each CA owns a snapshot and selection; proof is durable but tied to exact device certificate.
 uint8_t ha[32]={1},hb[32]={2},replacement[32]={3};
 relays_configure(1,"BH1JSS",ha);restart();int original=count;assert(relays_get(0,&r));relays_connection(&r,true);assert(relays_select(1));restart();assert(selected==1&&relays_verified(&r));
 relays_configure(2,"BI4TES",hb);restart();assert(count==original&&!relays_verified(&r));assert(relays_select(2));assert(relays_get(0,&victim));assert(!relays_remove(0,&victim));j=make(8123);assert(!relays_add(j));cJSON_Delete(j);relays_connection(&r,false);restart();assert(selected==1&&count==original);
 relays_configure(1,"BH1JSS",ha);restart();assert(count==original&&selected==1&&relays_verified(&r));assert(relays_get(0,&victim)&&victim.uid==r.uid);
 relays_configure(1,"BH1JSS",replacement);restart();assert(!relays_verified(&r));
 relays_configure(1,"BH1JSS",ha);restart();assert(relays_verified(&r));
 // A failed proof write must never create durable evidence after reboot.
 relays_configure(2,"BI4TES",hb);restart();assert(relays_get(0,&r));fail_at=writes+1;relays_connection(&r,true);fail_at=-1;restart();assert(!relays_verified(&r));
 profile_id=0;strcpy(index_key,"index");memset(identity_hash,0,32);
 // Interrupted migration leaves legacy authoritative; retry succeeds.
 for(int k=0;k<256;k++){free(cells[k].data);memset(&cells[k],0,sizeof(cells[k]));}
 free(entries);entries=NULL;count=selected=0;
 j=cJSON_CreateObject();a=cJSON_AddArrayToObject(j,"relays");for(int k=1;k<=4;k++)cJSON_AddItemToArray(a,make(k));cJSON_AddNumberToObject(j,"selected",1);old=cJSON_PrintUnformatted(j);assert(!nvs_set_blob(1,"list",old,strlen(old)+1));free(old);cJSON_Delete(j);
 fail_at=writes+3;assert(relays_init()!=0);fail_at=-1;assert(find(1,"list")&&!find(2,"index"));restart();assert(count==4&&selected==1);
 puts("PASS storage: legacy migration, 24 records across restart, bounded page, auth order, rename, delete/select failures, rollback, no auto-switch and orphan isolation");return 0;}
'''
 (d/'test.c').write_text(source);cj=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+td,'-I'+str(cj),'-I'+str(root/'ui-preview-firmware/main'),str(d/'test.c'),str(cj/'cJSON.c'),'-lm','-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
