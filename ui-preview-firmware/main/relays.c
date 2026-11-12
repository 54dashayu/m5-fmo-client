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
#include <limits.h>
// Only small runtime evidence/index metadata is resident; records live in NVS.
typedef struct {uint32_t id;bool verified;int result;time_t at;} entry_t;
static entry_t *entries;
static int count,selected;
static uint32_t revision=1;
static SemaphoreHandle_t lock;
static nvs_handle_t nv;
static int profile_id;
static char profile_name[97]="未选择体系",index_key[16]="index";
static uint8_t identity_hash[32];
void relays_configure(int id,const char*name,const uint8_t hash[32]){profile_id=id;snprintf(profile_name,sizeof(profile_name),"%s",name);memcpy(identity_hash,hash,32);snprintf(index_key,sizeof(index_key),"ca%dindex",id);}
const char*relays_profile_name(void){return profile_name;}
typedef struct {uint8_t identity[32];uint32_t id;int64_t at;int32_t result;uint8_t verified;} proof_t;
static void proof_key(uint32_t id,char key[16]){snprintf(key,16,"e%02x%08lx",profile_id,(unsigned long)id);}
static void save_proof(const entry_t*e){char key[16];proof_key(e->id,key);proof_t p={.id=e->id,.at=e->at,.result=e->result,.verified=e->verified};memcpy(p.identity,identity_hash,32);if(nvs_set_blob(nv,key,&p,sizeof(p))==ESP_OK)nvs_commit(nv);}
static void load_proof(entry_t*e){char key[16];proof_key(e->id,key);proof_t p;size_t n=sizeof(p);if(nvs_get_blob(nv,key,&p,&n)==ESP_OK&&n==sizeof(p)&&p.id==e->id&&!memcmp(p.identity,identity_hash,32)){e->verified=p.verified;e->result=p.result;e->at=p.at;}}

static bool temporary_active;
static relay_t temporary,current_record;
static struct {uint32_t id;relay_t value;} cache[4];
static unsigned cache_next;
static void invalidate(uint32_t id){for(unsigned i=0;i<4;i++)if(cache[i].id==id)cache[i].id=0;}
#define INDEX_MAGIC 0x524c5932u
static const char*str(const cJSON*j,const char*k){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return cJSON_IsString(v)?v->valuestring:NULL;}
static bool parse(const cJSON*j,relay_t*r){const char*n=str(j,"name"),*c=str(j,"callsign"),*h=str(j,"host"),*f=str(j,"fingerprint");const cJSON*u=cJSON_GetObjectItemCaseSensitive(j,"uid"),*p=cJSON_GetObjectItemCaseSensitive(j,"port");uint8_t fp[32];if(!n||!strlen(n)||strlen(n)>64||!name_utf8_valid(n)||!wire_callsign(c)||!h||!strlen(h)||strlen(h)>63||!wire_unb64(f,fp,32)||!cJSON_IsNumber(u)||u->valuedouble<1||u->valuedouble>4294967295.0||floor(u->valuedouble)!=u->valuedouble||!cJSON_IsNumber(p)||p->valuedouble<1||p->valuedouble>65535||floor(p->valuedouble)!=p->valuedouble)return false;for(const char*q=h;*q;q++)if(!((*q>='a'&&*q<='z')||(*q>='A'&&*q<='Z')||(*q>='0'&&*q<='9')||*q=='.'||*q=='-'))return false;for(const char*q=n;*q;q++)if((unsigned char)*q<32)return false;memset(r,0,sizeof(*r));snprintf(r->name,sizeof(r->name),"%s",n);snprintf(r->call,sizeof(r->call),"%s",c);snprintf(r->host,sizeof(r->host),"%s",h);snprintf(r->fingerprint,sizeof(r->fingerprint),"%s",f);r->uid=u->valuedouble;r->port=p->valuedouble;return true;}

static bool same(const relay_t*a,const relay_t*b){return a->uid==b->uid&&a->port==b->port&&!strcmp(a->call,b->call)&&!strcmp(a->host,b->host)&&!strcmp(a->fingerprint,b->fingerprint);}
static void record_key(uint32_t id,char key[12]){snprintf(key,12,"r%08lx",(unsigned long)id);}
static bool load_record(uint32_t id,relay_t*out){for(unsigned i=0;i<4;i++)if(id&&cache[i].id==id){*out=cache[i].value;return true;}char key[12];record_key(id,key);size_t n=sizeof(*out);bool ok=nvs_get_blob(nv,key,out,&n)==ESP_OK&&n==sizeof(*out)&&memchr(out->name,0,sizeof(out->name))&&memchr(out->call,0,sizeof(out->call))&&memchr(out->host,0,sizeof(out->host))&&memchr(out->fingerprint,0,sizeof(out->fingerprint))&&out->uid&&out->port>0&&out->port<=65535;if(ok){unsigned slot=cache_next++%4;cache[slot].id=id;cache[slot].value=*out;}return ok;}

static bool store_record(uint32_t id,const relay_t*r){invalidate(id);char key[12];record_key(id,key);return nvs_set_blob(nv,key,r,sizeof(*r))==ESP_OK&&nvs_commit(nv)==ESP_OK;}
static bool save_index(void){size_t n=((size_t)count+3)*sizeof(uint32_t);uint32_t*b=malloc(n);if(!b)return false;b[0]=INDEX_MAGIC;b[1]=(uint32_t)count;b[2]=(uint32_t)selected;for(int i=0;i<count;i++)b[i+3]=entries[i].id;bool ok=nvs_set_blob(nv,index_key,b,n)==ESP_OK&&nvs_commit(nv)==ESP_OK;free(b);return ok;}
static const char*append_record(const relay_t*r){
 uint32_t id=1;for(int i=0;i<count;i++){relay_t old;if(!load_record(entries[i].id,&old))return "中继记录读取失败";if(old.uid==r->uid&&old.port==r->port&&!strcmp(old.host,r->host))return "这个中继已经存在";if(entries[i].id>=id)id=entries[i].id+1;}
 // Record IDs are global; other identity banks may retain records not in this list.
 while(id){char key[12];record_key(id,key);size_t n=0;esp_err_t e=nvs_get_blob(nv,key,NULL,&n);if(e==ESP_ERR_NVS_NOT_FOUND)break;if(e!=ESP_OK)return "中继记录读取失败";id++;}
 if(!id||count==INT_MAX)return "中继索引空间不足";
 entry_t*next=realloc(entries,((size_t)count+1)*sizeof(*entries));if(!next)return "内存暂不足，请稍后重试";entries=next;
 if(!store_record(id,r))return "保存失败，设备存储空间可能不足";
 entries[count++]=(entry_t){.id=id};if(!save_index()){count--;save_index();return "保存失败，原列表已保留";}return NULL;
}
// Explicit profile switches choose these public connection records, never grant trust.
static const relay_t*profile_default(void){
 static const relay_t bh={.name="机婶婶的测试台",.call="BH1JSS",.host="120.27.244.89",.fingerprint="U2IJZO1ouwf9i5gdU8WC9TU4_hOGAfZo0FxeJyy8JzI",.uid=1791,.port=28883};
 static const relay_t bi={.name="AI PASSPORT互通",.call="BI4TES",.host="115.29.167.110",.fingerprint="yMjgJ28IZngKI5DiwqPFp3S5M7OdY_CjiWD5lQr3vgc",.uid=1845,.port=1883};
 return profile_id==2?&bi:profile_id==1?&bh:NULL;
}
bool relays_select_default(void){
 const relay_t*target=profile_default();if(!target)return true;
 xSemaphoreTake(lock,portMAX_DELAY);int found=-1;
 for(int i=0;i<count;i++){relay_t r;if(load_record(entries[i].id,&r)&&r.uid==target->uid&&!strcmp(r.call,target->call)){found=i;break;}}
 // Preserve updated saved endpoint/name/fingerprint; only seed when absent.
 if(found<0&&!append_record(target))found=count-1;
 xSemaphoreGive(lock);return found>=0&&relays_select(found);
}
static int ordered_at(int position){int pos=0;for(int rank=0;rank<2;rank++)for(int i=0;i<count;i++)if((entries[i].verified?0:1)==rank){if(pos++==position)return i;}return -1;}
int relays_order_at(int position){xSemaphoreTake(lock,portMAX_DELAY);int i=ordered_at(position);xSemaphoreGive(lock);return i;}
int relays_order_position(int index){xSemaphoreTake(lock,portMAX_DELAY);int pos=count;if(index>=0&&index<count){pos=0;for(int i=0;i<count;i++)if((entries[i].verified&&!entries[index].verified)||(entries[i].verified==entries[index].verified&&i<index))pos++;}xSemaphoreGive(lock);return pos;}
esp_err_t relays_init(void){
 memset(cache,0,sizeof(cache));cache_next=0;lock=xSemaphoreCreateMutex();if(!lock)return ESP_ERR_NO_MEM;
 esp_err_t e=nvs_open("m5relay2",NVS_READWRITE,&nv);if(e!=ESP_OK)return e;
 size_t size=0;bool migrate=false;e=nvs_get_blob(nv,index_key,NULL,&size);
 // Upgrade copies only addresses from the pre-bank list, never trust evidence.
 const char*read_key=index_key;
 if(e==ESP_ERR_NVS_NOT_FOUND&&profile_id){e=nvs_get_blob(nv,"index",NULL,&size);if(e==ESP_OK){read_key="index";migrate=true;}}
 if(e==ESP_OK){
  if(size<16||size%4)return ESP_ERR_INVALID_STATE;
  uint32_t*b=malloc(size);if(!b)return ESP_ERR_NO_MEM;
  bool ok=nvs_get_blob(nv,read_key,b,&size)==ESP_OK&&b[0]==INDEX_MAGIC&&b[1]>0&&b[1]<INT_MAX&&size==((size_t)b[1]+3)*4&&b[2]<b[1];
  if(!ok){free(b);return ESP_ERR_INVALID_STATE;}
  count=(int)b[1];selected=(int)b[2];entries=calloc((size_t)count,sizeof(*entries));if(!entries){free(b);return ESP_ERR_NO_MEM;}
  relay_t r;for(int i=0;i<count;i++){entries[i].id=b[i+3];if(!load_record(entries[i].id,&r)){free(b);return ESP_ERR_INVALID_STATE;}for(int k=0;k<i;k++)if(entries[k].id==entries[i].id){free(b);return ESP_ERR_INVALID_STATE;}}
  free(b);if(migrate&&!save_index())return ESP_FAIL;for(int i=0;i<count;i++)load_proof(&entries[i]);return load_record(entries[selected].id,&current_record)?ESP_OK:ESP_ERR_INVALID_STATE;
 }
 if(e!=ESP_ERR_NVS_NOT_FOUND)return e;
 // Migrate old JSON once; retain its original namespace for rollback/recovery.
 nvs_handle_t legacy;bool imported=false;
 e=nvs_open("m5relays",NVS_READONLY,&legacy);
 if(e==ESP_OK){size_t n=0;esp_err_t found=nvs_get_str(legacy,"list",NULL,&n);if(found==ESP_OK){
  char*buf=malloc(n);if(!buf){nvs_close(legacy);return ESP_ERR_NO_MEM;}
  if(nvs_get_str(legacy,"list",buf,&n)!=ESP_OK){free(buf);nvs_close(legacy);return ESP_ERR_INVALID_STATE;}
  cJSON*doc=cJSON_Parse(buf);free(buf);bool bank=cJSON_IsObject(doc);cJSON*a=bank?cJSON_GetObjectItemCaseSensitive(doc,"relays"):doc;int total=cJSON_GetArraySize(a);bool ok=cJSON_IsArray(a)&&total>0;
  entries=ok?calloc((size_t)total,sizeof(*entries)):NULL;if(!entries)ok=false;
  for(int i=0;ok&&i<total;i++){relay_t r;ok=parse(cJSON_GetArrayItem(a,i),&r);if(ok){entries[i].id=(uint32_t)i+1;ok=store_record(entries[i].id,&r);}}
  if(ok){count=total;selected=0;uint8_t old_selected=0;nvs_get_u8(legacy,"selected",&old_selected);if(old_selected<count)selected=old_selected;if(bank){const cJSON*v=cJSON_GetObjectItemCaseSensitive(doc,"selected");if(cJSON_IsNumber(v)&&v->valueint>=0&&v->valueint<count)selected=v->valueint;}ok=save_index();}
  cJSON_Delete(doc);if(!ok){nvs_close(legacy);return ESP_FAIL;}imported=true;
 }else if(found!=ESP_ERR_NVS_NOT_FOUND){nvs_close(legacy);return found;}nvs_close(legacy);
 }else if(e!=ESP_ERR_NVS_NOT_FOUND)return e;
 if(!imported){relay_t first={.name="机婶婶的测试台",.call="BH1JSS",.host="120.27.244.89",.fingerprint="U2IJZO1ouwf9i5gdU8WC9TU4_hOGAfZo0FxeJyy8JzI",.uid=1791,.port=28883};if(append_record(&first))return ESP_FAIL;}
 return load_record(entries[selected].id,&current_record)?ESP_OK:ESP_ERR_INVALID_STATE;
}
int relays_count(void){xSemaphoreTake(lock,portMAX_DELAY);int n=count;xSemaphoreGive(lock);return n;}
bool relays_get(int i,relay_t*out){xSemaphoreTake(lock,portMAX_DELAY);bool ok=i>=0&&i<count&&load_record(entries[i].id,out);xSemaphoreGive(lock);return ok;}
int relays_current(relay_t*out,uint32_t*rev){xSemaphoreTake(lock,portMAX_DELAY);if(out)*out=temporary_active?temporary:current_record;if(rev)*rev=revision;int i=temporary_active?-1:selected;xSemaphoreGive(lock);return i;}
bool relays_select(int i){xSemaphoreTake(lock,portMAX_DELAY);relay_t next;bool ok=i>=0&&i<count&&load_record(entries[i].id,&next);if(ok&&(i!=selected||temporary_active)){int old=selected;selected=i;ok=save_index();if(ok){temporary_active=false;current_record=next;revision++;}else{selected=old;save_index();}}xSemaphoreGive(lock);return ok;}
void relays_use_temporary(const relay_t*r){xSemaphoreTake(lock,portMAX_DELAY);temporary=*r;temporary_active=true;revision++;xSemaphoreGive(lock);}
static cJSON*row_json(int i,const relay_t*r){cJSON*j=cJSON_CreateObject();cJSON_AddStringToObject(j,"name",r->name);cJSON_AddStringToObject(j,"callsign",r->call);cJSON_AddStringToObject(j,"host",r->host);cJSON_AddStringToObject(j,"fingerprint",r->fingerprint);cJSON_AddNumberToObject(j,"uid",r->uid);cJSON_AddNumberToObject(j,"port",r->port);cJSON_AddNumberToObject(j,"index",i);cJSON_AddBoolToObject(j,"accessVerified",entries[i].verified);cJSON_AddNumberToObject(j,"connectionResult",entries[i].result);cJSON_AddNumberToObject(j,"connectionTime",entries[i].at);return j;}
cJSON*relays_json_page(int offset){xSemaphoreTake(lock,portMAX_DELAY);if(offset<0)offset=0;if(offset>=count)offset=(count-1)/RELAY_PAGE_SIZE*RELAY_PAGE_SIZE;cJSON*j=cJSON_CreateObject(),*a=cJSON_AddArrayToObject(j,"relays");for(int p=offset;p<count&&p<offset+RELAY_PAGE_SIZE;p++){int i=ordered_at(p);relay_t r;if(load_record(entries[i].id,&r))cJSON_AddItemToArray(a,row_json(i,&r));}cJSON_AddNumberToObject(j,"profileId",profile_id);cJSON_AddStringToObject(j,"profileName",profile_name);cJSON_AddNumberToObject(j,"selected",temporary_active?-1:selected);cJSON_AddBoolToObject(j,"temporary",temporary_active);cJSON_AddNumberToObject(j,"total",count);cJSON_AddNumberToObject(j,"offset",offset);cJSON_AddNumberToObject(j,"pageSize",RELAY_PAGE_SIZE);xSemaphoreGive(lock);return j;}
cJSON*relays_json(void){return relays_json_page(0);}
const char*relays_add(const cJSON*j){relay_t r;if(!parse(j,&r))return "中继信息无效，请核对地址、呼号、UID 和指纹";xSemaphoreTake(lock,portMAX_DELAY);const char*error=append_record(&r);xSemaphoreGive(lock);return error;}
const char*relays_update_name(const cJSON*j){relay_t fresh;if(!parse(j,&fresh))return "中继资料无效";xSemaphoreTake(lock,portMAX_DELAY);const char*error="中继身份不匹配，请刷新";for(int i=0;i<count;i++){relay_t old;if(load_record(entries[i].id,&old)&&same(&old,&fresh)){error=NULL;if(strcmp(old.name,fresh.name)){if(!store_record(entries[i].id,&fresh)){store_record(entries[i].id,&old);error="名称保存失败";}else if(i==selected)current_record=fresh;}break;}}xSemaphoreGive(lock);return error;}
void relays_connection(const relay_t*t,bool success){xSemaphoreTake(lock,portMAX_DELAY);for(int i=0;i<count;i++){relay_t r;if(load_record(entries[i].id,&r)&&same(&r,t)){bool changed=entries[i].result!=(success?1:2)||(success&&!entries[i].verified);if(success)entries[i].verified=true;entries[i].result=success?1:2;entries[i].at=time(NULL);if(changed)save_proof(&entries[i]);}}xSemaphoreGive(lock);}
bool relays_verified(const relay_t*t){xSemaphoreTake(lock,portMAX_DELAY);bool ok=false;for(int i=0;i<count;i++)if(entries[i].verified){relay_t r;if(load_record(entries[i].id,&r)&&same(&r,t)){ok=true;break;}}xSemaphoreGive(lock);return ok;}
void relays_clear_evidence(void){xSemaphoreTake(lock,portMAX_DELAY);for(int i=0;i<count;i++){entries[i].verified=false;entries[i].result=0;entries[i].at=0;save_proof(&entries[i]);}xSemaphoreGive(lock);}
const char*relays_remove(int i,const relay_t*expected){xSemaphoreTake(lock,portMAX_DELAY);const char*error=NULL;relay_t r;
 if(i<0||i>=count||!expected||!load_record(entries[i].id,&r)||r.uid!=expected->uid||r.port!=expected->port||strcmp(r.host,expected->host)||strcmp(r.fingerprint,expected->fingerprint))error="列表已变化，请刷新后重试";
 else if(i==selected)error="请先切换到其他中继，再删除当前中继";
 else if(count<=1)error="至少保留一个中继";
 else{entry_t removed=entries[i];int old_selected=selected;memmove(entries+i,entries+i+1,(count-i-1)*sizeof(*entries));count--;if(selected>i)selected--;
  if(!save_index()){memmove(entries+i+1,entries+i,(count-i)*sizeof(*entries));entries[i]=removed;count++;selected=old_selected;save_index();error="保存失败，未删除中继";}else{char key[16];proof_key(removed.id,key);nvs_erase_key(nv,key);nvs_commit(nv);/* Shared address retained for other identity banks and upgrade rollback. */}
 }xSemaphoreGive(lock);return error;
}
