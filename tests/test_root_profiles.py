"""Exercise actual public-root registry: dedup, capacity, errors and identity labels."""
import os,json,subprocess,tempfile,sys,time
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from fmo_identity import ephemeral_test_chain
r,*_=ephemeral_test_chain(int(time.time()))
source=(root/'ui-preview-firmware/main/root_profiles.inc').read_text().split('static esp_err_t roots_page')[0]
cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
code=r'''#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "cJSON.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
static int selected_ca;static int nv,fail_write;static char *storage[8];
static int nvs_get_str(int h,const char*k,char*out,size_t*n){(void)h;if(!strcmp(k,"cert_bi")||!strcmp(k,"cert_p0")){*n=32;return 0;}int i=-1;if(sscanf(k,"root_%d",&i)!=1||i<0||i>=8||!storage[i])return 1;size_t len=strlen(storage[i])+1;if(out){assert(*n>=len);memcpy(out,storage[i],len);}*n=len;return 0;}
static int nvs_set_str(int h,const char*k,const char*v){(void)h;if(fail_write)return 9;int i=-1;assert(sscanf(k,"root_%d",&i)==1&&i>=0&&i<8);free(storage[i]);storage[i]=strdup(v);return 0;}
static int nvs_commit(int h){(void)h;return 0;}
static const char*js(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
SOURCE
int main(void){const char*error=NULL;cJSON*r=cJSON_Parse(ROOT);assert(r);assert(root_slot(r,false,&error)==0);fail_write=1;assert(root_slot(r,true,&error)==0&&error);fail_write=0;error=NULL;assert(root_slot(r,true,&error)==3);assert(root_slot(r,true,&error)==3);assert(root_slot(r,false,&error)==3);
 for(int i=1;i<8;i++){char key[44];memset(key,'A'+i,43);key[43]=0;cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(r,"subject"),"publicKey",cJSON_CreateString(key));assert(root_slot(r,true,&error)==i+3);}
 cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(r,"subject"),"publicKey",cJSON_CreateString("overflow"));assert(!root_slot(r,true,&error));
 cJSON_ReplaceItemInObjectCaseSensitive(cJSON_GetObjectItemCaseSensitive(r,"subject"),"publicKey",cJSON_CreateString(root_keys[1]));assert(root_slot(r,true,&error)==2);
 cJSON*rows=cJSON_CreateArray();root_profile_row(rows,2,"BI4TES",root_keys[1],root_keys[1],true);root_profile_row(rows,3,"custom","other",root_keys[1],true);root_profile_row(rows,4,"root only","third",root_keys[1],true);
 assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,0),"connected")));assert(!cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,1),"connected")));assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,1),"canSwitch")));assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetArrayItem(rows,2),"canSwitch")));
 cJSON_Delete(rows);cJSON_Delete(r);for(int i=0;i<8;i++)free(storage[i]);puts("PASS root registry dedup, storage failure, capacity, builtins, current connection and root-only selection");}
'''.replace('ROOT',json.dumps(json.dumps(r))).replace('SOURCE',source)
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(cjson),str(p/'test.c'),str(cjson/'cJSON.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
