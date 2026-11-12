"""Exercise actual per-provider job persistence and certificate validation in C."""
import os,json,sys,time,tempfile,subprocess
from pathlib import Path
root=Path(__file__).resolve().parents[1];sys.path.insert(0,str(root/'tools'))
from fmo_identity import ephemeral_test_chain,b64
from cryptography.hazmat.primitives import serialization
r,i,u,ik,dk=ephemeral_test_chain(int(time.time()))
seed=dk.private_bytes(serialization.Encoding.Raw,serialization.PrivateFormat.Raw,serialization.NoEncryption())
request=json.dumps({'callsign':'BH1JSS','publicKey':u['subject']['publicKey']})
bundle=json.dumps(dict(rootCert=r,intermediateCert=i,userCert=u))
base=root/'ui-preview-firmware';cjson=Path(os.environ['IDF_PATH'])/'components/json/cJSON'
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cert_wire.h"
#include "monocypher.h"
#include "monocypher-ed25519.h"
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_NVS_NOT_FOUND 2
#define ESP_ERR_INVALID_ARG 3
#define ESP_ERR_NO_MEM 4
static int nv,fail_write,fail_at,write_count;static uint8_t selected_ca;static char selected_ca_name[97];static bool identity_restart_pending;static char*bundle;
static const char*root_keys[]={ROOT,ROOT};
struct entry {char key[32];unsigned char data[8192];size_t n;};static struct entry db[24];
static struct entry*find(const char*k,int create){for(int i=0;i<24;i++)if(!strcmp(db[i].key,k))return &db[i];if(create)for(int i=0;i<24;i++)if(!db[i].key[0]){strcpy(db[i].key,k);return &db[i];}return NULL;}
int nvs_get_blob(int h,const char*k,void*out,size_t*n){(void)h;struct entry*e=find(k,0);if(!e)return 2;if(out){if(*n<e->n)return 3;memcpy(out,e->data,e->n);}*n=e->n;return 0;}
int nvs_get_str(int h,const char*k,char*out,size_t*n){return nvs_get_blob(h,k,out,n);}
int nvs_set_blob(int h,const char*k,const void*v,size_t n){(void)h;if(fail_write||(++write_count==fail_at))return 1;struct entry*e=find(k,1);assert(e&&n<=8192);memcpy(e->data,v,n);e->n=n;return 0;}
int nvs_set_str(int h,const char*k,const char*v){return nvs_set_blob(h,k,v,strlen(v)+1);}
int nvs_erase_key(int h,const char*k){(void)h;if(fail_write||(++write_count==fail_at))return 1;struct entry*e=find(k,0);if(!e)return 2;memset(e,0,sizeof(*e));return 0;}
int nvs_commit(int h){(void)h;return fail_write||(++write_count==fail_at)?1:0;}
int nvs_get_u8(int h,const char*k,uint8_t*v){size_t n=1;return nvs_get_blob(h,k,v,&n);}
int nvs_set_u8(int h,const char*k,uint8_t v){return nvs_set_blob(h,k,&v,1);}
static int root_slot(cJSON*j,bool create,const char**error){(void)j;(void)create;(void)error;return 1;}
static const char*js(cJSON*j,const char*k){cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static const char*root_key(cJSON*r){return js(cJSON_GetObjectItemCaseSensitive(r,"subject"),"publicKey");}
static cJSON*stored_json(const char*k,size_t cap){struct entry*e=find(k,0);return e&&e->n<=cap?cJSON_Parse((char*)e->data):NULL;}
#include "enrollment_jobs.inc"
#include "empty_profile.inc"
int main(void){
 unsigned char seed[32]={SEED};unsigned char other[32];memset(other,99,32);
 nvs_set_blob(nv,"seed",seed,32);nvs_set_str(nv,"cert","active identity unchanged");
 assert(enroll_save(0,REQ)==0);assert(enroll_save(1,REQ)==0);
 enrollment_job_t a,b;assert(enroll_read(0,&a)&&enroll_read(1,&b));
 // Changing the active seed does not change either task's application key.
 nvs_set_blob(nv,"seed",other,32);assert(enroll_read(1,&b)&&!memcmp(b.seed,seed,32));
 assert(enroll_cancel(0)==0);assert(!enroll_read(0,&a)&&enroll_read(1,&b));
 cJSON*pack=cJSON_Parse(PACK);assert(enroll_store_ready(1,&b,pack)==NULL);assert(!enroll_read(1,&b));assert(find("cert_bi",0));assert(!memcmp(find("seed_bi",0)->data,seed,32));assert(!strcmp((char*)find("cert",0)->data,"active identity unchanged"));assert(!memcmp(find("seed",0)->data,other,32));
 // Bad signature and wrong request key never replace a bank.
 cJSON*sig=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(pack,"userCert"),"signature");sig->valuestring[0]=sig->valuestring[0]=='A'?'B':'A';assert(enroll_store_ready(0,&b,pack)!=NULL);assert(!find("cert_bh",0));cJSON_Delete(pack);
 pack=cJSON_Parse(PACK);memcpy(b.seed,other,32);assert(enroll_store_ready(0,&b,pack)!=NULL);assert(!find("cert_bh",0));cJSON_Delete(pack);
 // Legacy pending application migrates once and preserves its original key.
 nvs_set_blob(nv,"seed",seed,32);nvs_set_str(nv,"enroll_req",REQ);assert(enroll_migrate()==0);assert(enroll_read(0,&a));assert(!find("enroll_req",0));assert(enroll_migrate()==0);
 fail_write=1;assert(enroll_cancel(0)!=0);assert(enroll_read(0,&a));fail_write=0;
 cJSON*status=cJSON_CreateObject();enroll_status(status);assert(cJSON_GetObjectItemCaseSensitive(status,"enrollments"));cJSON_Delete(status);
 // Root-only selection preserves active identity in its bank and pending key.
 nvs_erase_key(nv,"cert_bi");nvs_set_blob(nv,"seed",seed,32);assert(enroll_save(1,REQ)==0);nvs_set_blob(nv,"seed",other,32);bundle=PACK;nvs_set_str(nv,"cert",bundle);
 assert(prepare_empty_profile(2)==NULL);assert(!memcmp(find("seed_bi",0)->data,seed,32));assert(find("cert",0)&&find("cert_bh",0));
 static struct entry snapshot[24];memcpy(snapshot,db,sizeof(db));
 for(int failure=1;failure<=7;failure++){memcpy(db,snapshot,sizeof(db));write_count=0;fail_at=failure;assert(apply_empty_profile()!=0);fail_at=0;write_count=0;assert(apply_empty_profile()==0);assert(!find("cert",0)&&find("cert_bh",0));assert(!memcmp(find("seed",0)->data,seed,32));assert(enroll_read(1,&b));}
 memcpy(db,snapshot,sizeof(db));
 assert(apply_empty_profile()==0);assert(!find("cert",0));assert(find("cert_bh",0));assert(!memcmp(find("seed",0)->data,seed,32));uint8_t selected=0;assert(nvs_get_u8(nv,"selected_ca",&selected)==0&&selected==2);assert(enroll_read(1,&b));assert(apply_empty_profile()==0);
 char label[97];profile_name_for(2,label,sizeof(label));assert(strstr(label,"BI4TES"));assert(prepare_empty_profile(99)!=NULL);
 crypto_wipe(&a,sizeof(a));crypto_wipe(&b,sizeof(b));puts("PASS independent tasks, seed binding, validated storage without activation, migration and cancellation failure");
}
'''.replace('ROOT',json.dumps(r['subject']['publicKey'])).replace('SEED',','.join(map(str,seed))).replace('REQ',json.dumps(request)).replace('PACK',json.dumps(bundle))
with tempfile.TemporaryDirectory() as td:
 p=Path(td);(p/'test.c').write_text(code)
 subprocess.run(['cc','-Wall','-Wextra','-Werror','-I'+str(base/'main'),'-I'+str(base/'components/monocypher'),'-I'+str(cjson),str(p/'test.c'),str(base/'main/cert_wire.c'),str(base/'components/monocypher/monocypher.c'),str(base/'components/monocypher/monocypher-ed25519.c'),str(cjson/'cJSON.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
