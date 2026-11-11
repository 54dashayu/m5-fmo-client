#include "relays.h"
#include "utf8_name.h"
#include "cert_wire.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
static bool access_verified[RELAY_MAX];
static int connection_result[RELAY_MAX];static time_t connection_time[RELAY_MAX];
static bool temporary_active;static relay_t temporary;
static relay_t rows[RELAY_MAX];static int count=1,selected=0;static uint32_t revision=1;static SemaphoreHandle_t lock;static nvs_handle_t nv;
static const char*str(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static bool parse(const cJSON*j,relay_t*r){const char*n=str(j,"name"),*c=str(j,"callsign"),*h=str(j,"host"),*f=str(j,"fingerprint");const cJSON*u=cJSON_GetObjectItemCaseSensitive(j,"uid"),*p=cJSON_GetObjectItemCaseSensitive(j,"port");uint8_t fp[32];if(!n||!strlen(n)||strlen(n)>64||!name_utf8_valid(n)||!wire_callsign(c)||!h||!strlen(h)||strlen(h)>63||!wire_unb64(f,fp,32)||!cJSON_IsNumber(u)||u->valuedouble<1||u->valuedouble>4294967295.0||floor(u->valuedouble)!=u->valuedouble||!cJSON_IsNumber(p)||p->valuedouble<1||p->valuedouble>65535||floor(p->valuedouble)!=p->valuedouble)return false;for(const char*q=h;*q;q++)if(!((*q>='a'&&*q<='z')||(*q>='A'&&*q<='Z')||(*q>='0'&&*q<='9')||*q=='.'||*q=='-'))return false;for(const char*q=n;*q;q++)if((unsigned char)*q<32)return false;memset(r,0,sizeof(*r));snprintf(r->name,sizeof(r->name),"%s",n);snprintf(r->call,sizeof(r->call),"%s",c);snprintf(r->host,sizeof(r->host),"%s",h);snprintf(r->fingerprint,sizeof(r->fingerprint),"%s",f);r->uid=u->valuedouble;r->port=p->valuedouble;return true;}
static cJSON*array(void){cJSON*a=cJSON_CreateArray();for(int i=0;i<count;i++){cJSON*j=cJSON_CreateObject();cJSON_AddStringToObject(j,"name",rows[i].name);cJSON_AddStringToObject(j,"callsign",rows[i].call);cJSON_AddStringToObject(j,"host",rows[i].host);cJSON_AddStringToObject(j,"fingerprint",rows[i].fingerprint);cJSON_AddNumberToObject(j,"uid",rows[i].uid);cJSON_AddNumberToObject(j,"port",rows[i].port);cJSON_AddBoolToObject(j,"accessVerified",access_verified[i]);cJSON_AddNumberToObject(j,"connectionResult",connection_result[i]);cJSON_AddNumberToObject(j,"connectionTime",connection_time[i]);cJSON_AddItemToArray(a,j);}return a;}
static bool persist(void){cJSON*j=cJSON_CreateObject();if(!j)return false;cJSON_AddItemToObject(j,"relays",array());cJSON_AddNumberToObject(j,"selected",selected);char*s=cJSON_PrintUnformatted(j);cJSON_Delete(j);bool ok=s&&nvs_set_str(nv,"list",s)==ESP_OK&&nvs_commit(nv)==ESP_OK;free(s);return ok;}
esp_err_t relays_init(void){lock=xSemaphoreCreateMutex();if(!lock)return ESP_ERR_NO_MEM;esp_err_t e=nvs_open("m5relays",NVS_READWRITE,&nv);if(e!=ESP_OK)return e;rows[0]=(relay_t){.name="机婶婶的测试台",.call="BH1JSS",.host="120.27.244.89",.fingerprint="U2IJZO1ouwf9i5gdU8WC9TU4_hOGAfZo0FxeJyy8JzI",.uid=1791,.port=28883};bool bank=false;size_t n=0;if(nvs_get_str(nv,"list",NULL,&n)==ESP_OK&&n<4096){char*buf=malloc(n);if(buf&&nvs_get_str(nv,"list",buf,&n)==ESP_OK){cJSON*doc=cJSON_Parse(buf);bank=cJSON_IsObject(doc);cJSON*a=bank?cJSON_GetObjectItemCaseSensitive(doc,"relays"):doc;int size=cJSON_GetArraySize(a);relay_t temp[RELAY_MAX];bool ok=cJSON_IsArray(a)&&size>0&&size<=RELAY_MAX;for(int i=0;ok&&i<size;i++)ok=parse(cJSON_GetArrayItem(a,i),&temp[i]);if(ok){memcpy(rows,temp,size*sizeof(relay_t));count=size;if(bank){cJSON*i=cJSON_GetObjectItemCaseSensitive(doc,"selected");if(cJSON_IsNumber(i)&&i->valueint>=0&&i->valueint<count)selected=i->valueint;}}cJSON_Delete(doc);}free(buf);}uint8_t index=0;nvs_get_u8(nv,"selected",&index);if(!bank&&index<count)selected=index;for(int i=0;i<count;i++)if(rows[i].uid==1791&&!strcmp(rows[i].call,"BH1JSS")&&!strcmp(rows[i].name,"鸡婶婶的测试台")){snprintf(rows[i].name,sizeof(rows[i].name),"机婶婶的测试台");persist();}return ESP_OK;}
int relays_count(void){xSemaphoreTake(lock,portMAX_DELAY);int n=count;xSemaphoreGive(lock);return n;}
bool relays_get(int i,relay_t*out){xSemaphoreTake(lock,portMAX_DELAY);bool ok=i>=0&&i<count;if(ok)*out=rows[i];xSemaphoreGive(lock);return ok;}
int relays_current(relay_t*out,uint32_t*rev){xSemaphoreTake(lock,portMAX_DELAY);if(out)*out=temporary_active?temporary:rows[selected];if(rev)*rev=revision;int i=temporary_active?-1:selected;xSemaphoreGive(lock);return i;}
bool relays_select(int i){xSemaphoreTake(lock,portMAX_DELAY);bool ok=i>=0&&i<count;if(ok&&(i!=selected||temporary_active)){int old=selected;selected=i;ok=persist();if(ok){temporary_active=false;revision++;}else selected=old;}xSemaphoreGive(lock);return ok;}
void relays_use_temporary(const relay_t*r){xSemaphoreTake(lock,portMAX_DELAY);temporary=*r;temporary_active=true;revision++;xSemaphoreGive(lock);}
cJSON*relays_json(void){xSemaphoreTake(lock,portMAX_DELAY);cJSON*j=cJSON_CreateObject();cJSON_AddItemToObject(j,"relays",array());cJSON_AddNumberToObject(j,"selected",temporary_active?-1:selected);cJSON_AddBoolToObject(j,"temporary",temporary_active);xSemaphoreGive(lock);return j;}
const char*relays_add(const cJSON*j){relay_t r;if(!parse(j,&r))return "中继信息无效，请核对地址、呼号、UID 和指纹";xSemaphoreTake(lock,portMAX_DELAY);const char*error=NULL;if(count>=RELAY_MAX)error="最多保存四个中继";for(int i=0;!error&&i<count;i++)if(rows[i].uid==r.uid&&!strcmp(rows[i].host,r.host)&&rows[i].port==r.port)error="这个中继已经存在";if(!error){rows[count]=r;access_verified[count]=false;connection_result[count]=0;connection_time[count]=0;count++;if(!persist()){count--;error="保存中继失败";}}xSemaphoreGive(lock);return error;}

const char*relays_update_name(const cJSON*j){relay_t fresh;if(!parse(j,&fresh))return "中继资料无效";xSemaphoreTake(lock,portMAX_DELAY);const char*err="中继身份不匹配，请刷新";for(int i=0;i<count;i++)if(rows[i].uid==fresh.uid&&rows[i].port==fresh.port&&!strcmp(rows[i].call,fresh.call)&&!strcmp(rows[i].host,fresh.host)&&!strcmp(rows[i].fingerprint,fresh.fingerprint)){char old[65];memcpy(old,rows[i].name,sizeof(old));err=NULL;if(strcmp(old,fresh.name)){memcpy(rows[i].name,fresh.name,sizeof(old));if(!persist()){memcpy(rows[i].name,old,sizeof(old));err="名称保存失败";}}break;}xSemaphoreGive(lock);return err;}
void relays_connection(const relay_t*t,bool success){xSemaphoreTake(lock,portMAX_DELAY);for(int i=0;i<count;i++)if(rows[i].uid==t->uid&&rows[i].port==t->port&&!strcmp(rows[i].host,t->host)&&!strcmp(rows[i].fingerprint,t->fingerprint)){if(success)access_verified[i]=true;connection_result[i]=success?1:2;connection_time[i]=time(NULL);}xSemaphoreGive(lock);}
void relays_clear_evidence(void){xSemaphoreTake(lock,portMAX_DELAY);memset(access_verified,0,sizeof(access_verified));memset(connection_result,0,sizeof(connection_result));memset(connection_time,0,sizeof(connection_time));xSemaphoreGive(lock);}

int relays_order(int out[RELAY_MAX]){xSemaphoreTake(lock,portMAX_DELAY);int n=0;for(int rank=0;rank<2;rank++)for(int i=0;i<count;i++)if((access_verified[i]?0:1)==rank)out[n++]=i;xSemaphoreGive(lock);return n;}
const char*relays_remove(int i,const relay_t*expected){xSemaphoreTake(lock,portMAX_DELAY);const char*err=NULL;
if(i<0||i>=count||!expected||rows[i].uid!=expected->uid||rows[i].port!=expected->port||strcmp(rows[i].host,expected->host)||strcmp(rows[i].fingerprint,expected->fingerprint))err="列表已变化，请刷新后重试";
else if(i==selected)err="请先切换到其他中继，再删除当前中继";
else if(count<=1)err="至少保留一个中继";
else{relay_t backup[RELAY_MAX];int results[RELAY_MAX];time_t times[RELAY_MAX];bool verified[RELAY_MAX];memcpy(backup,rows,sizeof(rows));memcpy(results,connection_result,sizeof(results));memcpy(times,connection_time,sizeof(times));memcpy(verified,access_verified,sizeof(verified));int old_selected=selected,old_count=count;
for(int k=i;k<count-1;k++){rows[k]=rows[k+1];connection_result[k]=connection_result[k+1];connection_time[k]=connection_time[k+1];access_verified[k]=access_verified[k+1];}count--;if(selected>i)selected--;
if(!persist()){memcpy(rows,backup,sizeof(rows));memcpy(connection_result,results,sizeof(results));memcpy(connection_time,times,sizeof(times));memcpy(access_verified,verified,sizeof(verified));selected=old_selected;count=old_count;err="保存失败，未删除中继";}
}xSemaphoreGive(lock);return err;}
