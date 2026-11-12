"""Replay actual profile boot migration across every storage-operation failure."""
import json,os,subprocess,tempfile,time,sys
from pathlib import Path
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from fmo_identity import ephemeral_test_chain
from cryptography.hazmat.primitives import serialization
r,i,u,ik,dk=ephemeral_test_chain(int(time.time()))
raw=json.dumps(dict(rootCert=r,intermediateCert=i,userCert=u))
seed=dk.private_bytes(serialization.Encoding.Raw,serialization.PrivateFormat.Raw,serialization.NoEncryption())
source=(root/'ui-preview-firmware/main/device_web.c').read_text();start=source.index('static esp_err_t apply_profile_switch(void)');end=source.index('esp_err_t device_web_start',start);function=source[start:end]
base=root/'ui-preview-firmware';cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
with tempfile.TemporaryDirectory() as td:
 p=Path(td)
 code='''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cert_wire.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 9
#define ESP_ERR_NO_MEM 8
#define ESP_ERR_NVS_NOT_FOUND 1
static int nv,target,op,fail_at,legacy;static unsigned char active_seed[32];static char active_cert[6145],active_call[8];
static const char *candidate=CERT;
static const unsigned char candidate_seed[32]={SEED};
static int fault(void){return ++op==fail_at?9:0;}
static const char*js(cJSON*j,const char*k){cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
int nvs_get_u8(int h,const char*k,uint8_t*v){(void)h;(void)k;if(!target)return 1;*v=target;return 0;}
int nvs_get_str(int h,const char*k,char*out,size_t*n){(void)h;(void)k;size_t size=strlen(candidate)+1;if(out){assert(*n>=size);memcpy(out,candidate,size);}*n=size;return 0;}
int nvs_get_blob(int h,const char*k,void*out,size_t*n){(void)h;(void)k;assert(*n>=32);memcpy(out,candidate_seed,32);*n=32;return 0;}
int nvs_set_blob(int h,const char*k,const void*v,size_t n){(void)h;assert(!strcmp(k,"seed")&&n==32);if(fault())return 9;memcpy(active_seed,v,32);return 0;}
int nvs_set_str(int h,const char*k,const char*v){(void)h;if(fault())return 9;if(!strcmp(k,"cert")){assert(!legacy&&!active_cert[0]);strcpy(active_cert,v);}else if(!strcmp(k,"callsign"))strcpy(active_call,v);else assert(0);return 0;}
int nvs_erase_key(int h,const char*k){(void)h;if(fault())return 9;if(!strcmp(k,"profile_next"))target=0;else if(!strcmp(k,"cert_backup"))legacy=0;else if(!strcmp(k,"cert"))active_cert[0]=0;return 0;}
int nvs_set_u8(int h,const char*k,uint8_t v){(void)h;assert(!strcmp(k,"relay_notice")&&v==1);return fault();}
int nvs_commit(int h){(void)h;return fault();}
FUNCTION
int main(void){for(int profile=1;profile<=10;profile++)for(int failure=1;failure<=12;failure++){target=profile;legacy=1;op=0;fail_at=failure;memset(active_seed,0,32);strcpy(active_cert,"previous certificate occupying NVS");active_call[0]=0;assert(apply_profile_switch()!=0);fail_at=op=0;assert(apply_profile_switch()==0);assert(!target);assert(!memcmp(active_seed,candidate_seed,32));assert(!strcmp(active_cert,candidate));assert(!strcmp(active_call,"BH1JSS"));}target=99;assert(apply_profile_switch()!=0);puts("PASS profile switch replays across 12 storage failures with full-store replacement; key and certificate remain paired");}
'''.replace('CERT',json.dumps(raw)).replace('SEED',','.join(str(n) for n in seed)).replace('FUNCTION',function)
 (p/'test.c').write_text(code)
 cmd=['cc','-Wall','-Wextra','-Werror','-I'+str(base/'main'),'-I'+str(base/'components/monocypher'),'-I'+str(cjson),str(p/'test.c'),str(base/'main/cert_wire.c'),str(base/'components/monocypher/monocypher.c'),str(base/'components/monocypher/monocypher-ed25519.c'),str(cjson/'cJSON.c'),'-lm','-o',str(p/'test')]
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
