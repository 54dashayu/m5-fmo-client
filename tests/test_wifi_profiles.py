"""Exercise production Wi-Fi profile storage against an in-memory NVS stub."""
from pathlib import Path
import tempfile,subprocess
source=(Path(__file__).resolve().parents[1]/'ui-preview-firmware/main/device_web.c').read_text()
code=source[source.index('#define WIFI_SLOTS'):source.index('static esp_err_t network_start(void);')]
head='''#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_NVS_NOT_FOUND -3
#define WIFI_AUTH_WPA2_PSK 3
#define WIFI_AUTH_OPEN 0
#define WIFI_ALL_CHANNEL_SCAN 1
#define WIFI_CONNECT_AP_BY_SIGNAL 0
typedef struct {struct {uint8_t ssid[32],password[64];struct {int authmode;} threshold;struct {bool capable;} pmf_cfg;int failure_retry_cnt,scan_method,sort_method;} sta;} wifi_config_t;
static wifi_config_t saved;static bool have_saved;static int nv;
static unsigned char pending[2048],stored[2048];static size_t pending_n,stored_n;static bool commit_fail;
static void crypto_wipe(void*p,size_t n){memset(p,0,n);}
static int nvs_set_blob(int h,const char*k,const void*p,size_t n){(void)h;(void)k;memcpy(pending,p,n);pending_n=n;return ESP_OK;}
static int nvs_commit(int h){(void)h;if(commit_fail)return ESP_FAIL;memcpy(stored,pending,pending_n);stored_n=pending_n;return ESP_OK;}
static int nvs_get_blob(int h,const char*k,void*p,size_t*n){(void)h;(void)k;if(!stored_n)return ESP_ERR_NVS_NOT_FOUND;if(*n<stored_n)return ESP_FAIL;memcpy(p,stored,stored_n);*n=stored_n;return ESP_OK;}
'''
tail='''
int main(void){
 memcpy(saved.sta.ssid,"legacy",6);memcpy(saved.sta.password,"test-only",9);have_saved=true;
 assert(profiles_load()==ESP_OK);assert(wifi_bank.preferred==0);assert(!strcmp(wifi_bank.items[0].ssid,"legacy"));
 wifi_config_t c={0},readback;
 for(int i=1;i<6;i++){snprintf((char*)c.sta.ssid,32,"test-%d",i);assert(profile_save(&c,false)==ESP_OK);}
 strcpy((char*)c.sta.ssid,"overflow");assert(profile_save(&c,false)==ESP_ERR_NO_MEM);
 strcpy((char*)c.sta.ssid,"test-2");strcpy((char*)c.sta.password,"changed-test");assert(profile_save(&c,true)==ESP_OK);assert(wifi_bank.preferred==2);
 assert(profile_get(2,&readback));assert(!strcmp((char*)readback.sta.password,"changed-test"));
 assert(!profile_get(-1,&readback));assert(!profile_get(6,&readback));
 commit_fail=true;strcpy((char*)c.sta.password,"failure-test");assert(profile_save(&c,true)==ESP_FAIL);assert(!strcmp(wifi_bank.items[2].password,"changed-test"));commit_fail=false;
 memset(&wifi_bank,0,sizeof(wifi_bank));assert(profiles_load()==ESP_OK);assert(wifi_bank.preferred==2);assert(!strcmp(wifi_bank.items[2].password,"changed-test"));
 // An exact 32-byte SSID must remain terminated in storage and reconstruct safely.
 memset(c.sta.ssid,'x',32);wifi_bank.items[5].ssid[0]=0;assert(profile_save(&c,false)==ESP_OK);assert(strlen(wifi_bank.items[5].ssid)==32);assert(profile_get(5,&readback));assert(!memcmp(readback.sta.ssid,c.sta.ssid,32));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(head+code+tail)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS legacy migration, six profiles, duplicate update, full capacity, commit failure, reboot reload, invalid index and 32-byte SSID')
