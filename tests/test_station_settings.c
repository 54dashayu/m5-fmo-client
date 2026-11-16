#include "station_settings.h"
#include "board_identity.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
static cJSON*input(void){return cJSON_Parse("{\"prefix\":true,\"aprsSsid\":15,\"latitude\":0,\"longitude\":0,\"frequencyMHz\":438.5,\"antennaHeightM\":10,\"rig\":\"IC-705\",\"antenna\":\"GP天线\",\"remark\":\"测试\"}");}
int main(void){
 station_settings_t s;station_defaults(&s);assert(s.prefix&&station_valid(&s)&&!s.has_location);
 cJSON*j=input();assert(!station_parse(j,&s)&&s.has_location&&s.latitude==0&&s.longitude==0&&s.aprs_ssid==15);cJSON_Delete(j);
 station_settings_t before=s;
 const char*fields[]={"aprsSsid","latitude","longitude","frequencyMHz","antennaHeightM"};double bad[]={16,91,-181,-1,1.5};
 for(int i=0;i<5;i++){j=input();cJSON_ReplaceItemInObject(j,fields[i],cJSON_CreateNumber(bad[i]));assert(station_parse(j,&s));assert(!memcmp(&s,&before,sizeof(s)));cJSON_Delete(j);}
 j=input();cJSON_ReplaceItemInObject(j,"latitude",cJSON_CreateNull());assert(station_parse(j,&s));cJSON_ReplaceItemInObject(j,"longitude",cJSON_CreateNull());assert(!station_parse(j,&s)&&!s.has_location);cJSON_Delete(j);
 j=input();cJSON_ReplaceItemInObject(j,"rig",cJSON_CreateString("一二三四五"));assert(!station_parse(j,&s));cJSON_ReplaceItemInObject(j,"rig",cJSON_CreateString("一二三四五六"));assert(station_parse(j,&s));cJSON_ReplaceItemInObject(j,"rig",cJSON_CreateString("A,B"));assert(station_parse(j,&s));cJSON_Delete(j);
 char out[13];station_display_call(out,"BH1JSS",true);assert(!strcmp(out,FMO_CALL_PREFIX "BH1JSS"));station_display_call(out,"BH1JSS",false);assert(!strcmp(out,"BH1JSS"));station_display_call(out,"",true);assert(!out[0]);
 j=input();assert(!station_parse(j,&s));cJSON_Delete(j);j=cJSON_CreateObject();station_json(j,&s);station_settings_t round;assert(!station_parse(j,&round));assert(round.has_location&&round.aprs_ssid==15&&!strcmp(round.antenna,s.antenna));cJSON_Delete(j);
 s.latitude=NAN;assert(!station_valid(&s));station_defaults(&s);memset(s.rig,'x',sizeof(s.rig));assert(!station_valid(&s));
 puts("PASS station validation, zero coordinates, paired clearing, UTF8 byte limits, prefix and JSON roundtrip");
}
