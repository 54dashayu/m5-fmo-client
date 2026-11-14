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
tick=source[source.index('static int64_t wifi_retry_at;'):source.index('static void worker(void*a)')]
head += """
#define WIFI_IF_STA 0
#define pdTRUE 1
#define ESP_OK 0
#define pdMS_TO_TICKS(x) (x)
static int api_lock,events;static bool network_started=true,scanning;static unsigned flags;static int64_t clock_us;static int connects,disconnects;static wifi_config_t configured;
#define atomic_load(p) (*(p))
static int64_t esp_timer_get_time(void){return clock_us;}
static int xSemaphoreTake(int l,int t){(void)l;(void)t;return pdTRUE;}
static void xSemaphoreGive(int l){(void)l;}
static unsigned xEventGroupGetBits(int e){(void)e;return flags;}
static void xEventGroupSetBits(int e,unsigned b){(void)e;flags|=b;}
static void xEventGroupClearBits(int e,unsigned b){(void)e;flags&=~b;}
static void vTaskDelay(int t){clock_us+=t*1000;}
static int esp_wifi_get_config(int i,wifi_config_t*c){(void)i;*c=configured;return ESP_OK;}
static int esp_wifi_set_config(int i,const wifi_config_t*c){(void)i;configured=*c;return ESP_OK;}
static int esp_wifi_connect(void){connects++;return ESP_OK;}
static int esp_wifi_disconnect(void){disconnects++;return ESP_OK;}
static void netmsg(const char*m){(void)m;}
"""
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
 // Boot preference, unavailable-network rotation, sparse slots, wrap and no mutation.
 assert(profile_next((const uint8_t*)"",&readback,false)==2);
 assert(profile_next((const uint8_t*)"test-2",&readback,false)==2);
 assert(profile_next((const uint8_t*)"test-2",&readback,true)==3);
 assert(profile_next((const uint8_t*)"test-5",&readback,true)==0);
 assert(wifi_bank.preferred==2);
 wifi_bank.items[3].ssid[0]=0;wifi_bank.items[4].ssid[0]=0;
 assert(profile_next((const uint8_t*)"test-2",&readback,true)==5);
 strcpy(wifi_bank.items[3].ssid,"test-3");strcpy(wifi_bank.items[4].ssid,"test-4");
 // An exact 32-byte SSID must remain terminated in storage and reconstruct safely.
 memset(c.sta.ssid,'x',32);wifi_bank.items[5].ssid[0]=0;assert(profile_save(&c,false)==ESP_OK);assert(strlen(wifi_bank.items[5].ssid)==32);assert(profile_get(5,&readback));assert(!memcmp(readback.sta.ssid,c.sta.ssid,32));
 // An unavailable preferred network must not starve other saved networks.
 flags=0;clock_us=0;wifi_retry_at=0;wifi_auto_tick();assert(connects==1);assert(!strcmp((char*)configured.sta.ssid,"test-2"));
 clock_us+=21000000;wifi_auto_tick();assert(connects==1);
 clock_us+=1000000;wifi_auto_tick();assert(connects==2);assert(!strcmp((char*)configured.sta.ssid,"test-3"));
 flags=1;clock_us+=30000000;wifi_auto_tick();assert(connects==2);assert(wifi_retry_at==0);
 flags=0;wifi_auto_tick();assert(connects==3);assert(!strcmp((char*)configured.sta.ssid,"test-3"));
 flags=2;clock_us+=30000000;wifi_auto_tick();assert(connects==3);
 flags=0;scanning=true;wifi_auto_tick();assert(connects==3);scanning=false;
 assert(disconnects==connects);assert(wifi_bank.preferred==2);
 memset(&wifi_bank,0,sizeof(wifi_bank));wifi_bank.preferred=-1;assert(profile_next((const uint8_t*)"",&readback,true)==-1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(head+code+tick+tail)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS legacy migration, six profiles, duplicate update, full capacity, commit failure, reboot reload, invalid index and 32-byte SSID')
