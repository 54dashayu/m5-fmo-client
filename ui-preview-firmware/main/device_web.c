#include "ui_diagnostics.h"
#include "resource_metrics.h"
#include "sstv_store.h"
#ifdef FMO_FULL_SSTV
#include "sstv_codec.h"
#endif
#include "sstv_rx.h"
#include <stdbool.h>
#include "board.h"
#include "auto_ca_policy.h"
#ifdef CONFIG_FMO_BOARD_XINGZHI
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#endif
extern unsigned ui_backlight_percent,ui_idle_seconds,ui_pressed_mask,ui_call_rgb,ui_call_opacity;
extern unsigned ui_key_layout;extern bool ui_screen_sleeping;extern bool ui_flashing_enabled;
#include "driver/ledc.h"
#include <stdatomic.h>
enum { LINK_IDLE, LINK_CONNECTING, LINK_READY, LINK_FAILED };
static atomic_int mqtt_phase=LINK_IDLE;
#include "device_web.h"
#include "voice.h"
#include "voice_wire.h"
#include "battery.h"
#include "side_leds.h"
#include "esp_heap_caps.h"
#include "relays.h"
#include "discovery.h"
#include "driver/uart.h"
#include "mqtt_client.h"
#include "mbedtls/sha256.h"
#include "cert_wire.h"
#include "identity_transition.h"
#include "monocypher-ed25519.h"
#include "monocypher.h"
#include "esp_wifi.h"
#include "esp_check.h"
#include "esp_system.h"
#include "bootloader_random.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
static nvs_handle_t nv;
static uint8_t sk[64],pk[32];
static char remembered[4][44];
static char token[44],callsign[8]="",apname[33],code[17];
static char *bundle;
static char enrollment[120]="尚未提交在线申请";
static void enrollment_tick(void);
static void profile_switch_tick(void);
static atomic_int ui_profile_request=0;
static atomic_uint auto_ca_mask;
static atomic_int auto_relay_pending;
static relay_t auto_relay_target;
static bool auto_connection_blocked;
static int64_t auto_message_until;
static void auto_relay_tick(void);
static void auto_resume_relay(void);
static void auto_profiles_describe(cJSON*out);
static bool shallow_json(const char*s);
static bool certificate_valid;
static device_view_t view;
static SemaphoreHandle_t lock, api_lock, mqtt_lock;
static esp_mqtt_client_handle_t active_mqtt;
typedef struct {httpd_req_t *http;cJSON *input,*output;bool usb;} request_t;
static esp_err_t auto_profiles_save(request_t*r,const cJSON*q);
static EventGroupHandle_t events;
static QueueHandle_t jobs;
static wifi_config_t saved;
static bool have_saved=false;
#define WIFI_SLOTS 6
typedef struct {char ssid[33],password[64];} wifi_profile_t;
typedef struct {uint32_t magic;int32_t preferred;wifi_profile_t items[WIFI_SLOTS];} wifi_bank_t;
static wifi_bank_t wifi_bank={.magic=0x57494631,.preferred=-1};
static void profile_config(const wifi_profile_t*p,wifi_config_t*c){memset(c,0,sizeof(*c));memcpy(c->sta.ssid,p->ssid,strlen(p->ssid));memcpy(c->sta.password,p->password,strlen(p->password));c->sta.threshold.authmode=p->password[0]?WIFI_AUTH_WPA2_PSK:WIFI_AUTH_OPEN;c->sta.pmf_cfg.capable=true;c->sta.failure_retry_cnt=2;c->sta.scan_method=WIFI_ALL_CHANNEL_SCAN;c->sta.sort_method=WIFI_CONNECT_AP_BY_SIGNAL;}
static bool profile_get(int index,wifi_config_t*c){if(index<0||index>=WIFI_SLOTS||!wifi_bank.items[index].ssid[0])return false;profile_config(&wifi_bank.items[index],c);return true;}
static int profile_find(const uint8_t*ssid){for(int i=0;i<WIFI_SLOTS;i++)if(wifi_bank.items[i].ssid[0]&&!strncmp(wifi_bank.items[i].ssid,(const char*)ssid,32))return i;return -1;}
static esp_err_t profile_save(const wifi_config_t*c,bool preferred){wifi_bank_t next=wifi_bank;int index=profile_find(c->sta.ssid);if(index<0)for(int i=0;i<WIFI_SLOTS;i++)if(!next.items[i].ssid[0]){index=i;break;}if(index<0)return ESP_ERR_NO_MEM;snprintf(next.items[index].ssid,33,"%.*s",32,c->sta.ssid);snprintf(next.items[index].password,64,"%.*s",63,c->sta.password);if(preferred||next.preferred<0)next.preferred=index;esp_err_t e=nvs_set_blob(nv,"wifi_profiles",&next,sizeof(next));if(e==ESP_OK)e=nvs_commit(nv);if(e==ESP_OK)wifi_bank=next;crypto_wipe(&next,sizeof(next));return e;}
static esp_err_t profiles_load(void){size_t n=sizeof(wifi_bank);wifi_bank_t loaded={0};esp_err_t e=nvs_get_blob(nv,"wifi_profiles",&loaded,&n);if(e==ESP_OK){if(n!=sizeof(loaded)||loaded.magic!=0x57494631||loaded.preferred< -1||loaded.preferred>=WIFI_SLOTS)return ESP_FAIL;for(int i=0;i<WIFI_SLOTS;i++)if(!memchr(loaded.items[i].ssid,0,33)||!memchr(loaded.items[i].password,0,64))return ESP_FAIL;wifi_bank=loaded;crypto_wipe(&loaded,sizeof(loaded));}else if(e==ESP_ERR_NVS_NOT_FOUND){if(have_saved)return profile_save(&saved,true);}else return e;return ESP_OK;}
// Called with api_lock held. Empty/deleted slots never block failover.
static int profile_next(const uint8_t*current,wifi_config_t*c,bool advance){
 int at=profile_find(current);
 if(!advance&&at>=0&&profile_get(at,c))return at;
 if(at<0&&profile_get(wifi_bank.preferred,c))return wifi_bank.preferred;
 for(int n=1;n<=WIFI_SLOTS;n++){int i=(at+n)%WIFI_SLOTS;if(profile_get(i,c))return i;}
 return -1;
}
static esp_err_t network_start(void);
static bool network_started,network_attempted,time_initialized;
static atomic_bool management_requested=false,scanning=false;
static bool ap_enabled;
void device_management_ap(bool enabled){atomic_store(&management_requested,enabled);}
static void update_ap(void){if(voice_busy())return;bool wanted=atomic_load(&management_requested)||!(xEventGroupGetBits(events)&1);if(wanted!=ap_enabled&&esp_wifi_set_mode(wanted?WIFI_MODE_APSTA:WIFI_MODE_STA)==ESP_OK){ap_enabled=wanted;xSemaphoreTake(lock,portMAX_DELAY);view.ap_active=wanted;xSemaphoreGive(lock);}}
static const char *TAG="device_web";
extern const uint8_t web_start[] asm("_binary_web_html_start");
extern const uint8_t web_end[] asm("_binary_web_html_end");
static void netmsg(const char *s){xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.network,sizeof(view.network),"%s",s);xSemaphoreGive(lock);}
void device_web_view(device_view_t *v){xSemaphoreTake(lock,portMAX_DELAY);*v=view;xSemaphoreGive(lock);}
// A certificate bundle is immutable between imports. Verify signatures once per
// digest/callsign, then enforce its validity window on every status/worker pass.
// Avoid allocating/parsing the full chain during each received audio packet.
static uint8_t selected_ca;
static char selected_ca_name[97];
static void identity_state(void){
 static bool cached=false;static uint8_t digest[32];static char cached_call[12];
 static int64_t begins,expires;
 const char *message=selected_ca?"当前体系待申请或领取证书":"尚未导入证书";certificate_valid=false;
 if(bundle){
  uint8_t current[32];bool hashed=mbedtls_sha256((const uint8_t*)bundle,strlen(bundle),current,0)==0;
  if(!hashed||!cached||memcmp(current,digest,32)||strcmp(cached_call,callsign)){
   cached=false;cJSON*j=cJSON_Parse(bundle);
   const char *error=wire_validate_bundle(j,pk,callsign,0);
   if(!error&&hashed){
    cJSON*u=cJSON_GetObjectItemCaseSensitive(j,"userCert");
    begins=(int64_t)cJSON_GetObjectItemCaseSensitive(u,"iat")->valuedouble;
    expires=(int64_t)cJSON_GetObjectItemCaseSensitive(u,"exp")->valuedouble;
    memcpy(digest,current,32);snprintf(cached_call,sizeof(cached_call),"%s",callsign);cached=true;
   }
   cJSON_Delete(j);
  }
  int64_t now=time(NULL);certificate_valid=cached&&now>1700000000&&now>=begins&&now<expires;
  message=!cached?"证书无效，请重新导入":now<=1700000000?"证书已保存，等待网络校时":certificate_valid?"证书有效，可以接入测试台":"证书未生效或已经过期";
 }else cached=false;
 xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.identity,sizeof(view.identity),"%s",message);xSemaphoreGive(lock);
}

static void wifi_event(void*a,esp_event_base_t b,int32_t id,void*d){(void)a;if(b==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){ip_event_got_ip_t*e=d;xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.ip,sizeof(view.ip),IPSTR,IP2STR(&e->ip_info.ip));view.online=true;wifi_ap_record_t ap;if(esp_wifi_sta_get_ap_info(&ap)==ESP_OK)snprintf(view.ssid,sizeof(view.ssid),"%.*s",32,ap.ssid);snprintf(view.network,sizeof(view.network),"Wi-Fi 已连接");xSemaphoreGive(lock);xEventGroupSetBits(events,1);if(!time_initialized){esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"ntp.aliyun.com");esp_sntp_setservername(1,"pool.ntp.org");esp_sntp_init();time_initialized=true;}else esp_sntp_restart();ESP_LOGI(TAG,"STA ready: " IPSTR,IP2STR(&e->ip_info.ip));}else if(b==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){wifi_event_sta_disconnected_t*e=d;ESP_LOGW(TAG,"Wi-Fi disconnected: reason=%u RSSI=%d",e->reason,e->rssi);xSemaphoreTake(lock,portMAX_DELAY);view.online=false;view.ip[0]=0;snprintf(view.network,sizeof(view.network),"Wi-Fi 断开，正在重连；可连接设备热点");view.wifi_reason=e->reason;view.wifi_rssi=e->rssi;xSemaphoreGive(lock);xEventGroupClearBits(events,1);}}
// Try the current/preferred network first, then rotate after a bounded timeout.
// Never rewrite credentials/preference during automatic recovery.
static int64_t wifi_retry_at;
static void wifi_auto_tick(void){
 if(!network_started)return;
 int64_t now=esp_timer_get_time();
 if(xSemaphoreTake(api_lock,0)!=pdTRUE)return;
 unsigned bits=xEventGroupGetBits(events);
 if((bits&1)||(bits&2)||atomic_load(&scanning)){wifi_retry_at=0;xSemaphoreGive(api_lock);return;}
 if(wifi_retry_at&&now<wifi_retry_at){xSemaphoreGive(api_lock);return;}
 wifi_config_t current={0},next={0};esp_wifi_get_config(WIFI_IF_STA,&current);
 int index=profile_next(current.sta.ssid,&next,wifi_retry_at!=0);
 crypto_wipe(&current,sizeof(current));
 if(index<0){wifi_retry_at=0;xSemaphoreGive(api_lock);return;}
 xEventGroupSetBits(events,2);xSemaphoreGive(api_lock);
 esp_wifi_disconnect();vTaskDelay(pdMS_TO_TICKS(200));xEventGroupClearBits(events,1);
 esp_err_t result=esp_wifi_set_config(WIFI_IF_STA,&next);
 if(result==ESP_OK)result=esp_wifi_connect();
 crypto_wipe(&next,sizeof(next));
 wifi_retry_at=esp_timer_get_time()+22000000;
 netmsg(result==ESP_OK?"正在尝试已保存的 Wi-Fi…":"Wi-Fi 暂未连接，将尝试其他保存网络");
 xEventGroupClearBits(events,2);
}
static void worker(void*a){(void)a;wifi_config_t candidate;network_attempted=true;if(network_start()==ESP_OK)network_started=true;else netmsg("Wi-Fi 启动失败，请重启设备");while(1){if(network_started){update_ap();auto_relay_tick();profile_switch_tick();enrollment_tick();if(!(xEventGroupGetBits(events)&2))discovery_tick();}if(xQueueReceive(jobs,&candidate,pdMS_TO_TICKS(1000))){wifi_retry_at=0;netmsg("正在连接新网络，请稍候…");if(!network_started){if(network_attempted){netmsg("请重启设备后重试联网，USB 管理仍可用");crypto_wipe(&candidate,sizeof(candidate));xEventGroupClearBits(events,2);continue;}network_attempted=true;uint8_t failures=0;nvs_get_u8(nv,"net_failures",&failures);if(failures<255)failures++;nvs_set_u8(nv,"net_failures",failures);nvs_commit(nv);vTaskDelay(pdMS_TO_TICKS(400));esp_err_t start=network_start();if(start!=ESP_OK){netmsg("Wi-Fi 启动失败，USB 管理仍可用");crypto_wipe(&candidate,sizeof(candidate));xEventGroupClearBits(events,2);continue;}network_started=true;nvs_set_u8(nv,"net_failures",0);nvs_set_u8(nv,"net_pending",0);nvs_commit(nv);}esp_wifi_disconnect();vTaskDelay(pdMS_TO_TICKS(200));xEventGroupClearBits(events,1);esp_wifi_set_config(WIFI_IF_STA,&candidate);esp_err_t r=esp_wifi_connect();bool ok=r==ESP_OK&&(xEventGroupWaitBits(events,1,pdFALSE,pdTRUE,pdMS_TO_TICKS(22000))&1);if(ok){xSemaphoreTake(api_lock,portMAX_DELAY);r=profile_save(&candidate,true);if(r==ESP_OK)r=nvs_set_blob(nv,"wifi",&candidate,sizeof(candidate));if(r==ESP_OK)r=nvs_commit(nv);if(r==ESP_OK){nvs_set_u8(nv,"net_pending",0);nvs_commit(nv);saved=candidate;have_saved=true;xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.ssid,sizeof(view.ssid),"%.*s",32,candidate.sta.ssid);xSemaphoreGive(lock);netmsg("连接成功，Wi-Fi 已保存");}else netmsg("已连接，但保存失败；原设置保留");xSemaphoreGive(api_lock);}else{netmsg("连接暂未成功，将尝试其他已保存网络");esp_wifi_disconnect();wifi_retry_at=esp_timer_get_time();}crypto_wipe(&candidate,sizeof(candidate));xEventGroupClearBits(events,2);}else wifi_auto_tick();}}
static esp_err_t response(request_t*q,cJSON*j){if(q->usb){q->output=j;return ESP_OK;}httpd_req_t*r=q->http;char*s=cJSON_PrintBuffered(j,256,false);cJSON_Delete(j);if(!s)return httpd_resp_send_err(r,HTTPD_500_INTERNAL_SERVER_ERROR,"memory");httpd_resp_set_type(r,"application/json; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Content-Type-Options","nosniff");esp_err_t e=httpd_resp_sendstr(r,s);free(s);return e;}
static esp_err_t fail(request_t*r,const char*message){if(r->http)httpd_resp_set_status(r->http,"400 Bad Request");cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",false);cJSON_AddStringToObject(j,"error",message);return response(r,j);}
static esp_err_t success(request_t*r,const char*message){cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"ok",true);cJSON_AddStringToObject(j,"message",message);return response(r,j);}
static int remembered_index(const char*h){for(int i=0;i<4;i++)if(remembered[i][0]&&strlen(h)==43&&!memcmp(h,remembered[i],43))return i;return -1;}
static bool auth(request_t*q){if(q->usb)return true;httpd_req_t*r=q->http;char h[48]={0};if(httpd_req_get_hdr_value_len(r,"X-Device-Token")!=43||httpd_req_get_hdr_value_str(r,"X-Device-Token",h,sizeof(h))!=ESP_OK||(memcmp(h,token,43)&&remembered_index(h)<0)){httpd_resp_send_err(r,HTTPD_401_UNAUTHORIZED,"Unlock with the code on the device");return false;}return true;}
static esp_err_t logout_page(request_t*r){if(!auth(r))return ESP_OK;char h[48]={0};if(r->http)httpd_req_get_hdr_value_str(r->http,"X-Device-Token",h,sizeof(h));int i=remembered_index(h);if(i>=0){char backup[44];memcpy(backup,remembered[i],44);memset(remembered[i],0,44);esp_err_t e=nvs_set_blob(nv,"web_remember",remembered,sizeof(remembered));if(e==ESP_OK)e=nvs_commit(nv);if(e!=ESP_OK){memcpy(remembered[i],backup,44);return fail(r,"撤销授权失败，请重试");}}return success(r,"已退出并取消此浏览器的记住登录");}
static cJSON *body(request_t*q){if(q->usb)return cJSON_Duplicate(q->input,true);httpd_req_t*r=q->http;if(r->content_len<2||r->content_len>8192)return NULL;char*s=calloc(1,r->content_len+1);if(!s)return NULL;int n=0;int64_t deadline=esp_timer_get_time()+6000000;while(n<r->content_len){int k=httpd_req_recv(r,s+n,r->content_len-n);if(k<=0||esp_timer_get_time()>deadline){crypto_wipe(s,r->content_len);free(s);return NULL;}n+=k;}
 bool quoted=false,escaped=false;int depth=0;for(int x=0;x<n;x++){char c=s[x];if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}else if(c=='"')quoted=true;else if(c=='{'||c=='['){if(++depth>12){crypto_wipe(s,n);free(s);return NULL;}}else if(c=='}'||c==']')depth--;}
 const char*end=NULL;cJSON*j=cJSON_ParseWithLengthOpts(s,n+1,&end,true);crypto_wipe(s,n);free(s);return j;}
static const char *js(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
#include "root_profiles.inc"
#include "enrollment_jobs.inc"
static bool identity_restart_pending;
#include "empty_profile.inc"
static esp_err_t index_page(httpd_req_t*r){httpd_resp_set_type(r,"text/html; charset=utf-8");httpd_resp_set_hdr(r,"Cache-Control","no-store");httpd_resp_set_hdr(r,"X-Frame-Options","DENY");httpd_resp_set_hdr(r,"Content-Security-Policy","default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; connect-src 'self' https://fmo.bh1jss.net https://fmo.simy.live https://space.bh1jss.net; img-src 'self' data:; frame-ancestors 'none'; base-uri 'none'");size_t len=web_end-web_start-1;for(size_t off=0;off<len;off+=768){size_t n=len-off;if(n>768)n=768;esp_err_t e=httpd_resp_send_chunk(r,(const char*)web_start+off,n);if(e!=ESP_OK)return e;}return httpd_resp_send_chunk(r,NULL,0);}
static esp_err_t login_page(request_t*r){static int64_t next=0;int64_t now=esp_timer_get_time();if(now<next){if(r->http)httpd_resp_set_status(r->http,"429 Too Many Requests");return fail(r,"请稍后重试");}cJSON*j=body(r);const char*p=js(j,"code");bool keep=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"remember"));bool ok=r->usb||(p&&strlen(p)==strlen(code)&&!memcmp(p,code,strlen(code)));cJSON_Delete(j);if(!ok){next=now+2000000;return fail(r,"管理码不正确，请查看 M5 屏幕");}char issued[44];memcpy(issued,token,44);if(keep){char backup[sizeof(remembered)];memcpy(backup,remembered,sizeof(remembered));uint8_t random[32];esp_fill_random(random,sizeof(random));wire_b64(random,32,issued);crypto_wipe(random,sizeof(random));memmove(remembered+1,remembered,3*44);memcpy(remembered[0],issued,44);esp_err_t e=nvs_set_blob(nv,"web_remember",remembered,sizeof(remembered));if(e==ESP_OK)e=nvs_commit(nv);if(e!=ESP_OK){memcpy(remembered,backup,sizeof(remembered));crypto_wipe(backup,sizeof(backup));return fail(r,"保存浏览器授权失败，请重试");}crypto_wipe(backup,sizeof(backup));}cJSON*out=cJSON_CreateObject();cJSON_AddStringToObject(out,"token",issued);return response(r,out);}
extern unsigned ui_key_events;extern int ui_last_key,ui_last_action;
// The browser polls only presentation fields; full diagnostics remain on USB.
static void board_configuration(cJSON*j){
 (void)j;
#ifdef CONFIG_FMO_BOARD_XINGZHI
 cJSON_AddStringToObject(j,"boardProfile",FMO_PROFILE_HASH);cJSON_AddStringToObject(j,"screenModel",FMO_SCREEN_MODEL);cJSON_AddNumberToObject(j,"screenWidth",FMO_WIDTH);cJSON_AddNumberToObject(j,"screenHeight",FMO_HEIGHT);
 cJSON_AddBoolToObject(j,"powerKeyReadable",false);
#endif
}
static void resource_status(cJSON *j){
#ifdef FMO_FULL_SSTV
 resource_metrics_t m;resource_metrics_read(&m);cJSON *r=cJSON_AddObjectToObject(j,"resources");
 cJSON_AddBoolToObject(r,"cpuValid",m.valid);
 cJSON_AddNumberToObject(r,"cpuPercent",m.average);cJSON_AddNumberToObject(r,"cpu0Percent",m.cpu[0]);cJSON_AddNumberToObject(r,"cpu1Percent",m.cpu[1]);
 cJSON_AddNumberToObject(r,"windowMs",m.window_ms);
 cJSON_AddNumberToObject(r,"internalTotal",m.internal_total);cJSON_AddNumberToObject(r,"internalFree",m.internal_free);cJSON_AddNumberToObject(r,"internalLargest",m.internal_largest);
 cJSON_AddNumberToObject(r,"psramTotal",m.psram_total);cJSON_AddNumberToObject(r,"psramFree",m.psram_free);
#endif
}
static esp_err_t browser_status(request_t*r){
 device_view_t v;device_web_view(&v);voice_view_t a;voice_view(&a);relay_t relay;relays_current(&relay,NULL);
 cJSON*j=cJSON_CreateObject();char key[44],display[12];wire_b64(pk,32,key);snprintf(display,sizeof(display),FMO_CALL_PREFIX "%s",callsign);
 cJSON_AddStringToObject(j,"enrollment",enrollment);enroll_status(j);uint8_t relay_notice=0;nvs_get_u8(nv,"relay_notice",&relay_notice);cJSON_AddBoolToObject(j,"relayRefreshSuggested",relay_notice!=0);
 cJSON_AddStringToObject(j,"version",FMO_VERSION);board_configuration(j);resource_status(j);cJSON_AddStringToObject(j,"profileMessage",v.profile_message);cJSON_AddNumberToObject(j,"autoCaMask",atomic_load(&auto_ca_mask));cJSON_AddBoolToObject(j,"autoCaPending",atomic_load(&auto_relay_pending)!=0||identity_restart_pending);cJSON_AddNumberToObject(j,"keyLayout",ui_key_layout);cJSON_AddBoolToObject(j,"screenSleeping",ui_screen_sleeping);cJSON_AddBoolToObject(j,"screenFlashingEnabled",ui_flashing_enabled);cJSON_AddNumberToObject(j,"callColor",ui_call_rgb);cJSON_AddNumberToObject(j,"callOpacity",ui_call_opacity);cJSON_AddStringToObject(j,"board",FMO_BOARD_ID);
 cJSON_AddStringToObject(j,"callsign",callsign);
 cJSON_AddStringToObject(j,"displayName",display);
 cJSON_AddStringToObject(j,"publicKey",key);
 cJSON_AddStringToObject(j,"identity",v.identity);
 cJSON_AddStringToObject(j,"relay",v.relay);
 cJSON_AddStringToObject(j,"relayName",relay.name);
 cJSON_AddStringToObject(j,"ssid",v.ssid);
 cJSON_AddStringToObject(j,"ip",v.ip);
 cJSON_AddStringToObject(j,"network",v.network);
 cJSON_AddStringToObject(j,"caller",a.caller);
 cJSON_AddBoolToObject(j,"online",v.online);
 cJSON_AddBoolToObject(j,"mqttOnline",v.mqtt_online);
 cJSON_AddBoolToObject(j,"clockValid",time(NULL)>1700000000);
 cJSON_AddBoolToObject(j,"hasCertificate",bundle!=NULL);size_t bank_len=0;cJSON_AddBoolToObject(j,"hasBi4tesCertificate",nvs_get_str(nv,"cert_bi",NULL,&bank_len)==ESP_OK);cJSON_AddBoolToObject(j,"hasBh1jssCertificate",(bundle&&strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c"))||nvs_get_str(nv,"cert_bh",NULL,&bank_len)==ESP_OK);cJSON_AddStringToObject(j,"certificateProvider",!bundle?"none":strstr(bundle,"QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc")?"bi4tes":strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c")?"bh1jss":"other");
 cJSON_AddBoolToObject(j,"certificateValid",certificate_valid);
 cJSON_AddBoolToObject(j,"wifiConnecting",(xEventGroupGetBits(events)&2)!=0);
 cJSON_AddBoolToObject(j,"audioReady",a.ready);
 cJSON_AddBoolToObject(j,"tx",a.tx);
 cJSON_AddBoolToObject(j,"rx",a.rx);
 size_t len=0;cJSON_AddBoolToObject(j,"hasCertificateBackup",(nvs_get_str(nv,"cert_bh",NULL,&len)==ESP_OK||nvs_get_str(nv,"cert_backup",NULL,&len)==ESP_OK));
 wifi_ap_record_t ap;if(v.online&&esp_wifi_sta_get_ap_info(&ap)==ESP_OK)cJSON_AddNumberToObject(j,"currentRssi",ap.rssi);else cJSON_AddNullToObject(j,"currentRssi");
 cJSON_AddNumberToObject(j,"rxPackets",a.rx_packets);cJSON_AddNumberToObject(j,"txPackets",a.tx_packets);return response(r,j);
}
static esp_err_t status_page(request_t*r){if(!auth(r))return ESP_OK;identity_state();
#ifdef FMO_SSTV_STREAMING
 if(r->usb&&heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)<20000)return browser_status(r);
#endif
 if(!r->usb){return browser_status(r);}device_view_t v;device_web_view(&v);cJSON*j=cJSON_CreateObject();char key[44];wire_b64(pk,32,key);cJSON_AddStringToObject(j,"enrollment",enrollment);enroll_status(j);uint8_t relay_notice=0;nvs_get_u8(nv,"relay_notice",&relay_notice);cJSON_AddBoolToObject(j,"relayRefreshSuggested",relay_notice!=0);cJSON_AddStringToObject(j,"version",FMO_VERSION);board_configuration(j);resource_status(j);cJSON_AddStringToObject(j,"profileMessage",v.profile_message);cJSON_AddNumberToObject(j,"autoCaMask",atomic_load(&auto_ca_mask));cJSON_AddBoolToObject(j,"autoCaPending",atomic_load(&auto_relay_pending)!=0||identity_restart_pending);cJSON_AddNumberToObject(j,"keyLayout",ui_key_layout);cJSON_AddBoolToObject(j,"screenSleeping",ui_screen_sleeping);cJSON_AddBoolToObject(j,"screenFlashingEnabled",ui_flashing_enabled);cJSON_AddNumberToObject(j,"callColor",ui_call_rgb);cJSON_AddNumberToObject(j,"callOpacity",ui_call_opacity);cJSON_AddStringToObject(j,"board",FMO_BOARD_ID);int battery_percent;bool charging;battery_view(&battery_percent,&charging);cJSON_AddBoolToObject(j,"batteryAvailable",battery_percent>=0);if(battery_percent>=0){cJSON_AddNumberToObject(j,"batteryPercent",battery_percent);cJSON_AddBoolToObject(j,"batteryCharging",charging);}else{cJSON_AddNullToObject(j,"batteryPercent");cJSON_AddNullToObject(j,"batteryCharging");}uint8_t wifi_protocol=0;esp_wifi_get_protocol(WIFI_IF_STA,&wifi_protocol);cJSON_AddNumberToObject(j,"wifiProtocol",wifi_protocol);cJSON_AddStringToObject(j,"callsign",callsign);cJSON_AddStringToObject(j,"publicKey",key);cJSON_AddStringToObject(j,"ap",apname);cJSON_AddBoolToObject(j,"apActive",v.ap_active);cJSON_AddStringToObject(j,"apIp","192.168.4.1");cJSON_AddBoolToObject(j,"online",v.online);cJSON_AddNumberToObject(j,"wifiReason",v.wifi_reason);cJSON_AddNumberToObject(j,"wifiRssi",v.wifi_rssi);cJSON_AddBoolToObject(j,"mqttOnline",v.mqtt_online);cJSON_AddNumberToObject(j,"mqttPhase",atomic_load(&mqtt_phase));cJSON_AddStringToObject(j,"relay",v.relay);char display[12];snprintf(display,sizeof(display),FMO_CALL_PREFIX "%s",callsign);cJSON_AddStringToObject(j,"displayName",display);cJSON_AddStringToObject(j,"ip",v.ip);cJSON_AddStringToObject(j,"ssid",v.ssid);cJSON_AddStringToObject(j,"network",v.network);wifi_ap_record_t ap;if(v.online&&esp_wifi_sta_get_ap_info(&ap)==ESP_OK)cJSON_AddNumberToObject(j,"currentRssi",ap.rssi);else cJSON_AddNullToObject(j,"currentRssi");cJSON_AddBoolToObject(j,"wifiConnecting",(xEventGroupGetBits(events)&2)!=0);cJSON_AddStringToObject(j,"identity",v.identity);cJSON_AddBoolToObject(j,"clockValid",time(NULL)>1700000000);cJSON_AddBoolToObject(j,"hasCertificate",bundle!=NULL);size_t bank_len=0;cJSON_AddBoolToObject(j,"hasBi4tesCertificate",nvs_get_str(nv,"cert_bi",NULL,&bank_len)==ESP_OK);cJSON_AddBoolToObject(j,"hasBh1jssCertificate",(bundle&&strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c"))||nvs_get_str(nv,"cert_bh",NULL,&bank_len)==ESP_OK);cJSON_AddStringToObject(j,"certificateProvider",!bundle?"none":strstr(bundle,"QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc")?"bi4tes":strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c")?"bh1jss":"other");size_t backup_len=0;cJSON_AddBoolToObject(j,"hasCertificateBackup",(nvs_get_str(nv,"cert_bh",NULL,&backup_len)==ESP_OK||nvs_get_str(nv,"cert_backup",NULL,&backup_len)==ESP_OK));cJSON_AddBoolToObject(j,"certificateValid",certificate_valid);cJSON_AddNumberToObject(j,"heap",esp_get_free_heap_size());cJSON_AddNumberToObject(j,"largestHeapBlock",heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));int queued=-1;if(xSemaphoreTake(mqtt_lock,0)==pdTRUE){queued=active_mqtt&&v.mqtt_online?esp_mqtt_client_outbox_size_now(active_mqtt):0;xSemaphoreGive(mqtt_lock);}cJSON_AddNumberToObject(j,"mqttOutboxBytes",queued);side_leds_view_t led;side_leds_view(&led);cJSON_AddNumberToObject(j,"backlightPercent",ui_backlight_percent);cJSON_AddNumberToObject(j,"backlightDuty",ledc_get_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0));cJSON_AddNumberToObject(j,"displayIdleSeconds",ui_idle_seconds);cJSON_AddNumberToObject(j,"pressedMask",ui_pressed_mask);cJSON_AddBoolToObject(j,"ledReady",led.ready);cJSON_AddNumberToObject(j,"ledWrites",led.writes);cJSON_AddNumberToObject(j,"ledErrors",led.failures);cJSON_AddNumberToObject(j,"ledErrorCode",led.error);cJSON_AddNumberToObject(j,"ledColor",led.color);cJSON_AddNumberToObject(j,"ledBrightness",led.brightness);cJSON_AddNumberToObject(j,"ledGpioRoute",led.route);cJSON_AddNumberToObject(j,"ledOpenDrain",led.open_drain);voice_view_t audio;voice_view(&audio);cJSON_AddStringToObject(j,"txStopReason",audio.stop_reason);cJSON_AddNumberToObject(j,"keyEvents",ui_key_events);cJSON_AddNumberToObject(j,"lastKey",ui_last_key);cJSON_AddNumberToObject(j,"lastKeyAction",ui_last_action);cJSON_AddNumberToObject(j,"resetReason",esp_reset_reason());cJSON_AddNumberToObject(j,"audioStackFree",voice_stack_free());cJSON_AddNumberToObject(j,"minimumHeap",esp_get_minimum_free_heap_size());cJSON_AddNumberToObject(j,"uptime",esp_timer_get_time()/1000000);cJSON_AddBoolToObject(j,"tx",audio.tx);cJSON_AddBoolToObject(j,"rx",audio.rx);cJSON_AddStringToObject(j,"caller",audio.caller);cJSON_AddStringToObject(j,"lastCaller",audio.last_caller);cJSON_AddStringToObject(j,"previousCaller",audio.previous_caller);cJSON_AddNumberToObject(j,"rxLate",audio.rx_late);cJSON_AddNumberToObject(j,"rxOverflow",audio.rx_overflow);cJSON_AddNumberToObject(j,"rxRejected",audio.rx_rejected);cJSON_AddNumberToObject(j,"rxOversized",audio.rx_oversized);cJSON_AddNumberToObject(j,"rxMaxBytes",audio.rx_max_bytes);cJSON_AddNumberToObject(j,"rxMaxFrames",audio.rx_max_frames);cJSON_AddNumberToObject(j,"rxRebuffers",audio.rx_rebuffers);cJSON_AddNumberToObject(j,"rxUnderflows",audio.rx_underflows);cJSON_AddNumberToObject(j,"micClipped",audio.mic_clipped);cJSON_AddStringToObject(j,"audioNotice",audio.notice);cJSON_AddBoolToObject(j,"audioReady",audio.ready);cJSON_AddNumberToObject(j,"dacRateHz",audio.dac_rate);cJSON_AddNumberToObject(j,"dmaRateHz",audio.dma_rate);cJSON_AddNumberToObject(j,"volume",audio.volume);cJSON_AddBoolToObject(j,"muted",audio.muted);cJSON_AddNumberToObject(j,"micPeak",audio.mic_peak);cJSON_AddNumberToObject(j,"micReadMs",audio.mic_read_ms);cJSON_AddNumberToObject(j,"micRateHz",audio.mic_rate);cJSON_AddNumberToObject(j,"micInitialRateHz",audio.mic_initial_rate);cJSON_AddNumberToObject(j,"micClockRequestHz",audio.mic_clock_request);cJSON_AddNumberToObject(j,"pttStopMs",audio.stop_ms);cJSON_AddNumberToObject(j,"decodedFrames",audio.decoded_frames);cJSON_AddNumberToObject(j,"decodedSamples",audio.decoded_samples);cJSON_AddNumberToObject(j,"decodeErrors",audio.decode_errors);cJSON_AddNumberToObject(j,"decodeUsMax",audio.decode_us_max);cJSON_AddNumberToObject(j,"decodeUsTotal",audio.decode_us_total);cJSON_AddNumberToObject(j,"decodeSlowFrames",audio.decode_slow_frames);cJSON_AddNumberToObject(j,"playGapUsMax",audio.play_gap_us_max);cJSON_AddNumberToObject(j,"playLateFeeds",audio.play_late_feeds);cJSON_AddNumberToObject(j,"pcmSamplesWritten",audio.pcm_samples_written);cJSON_AddNumberToObject(j,"decoderResets",audio.decoder_resets);cJSON_AddNumberToObject(j,"rxDuplicates",audio.rx_duplicates);cJSON_AddNumberToObject(j,"rxSharedTimestamps",audio.rx_shared_timestamps);cJSON_AddNumberToObject(j,"outputErrors",audio.output_errors);cJSON_AddNumberToObject(j,"lastCodec",audio.last_codec);cJSON_AddNumberToObject(j,"speakerPeak",audio.speaker_peak);cJSON_AddNumberToObject(j,"rxPackets",audio.rx_packets);cJSON_AddNumberToObject(j,"txPackets",audio.tx_packets);cJSON_AddNumberToObject(j,"txPending",audio.tx_pending);cJSON_AddNumberToObject(j,"txRetries",audio.tx_retries);cJSON_AddNumberToObject(j,"txDropped",audio.tx_dropped);cJSON_AddNumberToObject(j,"txRogerFrames",audio.tx_roger_frames);cJSON_AddNumberToObject(j,"txCaptureFrames",audio.tx_capture_frames);cJSON_AddNumberToObject(j,"txCaptureSamples",audio.tx_capture_samples);cJSON_AddNumberToObject(j,"txEncodeUsMax",audio.tx_encode_us_max);cJSON_AddNumberToObject(j,"txReadUsMax",audio.tx_read_us_max);cJSON_AddNumberToObject(j,"txAdcOverflows",audio.tx_adc_overflows);cJSON_AddNumberToObject(j,"txDurationMs",audio.tx_duration_ms);cJSON_AddNumberToObject(j,"audioDropped",audio.dropped);relay_t profile;relays_current(&profile,NULL);cJSON_AddStringToObject(j,"relayName",profile.name);cJSON_AddNumberToObject(j,"relayUid",profile.uid);return response(r,j);}
static bool queue_wifi(const wifi_config_t*c){if(xEventGroupGetBits(events)&2)return false;xEventGroupSetBits(events,2);if(xQueueSend(jobs,c,0))return true;xEventGroupClearBits(events,2);return false;}
static esp_err_t scan_page(request_t*r){
 if(!auth(r))return ESP_OK;
 if(!network_started)return fail(r,"Wi-Fi 尚未启动，请稍后重试");
 if(voice_busy())return fail(r,"通联中不能扫描，请结束通联后重试");
 if(xEventGroupGetBits(events)&2)return fail(r,"正在配网，请稍后扫描");
 wifi_scan_config_t cfg={.show_hidden=false,.scan_type=WIFI_SCAN_TYPE_ACTIVE};
 atomic_store(&scanning,true);esp_err_t e=esp_wifi_scan_start(&cfg,true);atomic_store(&scanning,false);if(e!=ESP_OK)return fail(r,"网络扫描暂时不可用，请稍后重试");
 wifi_ap_record_t*aps=calloc(24,sizeof(*aps));if(!aps){esp_wifi_clear_ap_list();return fail(r,"可用内存不足");}
 uint16_t count=24;e=esp_wifi_scan_get_ap_records(&count,aps);if(e!=ESP_OK){free(aps);esp_wifi_clear_ap_list();return fail(r,"扫描结果读取失败");}
 cJSON*j=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(j,"networks");
 for(int i=0;i<count;i++){if(!aps[i].ssid[0]||aps[i].primary>14)continue;bool duplicate=false;for(int k=0;k<i;k++)if(!memcmp(aps[i].ssid,aps[k].ssid,32))duplicate=true;if(duplicate)continue;
 cJSON*n=cJSON_CreateObject();cJSON_AddStringToObject(n,"ssid",(char*)aps[i].ssid);cJSON_AddNumberToObject(n,"rssi",aps[i].rssi);cJSON_AddNumberToObject(n,"channel",aps[i].primary);cJSON_AddBoolToObject(n,"open",aps[i].authmode==WIFI_AUTH_OPEN);bool supported=aps[i].authmode==WIFI_AUTH_OPEN||aps[i].authmode==WIFI_AUTH_WPA2_PSK||aps[i].authmode==WIFI_AUTH_WPA_WPA2_PSK||aps[i].authmode==WIFI_AUTH_WPA3_PSK||aps[i].authmode==WIFI_AUTH_WPA2_WPA3_PSK;cJSON_AddBoolToObject(n,"supported",supported);cJSON_AddBoolToObject(n,"recommended",supported&&aps[i].authmode!=WIFI_AUTH_OPEN&&aps[i].rssi>=-67);cJSON_AddItemToArray(list,n);}
 free(aps);return response(r,j);
}
static esp_err_t networks_page(request_t*r){if(!auth(r))return ESP_OK;cJSON*j=cJSON_CreateObject(),*list=cJSON_AddArrayToObject(j,"networks");device_view_t v;device_web_view(&v);for(int i=0;i<WIFI_SLOTS;i++){wifi_profile_t*p=&wifi_bank.items[i];if(!p->ssid[0])continue;cJSON*n=cJSON_CreateObject();cJSON_AddNumberToObject(n,"index",i);cJSON_AddStringToObject(n,"ssid",p->ssid);cJSON_AddBoolToObject(n,"passwordSet",p->password[0]!=0);cJSON_AddBoolToObject(n,"preferred",i==wifi_bank.preferred);cJSON_AddBoolToObject(n,"connected",v.online&&!strcmp(v.ssid,p->ssid));cJSON_AddItemToArray(list,n);}cJSON_AddNumberToObject(j,"capacity",WIFI_SLOTS);return response(r,j);}
static esp_err_t network_page(request_t*r){if(!auth(r))return ESP_OK;if(xEventGroupGetBits(events)&2)return fail(r,"已有配网任务，请稍候");cJSON*j=body(r);const char*s=js(j,"ssid"),*p=js(j,"password");bool connect=!cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(j,"connect"));if(!s||!strlen(s)||strlen(s)>32||!p||strlen(p)>63||(strlen(p)>0&&strlen(p)<8)){cJSON_Delete(j);return fail(r,"Wi-Fi 名称须为 1–32 字节，密码为空或 8–63 字节");}wifi_profile_t item={0};snprintf(item.ssid,sizeof(item.ssid),"%s",s);snprintf(item.password,sizeof(item.password),"%s",p);wifi_config_t c;profile_config(&item,&c);crypto_wipe(&item,sizeof(item));crypto_wipe((void*)p,strlen(p));cJSON_Delete(j);esp_err_t e=profile_save(&c,false);bool queued=e==ESP_OK&&connect&&queue_wifi(&c);crypto_wipe(&c,sizeof(c));if(e!=ESP_OK)return fail(r,e==ESP_ERR_NO_MEM?"最多保存 6 个网络，请先删除一个":"保存失败，请重试");if(connect&&!queued)return fail(r,"网络已保存，但连接任务未启动，请从列表重试");return success(r,connect?"网络已保存，正在连接":"网络已保存，可从列表选择连接");}
static esp_err_t network_action(request_t*r,bool remove){if(!auth(r))return ESP_OK;if(xEventGroupGetBits(events)&2)return fail(r,"正在连接，请稍候");cJSON*j=body(r),*n=cJSON_GetObjectItemCaseSensitive(j,"index");int index=cJSON_IsNumber(n)&&n->valuedouble==n->valueint?n->valueint:-1;cJSON_Delete(j);wifi_config_t c;if(!profile_get(index,&c))return fail(r,"网络不存在，请刷新列表");if(!remove){bool ok=queue_wifi(&c);crypto_wipe(&c,sizeof(c));return ok?success(r,"正在连接选定网络"):fail(r,"已有连接任务，请稍候");}bool was_saved=have_saved&&!memcmp(c.sta.ssid,saved.sta.ssid,32);crypto_wipe(&c,sizeof(c));wifi_bank_t next=wifi_bank;crypto_wipe(&next.items[index],sizeof(next.items[index]));if(next.preferred==index){next.preferred=-1;for(int i=0;i<WIFI_SLOTS;i++)if(next.items[i].ssid[0]){next.preferred=i;break;}}esp_err_t e=nvs_set_blob(nv,"wifi_profiles",&next,sizeof(next));if(e==ESP_OK&&was_saved){if(next.preferred>=0){profile_config(&next.items[next.preferred],&c);e=nvs_set_blob(nv,"wifi",&c,sizeof(c));}else{e=nvs_erase_key(nv,"wifi");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;}}if(e==ESP_OK)e=nvs_commit(nv);if(e==ESP_OK){wifi_bank=next;if(was_saved){have_saved=next.preferred>=0;if(have_saved)saved=c;else crypto_wipe(&saved,sizeof(saved));}}crypto_wipe(&c,sizeof(c));crypto_wipe(&next,sizeof(next));return e==ESP_OK?success(r,"已删除保存项，当前连接不会立即断开"):fail(r,"删除失败，请重试");}

static void identity_restart_task(void*a){(void)a;vTaskDelay(pdMS_TO_TICKS(1500));esp_restart();}
static esp_err_t identity_reset_page(request_t*r){
 if(!auth(r))return ESP_OK;
 if(voice_busy())return fail(r,"通联中不能更换使用者");
 cJSON*j=body(r);const char*call=js(j,"callsign");const char*confirm=js(j,"confirm");
 if(!wire_callsign(call)||!confirm||strcmp(confirm,"更换使用者")){cJSON_Delete(j);return fail(r,"请填写新的裸呼号，并确认更换使用者");}
 identity_transition_t next={.version=1};snprintf(next.callsign,sizeof(next.callsign),"%s",call);cJSON_Delete(j);
 // Wi-Fi is running here, supplying hardware entropy; do not re-enable boot RNG.
 esp_fill_random(next.seed,sizeof(next.seed));
 esp_err_t e=nvs_set_blob(nv,"identity_next",&next,sizeof(next));crypto_wipe(&next,sizeof(next));
 if(e==ESP_OK)e=nvs_commit(nv);
 if(e!=ESP_OK)return fail(r,"保存身份切换失败，请重试；请勿继续申请证书");
 identity_restart_pending=true;
 if(xTaskCreate(identity_restart_task,"identity_reset",2048,NULL,4,NULL)!=pdPASS)return success(r,"身份切换已保存，请手动重启后继续；重启前不可申请或导入证书");
 return success(r,"身份切换已保存，设备即将重启。请重新输入管理码，为新使用者申请证书");
}
static esp_err_t request_page(request_t*r){if(!auth(r))return ESP_OK;cJSON*j=body(r);const char*s=js(j,"callsign");bool bi4tes=js(j,"provider")&&!strcmp(js(j,"provider"),"bi4tes");if(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"cancel"))){cJSON_Delete(j);esp_err_t ce=enroll_cancel(bi4tes?1:0);if(ce!=ESP_OK)return fail(r,"取消失败，请重试");snprintf(enrollment,sizeof(enrollment),"自动领取已停止，现有证书保留");return success(r,"自动领取已停止；不撤销签发方的申请");}if(bi4tes&&!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"trustRoot"))){cJSON_Delete(j);return fail(r,"请确认 BI4TES 签发体系和自动领取");}bool online=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"online"));if(!online){cJSON_Delete(j);return fail(r,"仅支持在线提交申请，不提供申请文件下载");}char enrollment_nonce[24]={0};uint8_t proof_nonce[16];const char*provided_nonce=js(j,"enrollmentNonce");if(online&&!bi4tes){if(!wire_unb64(provided_nonce,proof_nonce,16)){cJSON_Delete(j);return fail(r,"请先上传设备照片并填写现有 FMO UID");}snprintf(enrollment_nonce,sizeof(enrollment_nonce),"%s",provided_nonce);}char call[8]={0};if(s&&strlen(s)<sizeof(call))for(size_t i=0;i<strlen(s);i++)call[i]=toupper((unsigned char)s[i]);cJSON_Delete(j);if(!wire_callsign(call))return fail(r,"请输入国内裸呼号，例如 BH1JSS，不含后缀");if(bundle&&strcmp(call,callsign))return fail(r,"已安装证书；请到“更换使用者”生成新身份后再申请");esp_err_t e=nvs_set_str(nv,"callsign",call);if(e==ESP_OK)e=nvs_commit(nv);if(e!=ESP_OK)return fail(r,"无法保存申请呼号");snprintf(callsign,sizeof(callsign),"%s",call);xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.call,sizeof(view.call),"%s",call);xSemaphoreGive(lock);
 uint8_t nonce_bytes[16],sig[64],tbs[256];char nonce[24],pub[44],signature[88];esp_fill_random(nonce_bytes,16);wire_b64(nonce_bytes,16,nonce);if(online&&!bi4tes)snprintf(nonce,sizeof(nonce),"%s",enrollment_nonce);wire_b64(pk,32,pub);size_t n=wire_request_tbs(tbs,sizeof(tbs),call,pk,nonce);if(!n)return fail(r,"申请构造失败");crypto_ed25519_sign(sig,sk,tbs,n);wire_b64(sig,64,signature);cJSON*out=cJSON_CreateObject();cJSON_AddStringToObject(out,"format","fmo-m5-csr-v1");cJSON_AddStringToObject(out,"callsign",call);cJSON_AddStringToObject(out,"publicKey",pub);cJSON_AddStringToObject(out,"nonce",nonce);cJSON_AddStringToObject(out,"signature",signature);cJSON_AddStringToObject(out,"device","M5 FIRE");cJSON_AddStringToObject(out,"algorithm","Ed25519");if(bi4tes)cJSON_AddStringToObject(out,"provider","bi4tes");if(online){char*raw=cJSON_PrintUnformatted(out);esp_err_t saved_req=raw?enroll_save(bi4tes?1:0,raw):ESP_ERR_NO_MEM;free(raw);if(saved_req==ESP_OK)saved_req=nvs_commit(nv);if(saved_req!=ESP_OK){cJSON_Delete(out);return fail(r,"无法保存在线申请，请重试");}snprintf(enroll_states[bi4tes?1:0],120,"申请已保存，等待本体系审核领取");snprintf(enrollment,sizeof(enrollment),"%s",bi4tes?"BI4TES 自动领取已开启，原证书保持使用":"申请已保存，联网后自动提交并领取");}cJSON_Delete(out);return success(r,bi4tes?"BI4TES 自动领取已开启":"申请已保存，联网后自动提交并领取");}
static esp_err_t save_profile_seed(const char*key){uint8_t seed[32];size_t n=sizeof(seed);esp_err_t e=nvs_get_blob(nv,"seed",seed,&n);if(e==ESP_OK&&n!=32)e=ESP_FAIL;if(e==ESP_OK)e=nvs_set_blob(nv,key,seed,32);crypto_wipe(seed,sizeof(seed));return e;}
static const char* save_migrated_identity(cJSON*b,const char*encoded){
 if(time(NULL)<1700000000){return "请等待设备联网校时后再迁移";}uint8_t seed[32],secret[64],pub[32];if(!wire_unb64(encoded,seed,32))return "迁移密钥格式错误";
 crypto_ed25519_key_pair(secret,pub,seed);crypto_wipe(secret,sizeof(secret));
 const char*root=js(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b,"rootCert"),"subject"),"publicKey");
 const char*error=(!root||strcmp(root,"QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc"))?"迁移仅支持已固定的 BI4TES 根":wire_validate_bundle(b,pub,callsign,time(NULL));
 // key_pair consumes seed; decode again only after cryptographic validation.
 if(error){return error;}char*raw=cJSON_PrintUnformatted(b);if(!raw)return "内存不足";
 if(strlen(raw)>6144){free(raw);return "证书包过大";}wire_unb64(encoded,seed,32);
 esp_err_t e=nvs_set_str(nv,"cert_bi",raw);if(e==ESP_OK)e=nvs_set_blob(nv,"seed_bi",seed,32);if(e==ESP_OK){e=nvs_erase_key(nv,"enroll_req");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;}if(e==ESP_OK)e=nvs_commit(nv);if(e==ESP_OK)snprintf(enrollment,sizeof(enrollment),"迁移身份已保存，各体系领取任务保留");
 crypto_wipe(seed,sizeof(seed));free(raw);return e==ESP_OK?NULL:"迁移保存失败，当前身份未改变";
}
static const char* install_bundle(cJSON*b){
 const char*error=wire_validate_bundle(b,pk,callsign,time(NULL));if(error)return error;
 cJSON*root=cJSON_GetObjectItemCaseSensitive(b,"rootCert");const char*root_error=NULL;int slot=root_slot(root,true,&root_error);if(!slot)return root_error?root_error:"根 CA 保存失败";
 char*raw=cJSON_PrintUnformatted(b);if(!raw)return "可用内存不足";
 if(slot>2){
  if(strlen(raw)>6144){free(raw);return "证书包超过 6 KB";}char ck[24],skn[24];snprintf(ck,sizeof(ck),"cert_p%d",slot-3);snprintf(skn,sizeof(skn),"seed_p%d",slot-3);
  cJSON*previous=stored_json(ck,6145);if(previous){const char*oldpk=js(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(previous,"userCert"),"subject"),"publicKey");char localpk[44];wire_b64(pk,32,localpk);bool same=oldpk&&!strcmp(oldpk,localpk);cJSON_Delete(previous);if(!same){free(raw);return "该体系已有另一密钥的身份，请先切换到它后更新证书";}}
  // This path imports a certificate for the current local key, never a private key.
  esp_err_t se=save_profile_seed(skn);if(se==ESP_OK)se=nvs_set_str(nv,ck,raw);if(se==ESP_OK)se=nvs_commit(nv);free(raw);
  if(se!=ESP_OK)return "设备证书保存失败，当前身份保留";
  return NULL;
 }
 bool changed=!bundle||strcmp(bundle,raw);if(strlen(raw)>6144){free(raw);return "证书包超过 6 KB";}
 esp_err_t e=ESP_OK;if(bundle&&strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c")){e=nvs_set_str(nv,"cert_bh",bundle);if(e==ESP_OK)e=save_profile_seed("seed_bh");if(e==ESP_OK)e=nvs_commit(nv);}if(e==ESP_OK&&bundle&&!strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c")&&!strstr(bundle,"QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc")){e=nvs_set_str(nv,"cert_backup",bundle);if(e==ESP_OK)e=nvs_commit(nv);}const char*newroot=js(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b,"rootCert"),"subject"),"publicKey");if(e==ESP_OK&&newroot&&!strcmp(newroot,"QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc"))e=nvs_set_str(nv,"cert_bi",raw);if(e==ESP_OK&&newroot&&!strcmp(newroot,"QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc"))e=save_profile_seed("seed_bi");if(e==ESP_OK&&newroot&&!strcmp(newroot,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c"))e=nvs_set_str(nv,"cert_bh",raw);if(e==ESP_OK&&newroot&&!strcmp(newroot,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c"))e=save_profile_seed("seed_bh");if(e==ESP_OK)e=nvs_set_str(nv,"cert",raw);if(e==ESP_OK&&changed)e=nvs_set_u8(nv,"relay_notice",1);if(e==ESP_OK)e=nvs_commit(nv);if(e!=ESP_OK){free(raw);return "保存失败，原证书保留";}free(bundle);bundle=raw;identity_state();if(changed){discovery_identity_changed();identity_restart_pending=true;if(xTaskCreate(identity_restart_task,"cert_reload",2048,NULL,4,NULL)!=pdPASS)snprintf(enrollment,sizeof(enrollment),"证书已保存，请重启后加载中继列表");}
 nvs_erase_key(nv,"enroll_req");nvs_commit(nv);snprintf(enrollment,sizeof(enrollment),"设备证书已安装，无需重复申请");return NULL;
}
static esp_err_t import_page(request_t*r){if(!auth(r)){return ESP_OK;}if(voice_busy())return fail(r,"通联中不能更换证书");cJSON*j=body(r);if(!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"trustRoot"))){cJSON_Delete(j);return fail(r,"请明确确认信任本次证书的签发机构");}const char*secret=js(j,"privateSeed");bool migration=secret!=NULL;const char*error=migration?(!r->usb?"完整身份含私钥，请通过本机 USB 管理入口导入":save_migrated_identity(cJSON_GetObjectItemCaseSensitive(j,"bundle"),secret)):install_bundle(cJSON_GetObjectItemCaseSensitive(j,"bundle"));if(secret)crypto_wipe((void*)secret,strlen(secret));cJSON_Delete(j);return error?fail(r,error):success(r,migration?"BI4TES 身份已保存，尚未切换；请先让原 Passport 离线，再选择使用 BI4TES":"设备证书已保存，请在根 CA 列表查看或切换使用");}
// Poll only a locally persisted, user-initiated signed application. Transport
// carries public CSR/certificates; authenticity is enforced by pinned CA + full
// chain and device binding, not by trusting network status or a returned root.
static void enrollment_tick(void){
 static int64_t next=0;int64_t now=esp_timer_get_time();if(now<next||!(xEventGroupGetBits(events)&1)||voice_busy())return;
 next=now+30000000;static int cursor=0;int selected=-1;
 enrollment_job_t*job=calloc(1,sizeof(*job));if(!job)return;
 xSemaphoreTake(api_lock,portMAX_DELAY);for(int i=0;i<2;i++){int candidate=(cursor+i)%2;if(enroll_read(candidate,job)){selected=candidate;cursor=(candidate+1)%2;break;}}
 if(selected<0){crypto_wipe(job,sizeof(*job));free(job);xSemaphoreGive(api_lock);return;}char*request=job->request;
 // Keep management JSON/status allocations out of the HTTP/verification peak.
 // Network timeout is bounded; voice processing does not take this lock.
 if(voice_busy()||esp_get_free_heap_size()<16000){crypto_wipe(job,sizeof(*job));free(job);xSemaphoreGive(api_lock);return;}
 cJSON*pending=cJSON_Parse(request);bool bi4tes=js(pending,"provider")&&!strcmp(js(pending,"provider"),"bi4tes");cJSON_Delete(pending);
 char*reply=NULL;int used=0,code=0;bool complete=false;
 esp_http_client_config_t cfg={.url="http://fmo.bh1jss.net/m5-ca/apply",.method=HTTP_METHOD_POST,.timeout_ms=3000,.buffer_size=512,.buffer_size_tx=512,.disable_auto_redirect=true};
 esp_http_client_handle_t client=esp_http_client_init(&cfg);
 if(client){esp_http_client_set_header(client,"Content-Type","application/json");size_t n=strlen(request);if(esp_http_client_open(client,n)==ESP_OK&&esp_http_client_write(client,request,n)==(int)n){int64_t length=esp_http_client_fetch_headers(client);code=esp_http_client_get_status_code(client);if(code==200&&length>0&&length<=6144&&(reply=calloc(1,(size_t)length+1))){while(used<length&&!voice_busy()){int got=esp_http_client_read(client,reply+used,length-used);if(got<=0)break;used+=got;}complete=used==length;}}esp_http_client_close(client);esp_http_client_cleanup(client);}
 // Recheck the persisted request before installing. Management requests are
 // serialized with this bounded retrieval to keep the ESP32 heap available.
 // api_lock serializes cancellation, switch and task writes during this poll.
 bool same=true;
 if(same){
  if(!complete||voice_busy())snprintf(enrollment,sizeof(enrollment),"暂未领取，设备将在空闲联网时自动重试");
  else if(!shallow_json(reply))snprintf(enrollment,sizeof(enrollment),"领取内容格式错误，原证书保留");
  else {cJSON*j=cJSON_Parse(reply);free(reply);reply=NULL;const char*state=js(j,"state");if(state&&!strcmp(state,"pending"))snprintf(enrollment,sizeof(enrollment),"申请已提交，等待管理员审核签发");
   else if(state&&!strcmp(state,"none"))snprintf(enrollment,sizeof(enrollment),"BI4TES 尚无本机申请，请在网页提交资料");
   else if(state&&!strcmp(state,"need_evidence"))snprintf(enrollment,sizeof(enrollment),"请在网页补充设备照片和 FMO UID 后重新提交");
   else if(state&&!strcmp(state,"rejected"))snprintf(enrollment,sizeof(enrollment),"申请未通过，请联系管理员核实呼号");
   else if(state&&!strcmp(state,"ready")){cJSON*b=cJSON_GetObjectItemCaseSensitive(j,"bundle");const char*rootkey=js(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b,"rootCert"),"subject"),"publicKey");const char*error=NULL;
    if(!rootkey||strcmp(rootkey,bi4tes?"QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc":"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c"))error="签发根不匹配，拒绝自动安装";
    else if(time(NULL)<1700000000)error="证书可领取，等待网络校时后安装";
    else {error=enroll_store_ready(selected,job,b);if(!error){
     uint8_t activate=0;
     if(nvs_get_u8(nv,"profile_next",&activate)==ESP_OK&&activate==selected+1){
      identity_restart_pending=true;
      bool queued=xTaskCreate(identity_restart_task,"cert_activate",2048,NULL,4,NULL)==pdPASS;
      snprintf(enrollment,sizeof(enrollment),"%s",queued?"证书已自动启用，即将重启接入":"证书已自动启用，请重启设备");
     }else snprintf(enrollment,sizeof(enrollment),"证书已自动启用，可按服务器选择；当前身份保留");
    }}
    if(error)snprintf(enrollment,sizeof(enrollment),"%s",error);
   }else snprintf(enrollment,sizeof(enrollment),"领取状态异常，将自动重试");cJSON_Delete(j);}
 }
 snprintf(enroll_states[selected],120,"%s",enrollment);crypto_wipe(job,sizeof(*job));free(job);free(reply);xSemaphoreGive(api_lock);
}
static esp_err_t rollback_page(request_t*r){if(!auth(r))return ESP_OK;if(identity_restart_pending)return fail(r,"已有身份切换，请等待设备重启");if(voice_busy())return fail(r,"通联中不能更换证书");cJSON*choice=body(r);const char*provider=js(choice,"provider");bool select_bi=provider&&!strcmp(provider,"bi4tes");bool select_bh=provider&&!strcmp(provider,"bh1jss");cJSON*pid=cJSON_GetObjectItemCaseSensitive(choice,"profileId");int custom=cJSON_IsNumber(pid)?pid->valueint:0;if(custom&&(custom<3||custom>10||pid->valuedouble!=custom||!profile_exists(custom))){cJSON_Delete(choice);return fail(r,"证书体系编号无效");}cJSON_Delete(choice);char custom_cert[16],custom_seed[16];snprintf(custom_cert,sizeof(custom_cert),"cert_p%d",custom-3);snprintf(custom_seed,sizeof(custom_seed),"seed_p%d",custom-3);size_t n=0;const char*backup=custom?custom_cert:select_bi?"cert_bi":"cert_bh";if(!custom&&!select_bi&&!select_bh&&nvs_get_str(nv,backup,NULL,&n)!=ESP_OK)backup="cert_backup";if(nvs_get_str(nv,backup,NULL,&n)==ESP_ERR_NVS_NOT_FOUND&&(custom||select_bi||select_bh)){int id=custom?custom:select_bi?2:1;const char*err=prepare_empty_profile(id);if(err)return fail(r,err);identity_restart_pending=true;if(xTaskCreate(identity_restart_task,"empty_switch",2048,NULL,4,NULL)!=pdPASS)return success(r,"体系已选择，请手动重启");return success(r,"体系已选择，即将重启；请在网页申请或领取设备证书");}if(n<2||n>6145)return fail(r,"没有可恢复的证书");char*s=malloc(n);if(!s)return fail(r,"可用内存不足");esp_err_t e=nvs_get_str(nv,backup,s,&n);cJSON*b=e==ESP_OK?cJSON_Parse(s):NULL;uint8_t target_seed[32],target_sk[64],target_pk[32];size_t seed_size=32;bool bank=custom||!strcmp(backup,"cert_bi")||!strcmp(backup,"cert_bh");const char*seed_key=custom?custom_seed:!strcmp(backup,"cert_bi")?"seed_bi":"seed_bh";esp_err_t seed_e=bank?nvs_get_blob(nv,seed_key,target_seed,&seed_size):ESP_ERR_NVS_NOT_FOUND;if(seed_e==ESP_OK&&seed_size==32){crypto_ed25519_key_pair(target_sk,target_pk,target_seed);crypto_wipe(target_sk,sizeof(target_sk));}else memcpy(target_pk,pk,32);const char*error=wire_validate_bundle(b,target_pk,callsign,time(NULL));cJSON_Delete(b);if(error){free(s);return fail(r,error);}if(bank){
 // Retain original BH1JSS identity before selecting the imported Passport identity.
 if(bundle&&strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c")){e=nvs_set_str(nv,"cert_bh",bundle);if(e==ESP_OK)e=save_profile_seed("seed_bh");}
 if(e==ESP_OK&&seed_e!=ESP_OK)e=save_profile_seed(seed_key);
 if(e==ESP_OK){e=nvs_commit(nv);}if(e==ESP_OK)e=nvs_set_u8(nv,"profile_next",custom?custom:!strcmp(backup,"cert_bi")?2:1);if(e==ESP_OK)e=nvs_commit(nv);free(s);
 if(e!=ESP_OK){return fail(r,"身份切换保存失败，当前身份保留");}identity_restart_pending=true;
 if(xTaskCreate(identity_restart_task,"profile_switch",2048,NULL,4,NULL)!=pdPASS){return success(r,"身份切换已保存，请手动重启");}return success(r,"证书身份切换已保存，设备即将重启");
 }if(bundle&&strstr(bundle,"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c"))e=nvs_set_str(nv,"cert_bh",bundle);if(e==ESP_OK)e=nvs_set_str(nv,"cert",s);if(e==ESP_OK)e=nvs_erase_key(nv,"enroll_req");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;if(e==ESP_OK)e=nvs_commit(nv);if(e!=ESP_OK){free(s);return fail(r,"恢复失败，原证书保留");}nvs_erase_key(nv,"enroll_req");nvs_commit(nv);snprintf(enrollment,sizeof(enrollment),"已回退证书，各体系领取任务保留");free(bundle);bundle=s;identity_state();identity_restart_pending=true;if(xTaskCreate(identity_restart_task,"cert_reload",2048,NULL,4,NULL)!=pdPASS)return success(r,"证书已保存，请手动重启");return success(r,"已恢复保留证书，即将重启并刷新中继");}
// UI only queues a request; certificate validation and NVS writes run on the
// management worker, never on the LVGL/keyboard loop.
int device_profiles(device_profile_t out[DEVICE_PROFILE_MAX]){
 if(!api_lock||xSemaphoreTake(api_lock,0)!=pdTRUE)return -1;
 request_t r={.usb=true};roots_page(&r);cJSON*rows=cJSON_GetObjectItemCaseSensitive(r.output,"profiles");int count=cJSON_IsArray(rows)?cJSON_GetArraySize(rows):-1;if(count>DEVICE_PROFILE_MAX)count=DEVICE_PROFILE_MAX;
 for(int i=0;i<count;i++){cJSON*j=cJSON_GetArrayItem(rows,i);out[i].id=cJSON_GetObjectItemCaseSensitive(j,"id")->valueint;snprintf(out[i].name,sizeof(out[i].name),"%s",js(j,"name"));out[i].current=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"current"));out[i].active=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"active"));out[i].has_certificate=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"hasCertificate"));}
 cJSON_Delete(r.output);xSemaphoreGive(api_lock);return count;
}
bool device_profile_switch(int id){
 if(id<1||id>DEVICE_PROFILE_MAX||voice_busy()||atomic_load(&auto_relay_pending)||identity_restart_pending){return false;}int expected=0;if(!atomic_compare_exchange_strong(&ui_profile_request,&expected,id))return false;
 xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.profile_message,sizeof(view.profile_message),"正在校验证书，请稍候");xSemaphoreGive(lock);return true;
}
#include "auto_profiles.inc"
static void profile_switch_tick(void){
 int id=atomic_exchange(&ui_profile_request,0);if(!id)return;
 xSemaphoreTake(api_lock,portMAX_DELAY);request_t r={.usb=true,.input=cJSON_CreateObject()};if(!r.input){xSemaphoreGive(api_lock);return;}
 if(id<=2)cJSON_AddStringToObject(r.input,"provider",id==1?"bh1jss":"bi4tes");else cJSON_AddNumberToObject(r.input,"profileId",id);
 rollback_page(&r);const char*message=js(r.output,"error");if(!message)message=js(r.output,"message");
 xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.profile_message,sizeof(view.profile_message),"%s",message?message:"证书操作失败，请重试");xSemaphoreGive(lock);
 cJSON_Delete(r.input);cJSON_Delete(r.output);xSemaphoreGive(api_lock);
}
static esp_err_t delete_page(request_t*r){if(!auth(r))return ESP_OK;esp_err_t e=nvs_erase_key(nv,"cert");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;if(e==ESP_OK)e=nvs_commit(nv);if(e!=ESP_OK)return fail(r,"移除失败");nvs_erase_key(nv,"enroll_req");nvs_commit(nv);snprintf(enrollment,sizeof(enrollment),"尚未提交在线申请");free(bundle);bundle=NULL;identity_state();return success(r,"已移除用户证书，设备密钥保持不变");}

static esp_err_t reconnect_page(request_t*r){if(!auth(r))return ESP_OK;if(!have_saved)return fail(r,"尚无已保存的 Wi-Fi，请先配网");if(xEventGroupGetBits(events)&2)return fail(r,"正在连接，请稍候");if(network_attempted&&!network_started)return fail(r,"网络初始化失败，请重启设备");xEventGroupSetBits(events,2);if(!xQueueSend(jobs,&saved,0)){xEventGroupClearBits(events,2);return fail(r,"已有连接任务，请稍候");}return success(r,"正在重连已保存网络");}
bool device_select_relay(int index){
 if(voice_busy()||identity_restart_pending)return false;
 if(atomic_load(&auto_ca_mask)){
  relay_t target;if(!relays_get(index,&target))return false;
  xSemaphoreTake(lock,portMAX_DELAY);
  bool ok=!atomic_load(&auto_relay_pending)&&!atomic_load(&ui_profile_request);
  if(ok){auto_relay_target=target;atomic_store(&auto_relay_pending,1);snprintf(view.profile_message,sizeof(view.profile_message),"正在匹配可用证书体系");}
  xSemaphoreGive(lock);return ok;
 }
 if(!relays_select(index)){return false;}auto_connection_blocked=false;voice_connected(false);return true;
}
static esp_err_t relays_page(request_t*r){if(!auth(r))return ESP_OK;return response(r,relays_json());}
static esp_err_t relays_paged(request_t*r){if(!auth(r))return ESP_OK;cJSON*j=body(r),*v=cJSON_GetObjectItemCaseSensitive(j,"offset");int offset=cJSON_IsNumber(v)&&v->valuedouble>=0&&v->valuedouble<2147483600.0&&v->valuedouble==v->valueint?v->valueint:0;cJSON_Delete(j);return response(r,relays_json_page(offset));}
static esp_err_t add_relay_page(request_t*r){if(!auth(r)){return ESP_OK;}if(voice_busy())return fail(r,"通联中不能添加中继，请结束通联后重试");cJSON*j=body(r);const char*e=relays_add(j);bool connect=cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"connect"));if(!e&&connect){bool found=false;for(int i=0;i<relays_count();i++){relay_t v;if(relays_get(i,&v)&&v.uid==(uint32_t)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(j,"uid"))&&v.port==(int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(j,"port"))&&!strcmp(v.call,js(j,"callsign"))&&!strcmp(v.host,js(j,"host"))&&!strcmp(v.fingerprint,js(j,"fingerprint"))){found=device_select_relay(i);break;}}if(!found)e="选择失败，请重试";}cJSON_Delete(j);return e?fail(r,e):success(r,connect?"正在尝试接入":"中继已保存，请在设备列表选择");}
static esp_err_t select_relay_page(request_t*r){if(!auth(r))return ESP_OK;cJSON*j=body(r);cJSON*i=cJSON_GetObjectItemCaseSensitive(j,"index");bool ok=cJSON_IsNumber(i)&&i->valuedouble==i->valueint&&device_select_relay(i->valueint);cJSON_Delete(j);return ok?success(r,"正在切换中继"):fail(r,"中继无效或正在通联，请稍后重试");}
static esp_err_t delete_relay_page(request_t*r){if(!auth(r)){return ESP_OK;}if(voice_busy())return fail(r,"通联中不能删除中继");cJSON*j=body(r);cJSON*i=cJSON_GetObjectItemCaseSensitive(j,"index"),*u=cJSON_GetObjectItemCaseSensitive(j,"uid"),*p=cJSON_GetObjectItemCaseSensitive(j,"port");const char*h=js(j,"host"),*f=js(j,"fingerprint");relay_t expected={0};const char*error="删除请求无效，请刷新后重试";
if(cJSON_IsNumber(i)&&i->valuedouble==i->valueint&&cJSON_IsNumber(u)&&u->valuedouble>=1&&u->valuedouble<=4294967295.0&&cJSON_IsNumber(p)&&h&&f&&strlen(h)<sizeof(expected.host)&&strlen(f)<sizeof(expected.fingerprint)){expected.uid=u->valuedouble;expected.port=p->valueint;snprintf(expected.host,sizeof(expected.host),"%s",h);snprintf(expected.fingerprint,sizeof(expected.fingerprint),"%s",f);error=relays_remove(i->valueint,&expected);}cJSON_Delete(j);return error?fail(r,error):success(r,"中继已删除，设备菜单已同步更新");}
#ifdef FMO_SSTV_STREAMING
static bool gallery_chunk(void *ctx,const uint8_t *p,size_t n){return httpd_resp_send_chunk(ctx,(const char*)p,n)==ESP_OK;}
#endif
static esp_err_t gallery_initialize(request_t*r){
 if(!auth(r))return ESP_OK;
 cJSON*j=body(r);const char*confirm=js(j,"confirm");bool confirmed=confirm&&!strcmp(confirm,"ERASE_SSTV_ALBUM");cJSON_Delete(j);
 if(!confirmed)return fail(r,"请先确认清空相册区域");
 if(voice_busy()||sstv_rx_active())return fail(r,"请退出 SSTV 接收并结束通联后重试");
 const char*e=sstv_store_initialize_confirmed();return e?fail(r,e):success(r,"相册已初始化，可以重新进入 SSTV 接收");
}
static esp_err_t gallery_page(request_t*r,const char*op){
 if(!auth(r))return ESP_OK;
 const char*error=sstv_store_error();if(error){
  if(!strcmp(op,"gallery")){cJSON*j=cJSON_CreateObject();cJSON_AddArrayToObject(j,"images");cJSON_AddStringToObject(j,"storageError",error);cJSON_AddBoolToObject(j,"needsInitialization",sstv_store_needs_initialization());return response(r,j);}
  return fail(r,error);
 }
 if(!strcmp(op,"gallery")){
  sstv_entry_t entries[SSTV_GALLERY_MAX];int n=sstv_store_list(entries);if(n<0)return fail(r,"相册忙，请稍后重试");
  cJSON*j=cJSON_CreateObject(),*a=cJSON_AddArrayToObject(j,"images");cJSON_AddNumberToObject(j,"capacity",SSTV_GALLERY_MAX);
#ifdef FMO_FULL_SSTV
  sstv_storage_t space;if(sstv_store_stats(&space)){cJSON*st=cJSON_AddObjectToObject(j,"storage");cJSON_AddNumberToObject(st,"total",space.total);cJSON_AddNumberToObject(st,"used",space.used);cJSON_AddNumberToObject(st,"safeLimit",space.safe_limit);cJSON_AddNumberToObject(st,"remaining",space.remaining);cJSON_AddBoolToObject(st,"low",space.low);}
#endif

  for(int i=0;i<n;i++){cJSON*v=cJSON_CreateObject();cJSON_AddNumberToObject(v,"id",entries[i].id);cJSON_AddNumberToObject(v,"savedAt",entries[i].saved_at);cJSON_AddNumberToObject(v,"width",entries[i].width);cJSON_AddNumberToObject(v,"height",entries[i].height);cJSON_AddNumberToObject(v,"fileBytes",entries[i].file_bytes);cJSON_AddBoolToObject(v,"compressed",entries[i].compressed);cJSON_AddBoolToObject(v,"partial",entries[i].partial);cJSON_AddNumberToObject(v,"receivedLines",entries[i].received_lines);cJSON_AddStringToObject(v,"relay",entries[i].relay);cJSON_AddItemToArray(a,v);}return response(r,j);
 }
 cJSON*j=body(r),*item=cJSON_GetObjectItemCaseSensitive(j,"id");uint32_t id=0;
 if(cJSON_IsNumber(item)&&item->valuedouble>=1&&item->valuedouble<=4294967295.0&&item->valuedouble==(uint32_t)item->valuedouble)id=item->valuedouble;
 cJSON_Delete(j);if(!id)return fail(r,"图片编号无效");
 if(!strcmp(op,"gallery_delete")){if(voice_busy())return fail(r,"通联中，请稍后删除");return sstv_store_delete(id)?success(r,"图片已删除"):fail(r,"删除失败，请刷新相册");}
 if(r->usb)return fail(r,"图片预览请使用设备局域网管理页面");
#ifdef FMO_SSTV_STREAMING
 httpd_resp_set_type(r->http,"application/octet-stream");httpd_resp_set_hdr(r->http,"Cache-Control","no-store");
 bool ok=sstv_store_export(id,gallery_chunk,r->http);
 if(!ok)return ESP_FAIL;
 return httpd_resp_send_chunk(r->http,NULL,0);
#else
 uint8_t*p=malloc(SSTV_BYTES);if(!p)return fail(r,"内存不足，请稍后查看");
 if(!sstv_store_load(id,p)){free(p);return fail(r,"图片不存在、损坏或相册忙");}
 httpd_resp_set_type(r->http,"application/octet-stream");httpd_resp_set_hdr(r->http,"Cache-Control","no-store");httpd_resp_set_hdr(r->http,"X-Content-Type-Options","nosniff");
 esp_err_t e=httpd_resp_send(r->http,(const char*)p,SSTV_BYTES);free(p);return e;
#endif
}
#ifdef CONFIG_FMO_BOARD_XINGZHI
static esp_err_t wall_probe(request_t*r);
#endif
static esp_err_t dispatch(request_t*r,const char*op){
#ifndef CONFIG_FMO_BOARD_XINGZHI
 if(!strcmp(op,"mic_diagnostics")&&r->usb){if(!auth(r))return ESP_OK;voice_view_t v;voice_view(&v);cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"testing",v.mic_test);cJSON_AddBoolToObject(j,"tx",v.tx);cJSON_AddNumberToObject(j,"sampleRate",v.mic_rate);cJSON_AddNumberToObject(j,"adcOverflows",v.tx_adc_overflows);cJSON_AddNumberToObject(j,"clipped",v.mic_clipped);
#define MIC_METRIC(k) cJSON_AddNumberToObject(j,#k,v.mic_metrics.k)
 MIC_METRIC(minimum);MIC_METRIC(maximum);MIC_METRIC(dc);MIC_METRIC(raw_rms);MIC_METRIC(pcm_rms);MIC_METRIC(hum50_permille);MIC_METRIC(hum100_permille);MIC_METRIC(tag_errors);MIC_METRIC(codec_error);
#undef MIC_METRIC
 return response(r,j);}
#endif

 if(identity_restart_pending)return fail(r,"身份切换已保存，请等待或手动重启设备后重新登录");
#ifdef CONFIG_FMO_BOARD_XINGZHI
 if(!strcmp(op,"sstv_wall_probe"))return wall_probe(r);
#endif
#ifdef FMO_FULL_SSTV
#ifdef FMO_SSTV_STREAMING
 if(!strcmp(op,"sstv_codec_probe")&&r->usb){
  if(!auth(r))return ESP_OK;
  sstv_diagnostics_t diag;if(voice_busy()||!sstv_rx_diagnostics(&diag)||diag.enabled||diag.status==SSTV_COMPLETE)return fail(r,"请先退出接收页面");
  uint8_t row[1280];memset(row,0x55,sizeof(row));bool ok=sstv_stream_prepare()&&sstv_stream_begin("local storage diagnostic");unsigned max_us=0;
  for(unsigned y=0;ok&&y<240;y+=2){int64_t start=esp_timer_get_time();ok=sstv_stream_rows(y,row);unsigned dt=esp_timer_get_time()-start;if(dt>max_us)max_us=dt;}
  if(ok)ok=sstv_stream_finish(0);
  sstv_stream_discard();sstv_rx_enable(false);
  cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"streamWritten",ok);cJSON_AddNumberToObject(j,"maxPairWriteUs",max_us);cJSON_AddNumberToObject(j,"sourceBytes",SSTV_BYTES);return response(r,j);
 }
#else
 if(!strcmp(op,"sstv_codec_probe")&&r->usb){
  if(!auth(r)){return ESP_OK;}if(voice_busy())return fail(r,"通联中，请稍后检查");
  sstv_diagnostics_t diag;if(!sstv_rx_diagnostics(&diag)||diag.enabled)return fail(r,"请先退出接收页面");
  uint8_t *src=heap_caps_malloc(SSTV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),*dst=heap_caps_malloc(SSTV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),*jpeg=NULL;size_t bytes=0;
  if(src)for(unsigned y=0;y<240;y++)for(unsigned x=0;x<320;x++){unsigned v=((x*31/319)<<11)|((y*63/239)<<5)|((x+y)*31/558),at=(y*320+x)*2;src[at]=v;src[at+1]=v>>8;}
  bool encoded=src&&dst&&sstv_jpeg_encode(src,&jpeg,&bytes),decoded=encoded&&sstv_jpeg_decode(jpeg,bytes,dst);unsigned long difference=0;
  if(decoded)for(unsigned i=0;i<SSTV_BYTES;i+=2){unsigned a=src[i]|src[i+1]<<8,b=dst[i]|dst[i+1]<<8;difference+=abs((int)(a>>11)-(int)(b>>11))*255/31+abs((int)((a>>5)&63)-(int)((b>>5)&63))*255/63+abs((int)(a&31)-(int)(b&31))*255/31;}
  free(src);free(dst);free(jpeg);cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"encoded",encoded);cJSON_AddBoolToObject(j,"decoded",decoded);cJSON_AddNumberToObject(j,"jpegBytes",bytes);if(decoded)cJSON_AddNumberToObject(j,"meanChannelError",(double)difference/(320*240*3));return response(r,j);
 }
 if(!strcmp(op,"sstv_storage_probe")&&r->usb){
  if(!auth(r)){return ESP_OK;}if(voice_busy())return fail(r,"通联中，请稍后检查");
  sstv_diagnostics_t diag;if(!sstv_rx_diagnostics(&diag)||diag.enabled)return fail(r,"请先退出接收页面");
  cJSON*body_json=body(r);cJSON*value=cJSON_GetObjectItemCaseSensitive(body_json,"id");uint32_t id=cJSON_IsNumber(value)&&value->valuedouble>=1&&value->valuedouble<=4294967295.0?(uint32_t)value->valuedouble:0;cJSON_Delete(body_json);if(!id)return fail(r,"图片编号无效");
  uint8_t*p=heap_caps_malloc(SSTV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),*decoded=heap_caps_malloc(SSTV_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT),*jpeg=NULL;size_t bytes=0;
  bool loaded=p&&decoded&&sstv_store_load(id,p);bool encoded=loaded&&sstv_jpeg_encode(p,&jpeg,&bytes);bool ok=encoded&&sstv_jpeg_decode(jpeg,bytes,decoded);free(p);free(decoded);free(jpeg);
  cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"loaded",loaded);cJSON_AddBoolToObject(j,"encoded",encoded);cJSON_AddBoolToObject(j,"decoded",ok);cJSON_AddNumberToObject(j,"jpegBytes",bytes);cJSON_AddNumberToObject(j,"sourceBytes",SSTV_BYTES);return response(r,j);
 }

#endif
 if(!strcmp(op,"sstv_prepare")&&r->usb){sstv_rx_init();op="sstv_status";}
 if(!strcmp(op,"sstv_status")){
  if(!auth(r)){return ESP_OK;}
  sstv_diagnostics_t d;if(!sstv_rx_diagnostics(&d))return fail(r,"SSTV busy");cJSON*j=cJSON_CreateObject();
  cJSON_AddBoolToObject(j,"enabled",d.enabled);cJSON_AddBoolToObject(j,"ready",d.ready);
#define SSTV_FIELD(name) cJSON_AddNumberToObject(j,#name,d.name)
  SSTV_FIELD(init_error);SSTV_FIELD(internal_free);SSTV_FIELD(internal_largest);SSTV_FIELD(samples);SSTV_FIELD(stream_breaks);SSTV_FIELD(timeouts);SSTV_FIELD(drops);SSTV_FIELD(clipped);SSTV_FIELD(max_process_us);SSTV_FIELD(max_store_us);SSTV_FIELD(store_stalls);SSTV_FIELD(max_feed_gap_us);SSTV_FIELD(snapshots);SSTV_FIELD(snapshot_misses);SSTV_FIELD(preview_nonzero);SSTV_FIELD(lines);SSTV_FIELD(leader_max);SSTV_FIELD(vis_starts);SSTV_FIELD(vis_code);SSTV_FIELD(frequency);SSTV_FIELD(status);
#undef SSTV_FIELD
#ifdef CONFIG_FMO_BOARD_XINGZHI
#define UI_FIELD(name) cJSON_AddNumberToObject(j,#name,atomic_load(&name))
  UI_FIELD(ui_stage);UI_FIELD(ui_loop_max_ms);UI_FIELD(ui_refresh_max_ms);UI_FIELD(ui_render_max_ms);UI_FIELD(ui_flush_sent);UI_FIELD(ui_flush_done);UI_FIELD(ui_flush_errors);UI_FIELD(ui_flush_non_dma);UI_FIELD(ui_flush_last_error);
#undef UI_FIELD
  unsigned ui_now=(unsigned)(esp_timer_get_time()/1000);
  cJSON_AddNumberToObject(j,"ui_stage_age_ms",ui_now-atomic_load(&ui_stage_ms));
  cJSON_AddNumberToObject(j,"ui_loop_age_ms",ui_now-atomic_load(&ui_loop_ms));
#endif
  return response(r,j);
 }
#endif
 if(!strcmp(op,"gallery_initialize"))return gallery_initialize(r);
 if(!strcmp(op,"gallery")||!strcmp(op,"gallery_image")||!strcmp(op,"gallery_delete"))return gallery_page(r,op);
 if(!strcmp(op,"discovery")){if(!auth(r))return ESP_OK;return response(r,discovery_json());}
 if(!strcmp(op,"discovery_refresh")){if(!auth(r))return ESP_OK;discovery_refresh();nvs_erase_key(nv,"relay_notice");nvs_commit(nv);return success(r,"已安排刷新，空闲时按当前身份重新验证接入");}
 if(!strcmp(op,"discovery_add")){if(!auth(r))return ESP_OK;cJSON*j=body(r),*i=cJSON_GetObjectItemCaseSensitive(j,"index"),*v=cJSON_GetObjectItemCaseSensitive(j,"revision");const char*error="请求无效";if(cJSON_IsNumber(i)&&i->valuedouble==i->valueint&&cJSON_IsNumber(v)&&v->valuedouble>=0&&v->valuedouble<=4294967295.0&&v->valuedouble==(uint32_t)v->valuedouble)error=discovery_add(i->valueint,(uint32_t)v->valuedouble);cJSON_Delete(j);return error?fail(r,error):success(r,"已添加到列表，尚未切换；接入权限仍需验证");}
 if(!strcmp(op,"discovery_select")){if(!auth(r))return ESP_OK;cJSON*j=body(r),*i=cJSON_GetObjectItemCaseSensitive(j,"index"),*v=cJSON_GetObjectItemCaseSensitive(j,"revision");const char*error="请求无效";if(cJSON_IsNumber(i)&&i->valuedouble==i->valueint&&cJSON_IsNumber(v)&&v->valuedouble>=0&&v->valuedouble<=4294967295.0&&v->valuedouble==(uint32_t)v->valuedouble)error=discovery_select(i->valueint,(uint32_t)v->valuedouble);cJSON_Delete(j);return error?fail(r,error):success(r,"正在接入所选中继");}
 if(!strcmp(op,"logout"))return logout_page(r);
 if(!strcmp(op,"identity_reset"))return identity_reset_page(r);
 if(!strcmp(op,"relay_name")){if(!auth(r))return ESP_OK;cJSON*j=body(r);const char*error=relays_update_name(j);cJSON_Delete(j);return error?fail(r,error):success(r,"中继名称已更新");}
 if(!strcmp(op,"relay_delete"))return delete_relay_page(r);
 if(!strcmp(op,"relays"))return relays_page(r);
 if(!strcmp(op,"relays_paged"))return relays_paged(r);
 if(!strcmp(op,"relay_add"))return add_relay_page(r);
 if(!strcmp(op,"relay_select"))return select_relay_page(r);
 if(!strcmp(op,"status"))return status_page(r);
 if(!strcmp(op,"scan"))return scan_page(r);
 if(!strcmp(op,"networks"))return networks_page(r);
 if(!strcmp(op,"network_connect"))return network_action(r,false);
 if(!strcmp(op,"network_delete"))return network_action(r,true);
 if(!strcmp(op,"wifi"))return network_page(r);
 if(!strcmp(op,"reconnect"))return reconnect_page(r);
 if(!strcmp(op,"request"))return request_page(r);
 if(!strcmp(op,"certificate"))return import_page(r);
 if(!strcmp(op,"roots"))return roots_page(r);
 if(!strcmp(op,"rollback"))return rollback_page(r);
 if(!strcmp(op,"delete"))return delete_page(r);
 if(!strcmp(op,"login"))return login_page(r);
 return fail(r,"未知操作");
}
static esp_err_t http_api(httpd_req_t*h){request_t r={.http=h};xSemaphoreTake(api_lock,portMAX_DELAY);esp_err_t e=dispatch(&r,(const char*)h->user_ctx);xSemaphoreGive(api_lock);return e;}
static esp_err_t http_transport(httpd_req_t*h){httpd_resp_set_type(h,"application/json");return httpd_resp_sendstr(h,"{\"usb\":false}");}
static void wipe_json(cJSON*j){if(!j)return;for(cJSON*c=j->child;c;c=c->next)wipe_json(c);if(cJSON_IsString(j)&&j->valuestring)crypto_wipe(j->valuestring,strlen(j->valuestring));}
static bool shallow_json(const char*s){bool quoted=false,escaped=false;int depth=0;for(;*s;s++){char c=*s;if(quoted){if(escaped)escaped=false;else if(c=='\\')escaped=true;else if(c=='"')quoted=false;}else if(c=='"')quoted=true;else if(c=='{'||c=='['){if(++depth>12)return false;}else if(c=='}'||c==']')depth--;}return true;}
static int board_usb_read(void *buf,size_t n){
#ifdef CONFIG_FMO_BOARD_XINGZHI
 return usb_serial_jtag_read_bytes(buf,n,pdMS_TO_TICKS(50));
#else
 return uart_read_bytes(UART_NUM_0,buf,n,pdMS_TO_TICKS(50));
#endif
}
static int board_usb_write(const void *buf,size_t n){
#ifdef CONFIG_FMO_BOARD_XINGZHI
 size_t sent=0;int64_t deadline=esp_timer_get_time()+500000;
 // The USB driver queues each write atomically: a whole status reply exceeds its ring buffer.
 while(sent<n&&esp_timer_get_time()<deadline){size_t chunk=n-sent;if(chunk>128)chunk=128;int count=usb_serial_jtag_write_bytes((const char*)buf+sent,chunk,pdMS_TO_TICKS(50));if(count>0)sent+=count;else vTaskDelay(1);}
 return (int)sent;
#else
 return uart_write_bytes(UART_NUM_0,buf,n);
#endif
}
#ifdef FMO_SSTV_STREAMING
#include "usb_json_stream.h"
static bool usb_json_output(void *ctx,const char *p,size_t n){(void)ctx;return board_usb_write(p,n)==(int)n;}
#endif
static bool usb_append(char**line,size_t*capacity,size_t*n,char c){
 if(*n>=8192)return false;
 if(*n+1>=*capacity){size_t next=*capacity*2;if(next>8193)next=8193;char*grown=realloc(*line,next);if(!grown)return false;*line=grown;*capacity=next;}
 (*line)[(*n)++]=c;return true;
}
static void usb_task(void*a){(void)a;size_t capacity=256;char*line=calloc(1,capacity);if(!line){vTaskDelete(NULL);return;}size_t n=0;bool overflow=false;uint8_t chunk[128];
 while(1){int count=board_usb_read(chunk,sizeof(chunk));for(int x=0;x<count;x++){char c=chunk[x];if(c=='\n'){
  line[n]=0;if(!overflow&&n>4&&!memcmp(line,"@M5:",4)&&shallow_json(line+4)){
   cJSON*j=cJSON_Parse(line+4);const char*op=js(j,"op"),*id=js(j,"id");
   if(op&&id&&strlen(id)<=32){request_t r={.usb=true,.input=cJSON_GetObjectItemCaseSensitive(j,"data")};xSemaphoreTake(api_lock,portMAX_DELAY);dispatch(&r,op);if(r.output){cJSON_AddStringToObject(r.output,"id",id);
#ifdef FMO_SSTV_STREAMING
board_usb_write("\n@M5:",5);if(usb_json_write(r.output,usb_json_output,NULL))board_usb_write("\n",1);
#else
char*out=cJSON_PrintBuffered(r.output,256,false);if(out){board_usb_write("\n@M5:",5);board_usb_write(out,strlen(out));board_usb_write("\n",1);free(out);}
#endif
cJSON_Delete(r.output);}xSemaphoreGive(api_lock);}
   wipe_json(j);cJSON_Delete(j);
  }crypto_wipe(line,n);n=0;overflow=false;if(capacity>256){char*small=realloc(line,256);if(small){line=small;capacity=256;}}
 }else if(!overflow&&!usb_append(&line,&capacity,&n,c))overflow=true;}crypto_wipe(chunk,sizeof(chunk));}
}
#include "sstv_upload.inc"
static esp_err_t network_start(void){
 ESP_RETURN_ON_ERROR(esp_netif_init(),TAG,"netif");ESP_RETURN_ON_ERROR(esp_event_loop_create_default(),TAG,"event");esp_netif_create_default_wifi_sta();esp_netif_create_default_wifi_ap();wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg),TAG,"wifi");ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM),TAG,"storage");
 ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_APSTA),TAG,"mode");
 wifi_config_t ap={0};memcpy(ap.ap.ssid,apname,strnlen(apname,32));snprintf((char*)ap.ap.password,sizeof(ap.ap.password),"%s",code);ap.ap.ssid_len=strlen(apname);ap.ap.channel=1;ap.ap.max_connection=2;ap.ap.authmode=WIFI_AUTH_WPA2_PSK;ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_AP,&ap),TAG,"AP config");ap_enabled=true;view.ap_active=true;
 // Restore normal B/G/N after the B/G-only association trial failed.
 ESP_RETURN_ON_ERROR(esp_wifi_set_protocol(WIFI_IF_STA,WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N),TAG,"Wi-Fi bgn protocol");
 ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_event,NULL),TAG,"event");ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_event,NULL),TAG,"event");ESP_RETURN_ON_ERROR(esp_wifi_start(),TAG,"start");ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE),TAG,"power save");
 
 httpd_config_t h=HTTPD_DEFAULT_CONFIG();h.stack_size=14336;h.max_req_hdr_len=1536;h.max_uri_handlers=39;h.max_open_sockets=4;h.lru_purge_enable=true;h.recv_wait_timeout=3;h.send_wait_timeout=5;httpd_handle_t server;ESP_RETURN_ON_ERROR(httpd_start(&server,&h),TAG,"http");
 const httpd_uri_t routes[]={
#ifdef CONFIG_FMO_BOARD_XINGZHI
 {.uri="/api/gallery/upload",.method=HTTP_POST,.handler=gallery_wall_http},
 {.uri="/api/gallery/wall/delete",.method=HTTP_POST,.handler=gallery_wall_http},
 {.uri="/api/gallery/wall/status",.method=HTTP_GET,.handler=http_api,.user_ctx="sstv_wall_probe"},
 {.uri="/api/sstv/status",.method=HTTP_GET,.handler=http_api,.user_ctx="sstv_status"},
#endif
 {.uri="/api/gallery/initialize",.method=HTTP_POST,.handler=http_api,.user_ctx="gallery_initialize"}, {.uri="/api/gallery",.method=HTTP_GET,.handler=http_api,.user_ctx="gallery"}, {.uri="/api/gallery/image",.method=HTTP_POST,.handler=http_api,.user_ctx="gallery_image"}, {.uri="/api/gallery/delete",.method=HTTP_POST,.handler=http_api,.user_ctx="gallery_delete"}, {.uri="/api/roots",.method=HTTP_POST,.handler=http_api,.user_ctx="roots"}, {.uri="/api/relays/page",.method=HTTP_POST,.handler=http_api,.user_ctx="relays_paged"}, {.uri="/api/discovery/add",.method=HTTP_POST,.handler=http_api,.user_ctx="discovery_add"}, {.uri="/api/discovery",.method=HTTP_GET,.handler=http_api,.user_ctx="discovery"},{.uri="/api/discovery/refresh",.method=HTTP_POST,.handler=http_api,.user_ctx="discovery_refresh"},{.uri="/api/discovery/select",.method=HTTP_POST,.handler=http_api,.user_ctx="discovery_select"}, {.uri="/api/logout",.method=HTTP_POST,.handler=http_api,.user_ctx="logout"}, {.uri="/api/relay/name",.method=HTTP_POST,.handler=http_api,.user_ctx="relay_name"}, {.uri="/api/relay/delete",.method=HTTP_POST,.handler=http_api,.user_ctx="relay_delete"}, {.uri="/api/identity/reset",.method=HTTP_POST,.handler=http_api,.user_ctx="identity_reset"}, {.uri="/api/certificate/rollback",.method=HTTP_POST,.handler=http_api,.user_ctx="rollback"}, {.uri="/api/scan",.method=HTTP_POST,.handler=http_api,.user_ctx="scan"}, {.uri="/api/networks",.method=HTTP_GET,.handler=http_api,.user_ctx="networks"}, {.uri="/api/network/connect",.method=HTTP_POST,.handler=http_api,.user_ctx="network_connect"}, {.uri="/api/network/delete",.method=HTTP_POST,.handler=http_api,.user_ctx="network_delete"}, {.uri="/api/relays",.method=HTTP_GET,.handler=http_api,.user_ctx="relays"}, {.uri="/api/relays",.method=HTTP_POST,.handler=http_api,.user_ctx="relay_add"}, {.uri="/api/relay/select",.method=HTTP_POST,.handler=http_api,.user_ctx="relay_select"}, {.uri="/api/reconnect",.method=HTTP_POST,.handler=http_api,.user_ctx="reconnect"}, {.uri="/",.method=HTTP_GET,.handler=index_page}, {.uri="/api/transport",.method=HTTP_GET,.handler=http_transport}, {.uri="/api/login",.method=HTTP_POST,.handler=http_api,.user_ctx="login"}, {.uri="/api/status",.method=HTTP_GET,.handler=http_api,.user_ctx="status"}, {.uri="/api/wifi",.method=HTTP_POST,.handler=http_api,.user_ctx="wifi"}, {.uri="/api/request",.method=HTTP_POST,.handler=http_api,.user_ctx="request"}, {.uri="/api/certificate",.method=HTTP_POST,.handler=http_api,.user_ctx="certificate"}, {.uri="/api/certificate",.method=HTTP_DELETE,.handler=http_api,.user_ctx="delete"}};
 for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]);i++)ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server,&routes[i]),TAG,"route");
 ESP_LOGI(TAG,"STA network and HTTP initialized");return ESP_OK;
}

static void relay_msg(const char*s,bool connected){xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.relay,sizeof(view.relay),"%s",s);view.mqtt_online=connected;xSemaphoreGive(lock);}
static relay_t mqtt_target;
static bool publish_voice(const uint8_t*p,size_t n){
 if(xSemaphoreTake(mqtt_lock,0)!=pdTRUE)return false;
 bool ok=active_mqtt&&atomic_load(&mqtt_phase)==LINK_READY&&esp_mqtt_client_enqueue_timeout(active_mqtt,"FMO/RAW",(const char*)p,n,0,0,true,0)>=0;
 xSemaphoreGive(mqtt_lock);return ok;
}
static void mqtt_event(void*a,esp_event_base_t base,int32_t id,void*data){(void)a;(void)base;esp_mqtt_event_handle_t e=data;static uint8_t packet[VOICE_WIRE_PACKET_MAX];static int received,total;static bool collecting;
 if(id==MQTT_EVENT_CONNECTED){relays_connection(&mqtt_target,true);atomic_store(&mqtt_phase,LINK_READY);relay_msg("中继认证成功",true);voice_connected(true);esp_mqtt_client_subscribe(e->client,"FMO/RAW",0);ESP_LOGI(TAG,"MQTT authenticated connection established");}
 else if(id==MQTT_EVENT_DISCONNECTED){atomic_store(&mqtt_phase,LINK_FAILED);voice_connected(false);collecting=false;relay_msg("连接已断开，等待重连",false);}
 else if(id==MQTT_EVENT_ERROR){relays_connection(&mqtt_target,false);atomic_store(&mqtt_phase,LINK_FAILED);if(e->error_handle)ESP_LOGW(TAG,"MQTT error type=%d socket=%d tls=%d broker=%d",e->error_handle->error_type,e->error_handle->esp_transport_sock_errno,e->error_handle->esp_tls_last_esp_err,e->error_handle->connect_return_code);voice_connected(false);relay_msg("接入失败，稍后重试",false);}
 else if(id==MQTT_EVENT_DATA){if(e->current_data_offset==0){received=0;total=e->total_data_len;collecting=e->topic_len==7&&!memcmp(e->topic,"FMO/RAW",7)&&total>0&&total<=VOICE_WIRE_PACKET_MAX;if(e->topic_len==7&&!memcmp(e->topic,"FMO/RAW",7)&&total>VOICE_WIRE_PACKET_MAX)voice_reject_oversized(total);}if(collecting){if(e->current_data_offset!=received||e->data_len<0||received+e->data_len>total){collecting=false;return;}memcpy(packet+received,e->data,e->data_len);received+=e->data_len;if(received==total){collecting=false;voice_receive(packet,total);}}}
}
static char *mqtt_password(uint32_t*uid,const relay_t*target){
 if(!bundle)return NULL;
 cJSON*b=cJSON_Parse(bundle);if(wire_validate_bundle(b,pk,callsign,time(NULL))){cJSON_Delete(b);return NULL;}
 cJSON*u=cJSON_GetObjectItemCaseSensitive(b,"userCert");
 uint8_t*scratch=malloc(4096);if(!scratch){cJSON_Delete(b);return NULL;}size_t n=wire_cert_tbs(u,scratch,4096);uint8_t ufp[32],sfp[32],sig[64];
 const char*server_fp=target->fingerprint;int64_t now=time(NULL);if(!n||mbedtls_sha256(scratch,n,ufp,0)!=0||!wire_unb64(server_fp,sfp,32)){free(scratch);cJSON_Delete(b);return NULL;}
 n=wire_login_tbs(scratch,4096,target->uid,target->call,target->host,target->port,sfp,now,ufp);crypto_ed25519_sign(sig,sk,scratch,n);free(scratch);char signature[88];wire_b64(sig,64,signature);
 cJSON*payload=cJSON_CreateObject(),*pack=cJSON_CreateObject(),*proof=cJSON_CreateObject();cJSON_AddItemToObject(payload,"certPackage",pack);cJSON_AddItemToObject(pack,"intermediateCert",cJSON_DetachItemFromObjectCaseSensitive(b,"intermediateCert"));cJSON_AddItemToObject(pack,"userCert",cJSON_DetachItemFromObjectCaseSensitive(b,"userCert"));cJSON_Delete(b);b=NULL;cJSON_AddStringToObject(payload,"targetCallsign",target->call);cJSON_AddNumberToObject(payload,"targetUID",target->uid);cJSON_AddStringToObject(payload,"role","user");cJSON_AddStringToObject(payload,"targetUrl",target->host);cJSON_AddNumberToObject(payload,"targetPort",target->port);cJSON_AddStringToObject(payload,"serverFingerprint",server_fp);cJSON_AddNumberToObject(payload,"timestamp",now);cJSON_AddItemToObject(payload,"proof",proof);cJSON_AddStringToObject(proof,"signature",signature);
 *uid=(uint32_t)cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(u,"subject"),"uid")->valuedouble;char*json=cJSON_PrintBuffered(payload,256,false);cJSON_Delete(payload);cJSON_Delete(b);if(!json)return NULL;size_t len=strlen(json);char*out=malloc((len*8+5)/6+1);if(out)wire_b64((uint8_t*)json,len,out);crypto_wipe(json,len);free(json);return out;
}
static bool discovery_identity(const relay_t*t,char**password,char call[8],uint32_t*uid,char fingerprint[65],char root[44]){
 bool held=xSemaphoreGetMutexHolder(api_lock)==xTaskGetCurrentTaskHandle();if(!held)xSemaphoreTake(api_lock,portMAX_DELAY);
 bool ok=bundle&&certificate_valid&&!identity_restart_pending&&(xEventGroupGetBits(events)&1)&&time(NULL)>1700000000;
 if(ok&&root){cJSON*b=cJSON_Parse(bundle);const char*k=js(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b,"rootCert"),"subject"),"publicKey");ok=k&&strlen(k)==43;if(ok)snprintf(root,44,"%s",k);cJSON_Delete(b);}
 if(ok){unsigned char hash[32];mbedtls_sha256((const unsigned char*)bundle,strlen(bundle),hash,0);for(int i=0;i<32;i++)snprintf(fingerprint+i*2,3,"%02x",hash[i]);snprintf(call,8,"%s",callsign);*uid=0;if(t&&password){*password=mqtt_password(uid,t);ok=*password!=NULL&&*uid!=t->uid;}}
 if(!held){xSemaphoreGive(api_lock);}
 return ok;
}
static void mqtt_worker(void*a){(void)a;esp_mqtt_client_handle_t client=NULL;int64_t next=0;char*active_bundle=NULL;uint32_t active_revision=0;unsigned attempts=0;
 while(1){vTaskDelay(pdMS_TO_TICKS(1000));device_view_t v;device_web_view(&v);if(v.mqtt_online)attempts=0;xSemaphoreTake(api_lock,portMAX_DELAY);bool valid=bundle&&!identity_restart_pending&&!auto_connection_blocked&&time(NULL)>1700000000;if(valid){identity_state();valid=certificate_valid;}relay_t target;uint32_t revision;relays_current(&target,&revision);bool changed=active_bundle&&(!bundle||strcmp(active_bundle,bundle)||revision!=active_revision);
 if(changed){next=0;attempts=0;}if(client&&(!v.online||!valid||changed||(atomic_load(&mqtt_phase)==LINK_FAILED&&esp_timer_get_time()>next))){ESP_LOGW(TAG,"MQTT recycle wifi=%d valid=%d changed=%d phase=%d",v.online,valid,changed,atomic_load(&mqtt_phase));voice_connected(false);xSemaphoreTake(mqtt_lock,portMAX_DELAY);active_mqtt=NULL;esp_mqtt_client_stop(client);esp_mqtt_client_destroy(client);client=NULL;xSemaphoreGive(mqtt_lock);free(active_bundle);active_bundle=NULL;relay_msg("等待连接测试台",false);}
 if(!client){if(!v.online)relay_msg("等待 Wi-Fi 连接",false);else if(!bundle)relay_msg("等待导入设备证书",false);else if(time(NULL)<1700000000)relay_msg("等待网络校时",false);else if(!valid)relay_msg("证书无效，无法接入",false);else if(esp_timer_get_time()>next){uint32_t uid=0;char*password=mqtt_password(&uid,&target);if(password&&uid==target.uid){crypto_wipe(password,strlen(password));free(password);password=NULL;relay_msg("设备与服务器 UID 冲突",false);}if(password){char id[48],uri[100];snprintf(uri,sizeof(uri),"mqtt://%s:%d",target.host,target.port);snprintf(id,sizeof(id),"M5-%lu",(unsigned long)uid);esp_mqtt_client_config_t cfg={.broker.address.uri=uri,.credentials.username=callsign,.credentials.client_id=id,.credentials.authentication.password=password,.network.disable_auto_reconnect=true,.session.keepalive=30,.buffer.size=4096,.outbox.limit=1600,.task.stack_size=8192};client=esp_mqtt_client_init(&cfg);crypto_wipe(password,strlen(password));free(password);if(client){active_revision=revision;voice_identity(uid,callsign,target.uid);xSemaphoreTake(mqtt_lock,portMAX_DELAY);active_mqtt=client;xSemaphoreGive(mqtt_lock);active_bundle=strdup(bundle);esp_mqtt_client_register_event(client,ESP_EVENT_ANY_ID,mqtt_event,NULL);relay_msg("正在认证并连接测试台",false);atomic_store(&mqtt_phase,LINK_CONNECTING);mqtt_target=target;if(esp_mqtt_client_start(client)!=ESP_OK){atomic_store(&mqtt_phase,LINK_FAILED);xSemaphoreTake(mqtt_lock,portMAX_DELAY);active_mqtt=NULL;esp_mqtt_client_destroy(client);client=NULL;xSemaphoreGive(mqtt_lock);free(active_bundle);active_bundle=NULL;}}}next=esp_timer_get_time()+(attempts==0?20000000:attempts==1?40000000:60000000);if(attempts<3)attempts++;}}
 xSemaphoreGive(api_lock);}
}
static esp_err_t apply_profile_switch(void){
 uint8_t target=0;esp_err_t e=nvs_get_u8(nv,"profile_next",&target);if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;if(e!=ESP_OK)return e;if(target<1||target>10)return ESP_FAIL;
 char extra_cert[16],extra_seed[16];snprintf(extra_cert,sizeof(extra_cert),"cert_p%u",target-3);snprintf(extra_seed,sizeof(extra_seed),"seed_p%u",target-3);const char*cert_key=target>2?extra_cert:target==1?"cert_bh":"cert_bi";const char*seed_key=target>2?extra_seed:target==1?"seed_bh":"seed_bi";size_t n=0;e=nvs_get_str(nv,cert_key,NULL,&n);if(e!=ESP_OK||n<2||n>6145)return ESP_FAIL;char*raw=malloc(n);if(!raw)return ESP_ERR_NO_MEM;e=nvs_get_str(nv,cert_key,raw,&n);
 uint8_t seed[32],secret[64],pub[32];size_t size=32;if(e==ESP_OK)e=nvs_get_blob(nv,seed_key,seed,&size);if(e!=ESP_OK||size!=32){free(raw);crypto_wipe(seed,32);return ESP_FAIL;}crypto_ed25519_key_pair(secret,pub,seed);crypto_wipe(secret,64);
 cJSON*b=cJSON_Parse(raw);const char*call=js(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(b,"userCert"),"subject"),"callsign");const char*error=wire_validate_bundle(b,pub,call,time(NULL));if(error){cJSON_Delete(b);free(raw);return ESP_FAIL;}
 // Both immutable profile slots and the durable target marker survive this
 // cleanup. Remove duplicate strings BEFORE replacing the active pair: NVS
 // strings require contiguous entries and cannot overwrite in place.
 e=nvs_erase_key(nv,"cert_backup");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;
 if(e==ESP_OK)e=nvs_commit(nv);
 if(e==ESP_OK){e=nvs_erase_key(nv,"cert");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;}
 if(e==ESP_OK)e=nvs_commit(nv);
 size=32;if(e==ESP_OK)e=nvs_get_blob(nv,seed_key,seed,&size);if(e==ESP_OK)e=nvs_set_blob(nv,"seed",seed,32);crypto_wipe(seed,32);if(e==ESP_OK)e=nvs_set_str(nv,"callsign",call);if(e==ESP_OK)e=nvs_set_str(nv,"cert",raw);if(e==ESP_OK)e=nvs_set_u8(nv,"relay_notice",1);if(e==ESP_OK){e=nvs_erase_key(nv,"enroll_req");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;}if(e==ESP_OK)e=nvs_commit(nv);if(e==ESP_OK)e=nvs_erase_key(nv,"profile_next");if(e==ESP_OK)e=nvs_commit(nv);cJSON_Delete(b);free(raw);return e;
}
esp_err_t device_web_start(void){
 lock=xSemaphoreCreateMutex();mqtt_lock=xSemaphoreCreateMutex();api_lock=xSemaphoreCreateMutex();events=xEventGroupCreate();jobs=xQueueCreate(1,sizeof(wifi_config_t));if(!lock||!mqtt_lock||!api_lock||!events||!jobs)return ESP_ERR_NO_MEM;
 ESP_RETURN_ON_ERROR(nvs_flash_init(),TAG,"NVS init failed");ESP_RETURN_ON_ERROR(nvs_open("m5fmo_v1",NVS_READWRITE,&nv),TAG,"NVS open failed");
 if(roots_seed_defaults()!=ESP_OK)ESP_LOGW(TAG,"Public root preset storage full; existing identities preserved");
 ESP_RETURN_ON_ERROR(identity_apply_pending(nv),TAG,"identity transition incomplete");
 esp_err_t migration_result=enroll_migrate();
 if(migration_result!=ESP_OK)ESP_LOGE(TAG,"Legacy enrollment retained; profile switch deferred: %s",esp_err_to_name(migration_result));
 // Persist default selection intent before the identity transaction clears its marker.
 uint8_t switch_target=0;
 if(nvs_get_u8(nv,"profile_next",&switch_target)!=ESP_OK)nvs_get_u8(nv,"empty_next",&switch_target);
 if(migration_result==ESP_OK&&(switch_target==1||switch_target==2)){
  migration_result=nvs_set_u8(nv,"relay_default",switch_target);
  if(migration_result==ESP_OK)migration_result=nvs_commit(nv);
 }
 esp_err_t empty_result=migration_result==ESP_OK?apply_empty_profile():migration_result;
 esp_err_t profile_result=empty_result==ESP_OK?apply_profile_switch():empty_result;
 nvs_get_u8(nv,"selected_ca",&selected_ca);
 if(profile_result!=ESP_OK)ESP_LOGE(TAG,"Profile switch pending; management recovery remains available: %s",esp_err_to_name(profile_result));
 size_t rn=sizeof(remembered);if(nvs_get_blob(nv,"web_remember",remembered,&rn)!=ESP_OK||rn!=sizeof(remembered))memset(remembered,0,sizeof(remembered));for(int i=0;i<4;i++)remembered[i][43]=0;
 uint8_t seed[32],random[8],session[32];size_t n=32;
 bootloader_random_enable();esp_fill_random(random,sizeof(random));for(size_t i=0;i<sizeof(random);i++)while(random[i]>=250)esp_fill_random(&random[i],1);esp_fill_random(session,sizeof(session));esp_err_t e=nvs_get_blob(nv,"seed",seed,&n);if(e==ESP_ERR_NVS_NOT_FOUND){esp_fill_random(seed,32);e=nvs_set_blob(nv,"seed",seed,32);if(e==ESP_OK)e=nvs_commit(nv);}bootloader_random_disable();
 if(e!=ESP_OK||n!=32){crypto_wipe(seed,32);return ESP_FAIL;}crypto_ed25519_key_pair(sk,pk,seed);crypto_wipe(seed,32);wire_b64(session,32,token);crypto_wipe(session,32);
 uint8_t mac[6];esp_read_mac(mac,ESP_MAC_WIFI_STA);snprintf(apname,sizeof(apname),"M5-FMO-%02X%02X%02X",mac[3],mac[4],mac[5]);snprintf(view.ap,sizeof(view.ap),"%s",apname);
 size_t code_len=sizeof(code);bool stored_code=nvs_get_str(nv,"web_code",code,&code_len)==ESP_OK&&code_len==9&&code[8]==0;for(int i=0;stored_code&&i<8;i++)if(code[i]<'0'||code[i]>'9')stored_code=false;if(!stored_code){for(int i=0;i<8;i++)code[i]='0'+random[i]%10;code[8]=0;e=nvs_set_str(nv,"web_code",code);if(e==ESP_OK)e=nvs_commit(nv);}crypto_wipe(random,sizeof(random));if(!stored_code&&e!=ESP_OK)return e;
 n=sizeof(callsign);nvs_get_str(nv,"callsign",callsign,&n);if(!wire_callsign(callsign))callsign[0]=0;n=0;if(nvs_get_str(nv,"cert",NULL,&n)==ESP_OK&&n>1&&n<=6145){bundle=malloc(n);if(bundle&&nvs_get_str(nv,"cert",bundle,&n)!=ESP_OK){free(bundle);bundle=NULL;}}
 uint8_t unfinished_empty=0;if(nvs_get_u8(nv,"empty_next",&unfinished_empty)==ESP_OK){free(bundle);bundle=NULL;}
 cJSON*relay_identity=bundle?cJSON_Parse(bundle):NULL;const char*relay_error=NULL;cJSON*relay_root=cJSON_GetObjectItemCaseSensitive(relay_identity,"rootCert");int relay_profile=relay_root?root_slot(relay_root,false,&relay_error):selected_ca;selected_ca=relay_profile;profile_name_for(selected_ca,selected_ca_name,sizeof(selected_ca_name));const char*relay_label=relay_profile==1?"BH1JSS M5":relay_profile==2?"BI4TES / Passport":selected_ca_name;uint8_t relay_hash[32]={0};if(bundle)mbedtls_sha256((const uint8_t*)bundle,strlen(bundle),relay_hash,0);else mbedtls_sha256(pk,32,relay_hash,0);relays_configure(relay_profile,relay_label?relay_label:"尚无设备证书",relay_hash);cJSON_Delete(relay_identity);ESP_RETURN_ON_ERROR(relays_init(),TAG,"relays");
 uint8_t default_target=0;
 if(profile_result==ESP_OK&&nvs_get_u8(nv,"relay_default",&default_target)==ESP_OK){
  if(default_target!=relay_profile||relays_select_default()){
   if(nvs_erase_key(nv,"relay_default")==ESP_OK)nvs_commit(nv);
  }else ESP_LOGW(TAG,"Default relay selection pending; saved list preserved");
 }

 uint32_t enabled=0;if(nvs_get_u32(nv,"auto_ca",&enabled)==ESP_OK)atomic_store(&auto_ca_mask,enabled&1023u);
 auto_resume_relay();
 n=sizeof(saved);have_saved=nvs_get_blob(nv,"wifi",&saved,&n)==ESP_OK&&n==sizeof(saved)&&saved.sta.ssid[0];ESP_RETURN_ON_ERROR(profiles_load(),TAG,"Wi-Fi profiles");have_saved=profile_get(wifi_bank.preferred,&saved);if(!have_saved)crypto_wipe(&saved,sizeof(saved));snprintf(view.code,sizeof(view.code),"%s",code);snprintf(view.call,sizeof(view.call),"%s",callsign);if(have_saved)snprintf(view.ssid,sizeof(view.ssid),"%.*s",32,saved.sta.ssid);netmsg("可连接设备热点，在网页中配网");identity_state();
 size_t pending_size=0;if(nvs_get_str(nv,"enroll_req",NULL,&pending_size)==ESP_OK)snprintf(enrollment,sizeof(enrollment),"待处理申请已恢复，联网后自动领取");else if(bundle)snprintf(enrollment,sizeof(enrollment),"设备证书已安装，无需重复申请");
 if(profile_result!=ESP_OK)snprintf(enrollment,sizeof(enrollment),"身份切换未完成，请在证书页重新选择身份；两套备份保留");
#ifdef CONFIG_FMO_BOARD_XINGZHI
 usb_serial_jtag_driver_config_t usb_cfg={.rx_buffer_size=12288,.tx_buffer_size=2048};
 ESP_RETURN_ON_ERROR(usb_serial_jtag_driver_install(&usb_cfg),TAG,"USB serial");
 usb_serial_jtag_vfs_use_driver();
#else
 uart_config_t u={.baud_rate=115200,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};ESP_RETURN_ON_ERROR(uart_param_config(UART_NUM_0,&u),TAG,"uart config");ESP_RETURN_ON_ERROR(uart_driver_install(UART_NUM_0,4096,0,0,NULL,0),TAG,"uart driver");
#endif
 if(xTaskCreate(usb_task,"usb_admin",14336,NULL,4,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
 if(xTaskCreate(worker,"wifi_setup",8192,NULL,4,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
 uint8_t failures=0;nvs_get_u8(nv,"net_failures",&failures);if(have_saved&&failures<2){xEventGroupSetBits(events,2);xQueueSend(jobs,&saved,0);}else if(failures>=2)netmsg("Wi-Fi 连续启动失败，请在网页重连已保存网络");
 ESP_RETURN_ON_ERROR(voice_init(publish_voice),TAG,"voice");
 if(xTaskCreate(mqtt_worker,"fmo_connect",8192,NULL,3,NULL)!=pdPASS)return ESP_ERR_NO_MEM;
 ESP_RETURN_ON_ERROR(discovery_start(discovery_identity,api_lock),TAG,"discovery");
 uint8_t refresh_identity=0;if(nvs_get_u8(nv,"relay_notice",&refresh_identity)==ESP_OK&&refresh_identity){discovery_identity_changed();}
 ESP_LOGI(TAG,"USB READY");return ESP_OK;
}
