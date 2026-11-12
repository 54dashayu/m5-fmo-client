"""Replay real identity migration after every interrupted NVS operation."""
from pathlib import Path
import subprocess,tempfile,os
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as td:
 p=Path(td)
 (p/'nvs.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
typedef int esp_err_t;typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_INVALID_ARG 2
int nvs_get_blob(int,const char*,void*,size_t*);int nvs_set_blob(int,const char*,const void*,size_t);int nvs_set_str(int,const char*,const char*);int nvs_erase_key(int,const char*);int nvs_commit(int);
''')
 (p/'test.c').write_text('''#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "identity_transition.h"
static identity_transition_t journal;static int pending,cert,backup,bh,bi,sbh,sbi,profile,request,reqbh,reqbi,remember,code,fail_at,op;static unsigned char saved_seed[32];static char saved_call[8];static int wifi=77,extra_cert[8],extra_seed[8];
bool wire_callsign(const char*c){return c&&!strcmp(c,"BI4TES");}
void crypto_wipe(void*p,size_t n){memset(p,0,n);}
static int fault(void){return ++op==fail_at?9:0;}
int nvs_get_blob(int h,const char*k,void*out,size_t*n){(void)h;assert(!strcmp(k,"identity_next"));if(!pending)return ESP_ERR_NVS_NOT_FOUND;memcpy(out,&journal,sizeof(journal));*n=sizeof(journal);return 0;}
int nvs_set_blob(int h,const char*k,const void*v,size_t n){(void)h;assert(!strcmp(k,"seed"));if(fault())return 9;memcpy(saved_seed,v,n);return 0;}
int nvs_set_str(int h,const char*k,const char*v){(void)h;assert(!strcmp(k,"callsign"));if(fault())return 9;strcpy(saved_call,v);return 0;}
int nvs_erase_key(int h,const char*k){(void)h;if(fault())return 9;int *v=!strncmp(k,"cert_p",6)?&extra_cert[atoi(k+6)]:!strncmp(k,"seed_p",6)?&extra_seed[atoi(k+6)]:!strcmp(k,"cert")?&cert:!strcmp(k,"cert_backup")?&backup:!strcmp(k,"cert_bh")?&bh:!strcmp(k,"cert_bi")?&bi:!strcmp(k,"seed_bh")?&sbh:!strcmp(k,"seed_bi")?&sbi:!strcmp(k,"profile_next")?&profile:!strcmp(k,"enroll_req")?&request:!strcmp(k,"enroll_bh")?&reqbh:!strcmp(k,"enroll_bi")?&reqbi:!strcmp(k,"web_remember")?&remember:!strcmp(k,"web_code")?&code:&pending;if(!*v)return ESP_ERR_NVS_NOT_FOUND;*v=0;return 0;}
int nvs_commit(int h){(void)h;return fault();}
static void setup(void){journal=(identity_transition_t){.version=1,.callsign="BI4TES"};memset(journal.seed,42,32);pending=cert=backup=bh=bi=sbh=sbi=profile=request=reqbh=reqbi=remember=code=1;for(int i=0;i<8;i++)extra_cert[i]=extra_seed[i]=1;op=0;memset(saved_seed,7,32);strcpy(saved_call,"BH1JSS");}
int main(void){
 for(int failure=1;failure<=33;failure++){setup();fail_at=failure;assert(identity_apply_pending(0)!=0);fail_at=op=0;assert(identity_apply_pending(0)==0);assert(!pending&&!cert&&!backup&&!bh&&!bi&&!sbh&&!sbi&&!profile&&!request&&!reqbh&&!reqbi&&!remember&&!code);assert(!strcmp(saved_call,"BI4TES"));for(int i=0;i<32;i++)assert(saved_seed[i]==42);assert(wifi==77);for(int i=0;i<8;i++)assert(!extra_cert[i]&&!extra_seed[i]);}
 setup();journal.version=8;fail_at=0;assert(identity_apply_pending(0)==ESP_ERR_INVALID_ARG);assert(pending&&cert&&backup&&request&&op==0);
 pending=0;assert(identity_apply_pending(0)==0);assert(op==0);
 puts("PASS identity replay across 33 storage failures, invalid journal fails closed, Wi-Fi untouched");
}
''')
 cmd=['cc','-Wall','-Wextra','-Werror','-I'+td,'-I'+str(root/'ui-preview-firmware/main'),'-I'+str(root/'ui-preview-firmware/components/monocypher'),'-I'+str(Path(os.environ['IDF_PATH'])/'components/json/cJSON'),str(p/'test.c'),'-o',str(p/'test')]
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
