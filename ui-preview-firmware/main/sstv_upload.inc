/* Authenticated same-origin relay; destination is fixed, TLS roots verified. */
#ifdef CONFIG_FMO_BOARD_XINGZHI
#include "esp_crt_bundle.h"
static bool wall_text(const cJSON*j,const char*k,size_t max,bool required){const cJSON*v=cJSON_GetObjectItemCaseSensitive(j,k);return (!v&&!required)||(cJSON_IsString(v)&&strlen(v->valuestring)<=max&&(!required||v->valuestring[0]));}
static bool wall_payload(const cJSON*j,bool remove){
 if(!cJSON_IsObject(j))return false;
 if(remove)return wall_text(j,"id",80,true)&&wall_text(j,"delete_token",160,true);
 const char*image=js(j,"image"),*call=js(j,"callsign");
 if(!image||strncmp(image,"data:image/png;base64,",22)||strlen(image)<30||!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j,"consent"))||!wall_text(j,"callsign",20,true)||strlen(call)<2)return false;
 for(const char*p=call;*p;p++)if(!((*p>='A'&&*p<='Z')||(*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='/'||*p=='-'))return false;
 return wall_text(j,"received",32,true)&&wall_text(j,"event",80,false)&&wall_text(j,"location",80,false)&&wall_text(j,"equipment",160,false)&&wall_text(j,"note",500,false);
}
static esp_err_t gallery_wall_http(httpd_req_t*h){
 request_t request={.http=h};xSemaphoreTake(api_lock,portMAX_DELAY);bool allowed=auth(&request);xSemaphoreGive(api_lock);if(!allowed)return ESP_OK;
 if(voice_busy())return fail(&request,"通联中，请稍后上传");
 bool remove=!strcmp(h->uri,"/api/gallery/wall/delete");size_t limit=remove?1024:800*1024;
 if(h->content_len<=0||(size_t)h->content_len>limit)return fail(&request,"请求过大或为空");
 char*payload=heap_caps_malloc((size_t)h->content_len+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(!payload)return fail(&request,"上传内存不足，请稍后重试");
 size_t used=0;int64_t deadline=esp_timer_get_time()+15000000;
 while(used<(size_t)h->content_len&&esp_timer_get_time()<deadline){int n=httpd_req_recv(h,payload+used,h->content_len-used);if(n<=0)break;used+=n;}
 payload[used]=0;cJSON*j=used==(size_t)h->content_len&&shallow_json(payload)?cJSON_Parse(payload):NULL;
 bool valid=wall_payload(j,remove);cJSON_Delete(j);if(!valid){free(payload);return fail(&request,"请检查图片、呼号、接收时间及公开上传确认");}
 char*reply=calloc(1,4097);if(!reply){free(payload);return fail(&request,"回复内存不足，请稍后重试");}
 esp_http_client_config_t cfg={.url=remove?"https://space.bh1jss.net/sstv-wall/api/delete":"https://space.bh1jss.net/sstv-wall/api/photos",.method=HTTP_METHOD_POST,.crt_bundle_attach=esp_crt_bundle_attach,.timeout_ms=10000,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true};
 esp_http_client_handle_t client=esp_http_client_init(&cfg);bool complete=false;int status=0;
 if(client){esp_http_client_set_header(client,"Content-Type","application/json");esp_http_client_set_header(client,"X-SSTV-Wall","1");
  if(esp_http_client_open(client,used)==ESP_OK){size_t sent=0;deadline=esp_timer_get_time()+30000000;while(sent<used&&esp_timer_get_time()<deadline){int n=esp_http_client_write(client,payload+sent,used-sent>4096?4096:used-sent);if(n<=0)break;sent+=n;}
   if(sent==used){int64_t length=esp_http_client_fetch_headers(client);status=esp_http_client_get_status_code(client);if(length>=0&&length<=4096){int n=esp_http_client_read_response(client,reply,4096);complete=n>0&&esp_http_client_is_complete_data_received(client);}}
  }esp_http_client_close(client);esp_http_client_cleanup(client);
 }
 free(payload);j=complete?cJSON_Parse(reply):NULL;free(reply);
 if(!cJSON_IsObject(j)){cJSON_Delete(j);return fail(&request,"未能确认照片墙结果，请先查看照片墙再重试，避免重复提交");}
 if(status<200||status>=300){const char*error=js(j,"error");esp_err_t e=fail(&request,error?error:"照片墙未接受请求");cJSON_Delete(j);return e;}
 /* Capability token is returned only to the authenticated caller; never log it. */
 return response(&request,j);
}
/* Read-only TLS probe, never uploads a test photograph. */
static esp_err_t wall_probe(request_t*r){
 if(!auth(r))return ESP_OK;
 if(voice_busy())return fail(r,"通联中，请稍后检查");
 esp_http_client_config_t cfg={.url="https://space.bh1jss.net/sstv-wall/api/photos?callsign=SWL-TEST",.method=HTTP_METHOD_GET,.crt_bundle_attach=esp_crt_bundle_attach,.timeout_ms=10000,.buffer_size=1024,.disable_auto_redirect=true};
 esp_http_client_handle_t c=esp_http_client_init(&cfg);int status=0;
 if(c){if(esp_http_client_open(c,0)==ESP_OK){esp_http_client_fetch_headers(c);status=esp_http_client_get_status_code(c);}esp_http_client_close(c);esp_http_client_cleanup(c);}
 cJSON*j=cJSON_CreateObject();cJSON_AddBoolToObject(j,"tlsVerified",status>0);cJSON_AddNumberToObject(j,"httpStatus",status);return response(r,j);
}
#endif
