#include "station_settings.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#include "board_identity.h"
#include "utf8_name.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
void station_defaults(station_settings_t*s){memset(s,0,sizeof(*s));s->version=1;s->prefix=true;}
static bool text_ok(const char*s,size_t cap){return memchr(s,0,cap)&&name_utf8_valid(s)&&!strchr(s,',')&&!strchr(s,127);}
bool station_valid(const station_settings_t*s){return s->version==1&&s->aprs_ssid<=15&&isfinite(s->latitude)&&fabs(s->latitude)<=90&&isfinite(s->longitude)&&fabs(s->longitude)<=180&&isfinite(s->frequency)&&s->frequency>=0&&s->frequency<=1000&&isfinite(s->height)&&s->height>=0&&s->height<=100000&&floor(s->height)==s->height&&text_ok(s->rig,sizeof(s->rig))&&text_ok(s->antenna,sizeof(s->antenna))&&text_ok(s->remark,sizeof(s->remark));}
static bool number(const cJSON*j,const char*k,double*out){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);if(!cJSON_IsNumber(v)||!isfinite(v->valuedouble))return false;*out=v->valuedouble;return true;}
static bool text(const cJSON*j,const char*k,char*out,size_t cap){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);if(!cJSON_IsString(v)||strlen(v->valuestring)>=cap||!text_ok(v->valuestring,strlen(v->valuestring)+1))return false;strcpy(out,v->valuestring);return true;}
const char*station_parse(const cJSON*j,station_settings_t*out){
 station_settings_t s;station_defaults(&s);const cJSON*p=cJSON_GetObjectItemCaseSensitive(j,"prefix");double ssid;
 if(!cJSON_IsBool(p)||!number(j,"aprsSsid",&ssid)||ssid<0||ssid>15||floor(ssid)!=ssid)return "请选择前缀开关和 0–15 的 APRS 后缀";
 s.prefix=cJSON_IsTrue(p);s.aprs_ssid=(uint8_t)ssid;
 const cJSON*lat=cJSON_GetObjectItemCaseSensitive(j,"latitude"),*lon=cJSON_GetObjectItemCaseSensitive(j,"longitude");
 if(cJSON_IsNull(lat)&&cJSON_IsNull(lon))s.has_location=false;
 else {if(!number(j,"latitude",&s.latitude)||!number(j,"longitude",&s.longitude)||fabs(s.latitude)>90||fabs(s.longitude)>180)return "经纬度须成对填写，纬度 -90～90，经度 -180～180";s.has_location=true;}
 if(!number(j,"frequencyMHz",&s.frequency)||s.frequency<0||s.frequency>1000)return "通联频率须为 0～1000 MHz，0 表示未填写";
 if(!number(j,"antennaHeightM",&s.height)||s.height<0||s.height>100000||floor(s.height)!=s.height)return "天线高度须为 0～100000 的整数米";
 if(!text(j,"rig",s.rig,sizeof(s.rig))||!text(j,"antenna",s.antenna,sizeof(s.antenna)))return "设备、天线各限 15 字节（约 5 个汉字），不可含逗号或控制符";
 if(!text(j,"remark",s.remark,sizeof(s.remark)))return "备注限 63 字节，不可含逗号或控制符";
 *out=s;return NULL;
}
void station_json(cJSON*j,const station_settings_t*s){
 cJSON_AddBoolToObject(j,"prefix",s->prefix);cJSON_AddNumberToObject(j,"aprsSsid",s->aprs_ssid);
 if(s->has_location){cJSON_AddNumberToObject(j,"latitude",s->latitude);cJSON_AddNumberToObject(j,"longitude",s->longitude);}else{cJSON_AddNullToObject(j,"latitude");cJSON_AddNullToObject(j,"longitude");}
 cJSON_AddNumberToObject(j,"frequencyMHz",s->frequency);cJSON_AddNumberToObject(j,"antennaHeightM",s->height);cJSON_AddStringToObject(j,"rig",s->rig);cJSON_AddStringToObject(j,"antenna",s->antenna);cJSON_AddStringToObject(j,"remark",s->remark);
}
void station_display_call(char out[13],const char*call,bool prefix){snprintf(out,13,"%s%.6s",prefix&&call[0]?FMO_CALL_PREFIX:"",call);}
