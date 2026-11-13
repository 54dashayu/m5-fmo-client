#pragma once
#include "cJSON.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
typedef struct {char bytes[128];size_t used;bool ok;bool(*write)(void*,const char*,size_t);void *ctx;} usb_json_stream_t;
static inline void usb_json_flush(usb_json_stream_t*s){if(s->ok&&s->used)s->ok=s->write(s->ctx,s->bytes,s->used);s->used=0;}
static inline void usb_json_char(usb_json_stream_t*s,char c){if(!s->ok)return;s->bytes[s->used++]=c;if(s->used==sizeof(s->bytes))usb_json_flush(s);}
static inline void usb_json_text(usb_json_stream_t*s,const char*p){while(*p)usb_json_char(s,*p++);}
static inline void usb_json_string(usb_json_stream_t*s,const char*p){
 static const char hex[]="0123456789abcdef";usb_json_char(s,'"');
 for(const unsigned char*q=(const unsigned char*)(p?p:"");*q;q++){
  if(*q=='"'||*q=='\\'){usb_json_char(s,'\\');usb_json_char(s,*q);}
  else if(*q<32){usb_json_text(s,"\\u00");usb_json_char(s,hex[*q>>4]);usb_json_char(s,hex[*q&15]);}
  else usb_json_char(s,*q);
 }
 usb_json_char(s,'"');
}
static inline void usb_json_node(usb_json_stream_t*s,const cJSON*j,unsigned depth){
 if(depth>32){s->ok=false;return;}
 if(cJSON_IsObject(j)||cJSON_IsArray(j)){
  bool object=cJSON_IsObject(j);usb_json_char(s,object?'{':'[');const cJSON*n=j->child;
  for(;n;n=n->next){if(n!=j->child)usb_json_char(s,',');if(object){usb_json_string(s,n->string);usb_json_char(s,':');}usb_json_node(s,n,depth+1);}
  usb_json_char(s,object?'}':']');
 }else if(cJSON_IsString(j))usb_json_string(s,j->valuestring);
 else if(cJSON_IsNumber(j)){char n[64];if(cJSON_PrintPreallocated((cJSON*)j,n,sizeof(n),false))usb_json_text(s,n);else s->ok=false;}
 else usb_json_text(s,cJSON_IsTrue(j)?"true":cJSON_IsFalse(j)?"false":"null");
}
static inline bool usb_json_write(const cJSON*j,bool(*write)(void*,const char*,size_t),void*ctx){usb_json_stream_t s={.ok=true,.write=write,.ctx=ctx};usb_json_node(&s,j,0);usb_json_flush(&s);return s.ok;}
