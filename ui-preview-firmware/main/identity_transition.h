#pragma once
#include "nvs.h"
#include "cert_wire.h"
#include "monocypher.h"
#include <string.h>
#include <stdio.h>
// A single durable journal record is replayed before any identity is loaded.
// Keep it until every legacy key has been replaced and committed.
typedef struct { uint32_t version; uint8_t seed[32]; char callsign[8]; } identity_transition_t;
static esp_err_t identity_apply_pending(nvs_handle_t nv){
 identity_transition_t next={0};size_t size=sizeof(next);
 esp_err_t e=nvs_get_blob(nv,"identity_next",&next,&size);
 if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;
 if(e!=ESP_OK)goto done;
 if(size!=sizeof(next)||next.version!=1||!memchr(next.callsign,0,sizeof(next.callsign))||!wire_callsign(next.callsign)){e=ESP_ERR_INVALID_ARG;goto done;}
 e=nvs_set_blob(nv,"seed",next.seed,sizeof(next.seed));if(e!=ESP_OK)goto done;
 e=nvs_set_str(nv,"callsign",next.callsign);if(e!=ESP_OK)goto done;
 const char*keys[]={"cert","cert_backup","cert_bh","cert_bi","seed_bh","seed_bi","profile_next","empty_next","selected_ca","enroll_req","enroll_bh","enroll_bi","web_remember","web_code"};
 for(unsigned i=0;i<sizeof(keys)/sizeof(keys[0]);i++){e=nvs_erase_key(nv,keys[i]);if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;if(e!=ESP_OK)goto done;}
 for(unsigned i=0;i<8;i++){char key[16];snprintf(key,sizeof(key),"cert_p%u",i);e=nvs_erase_key(nv,key);if(e!=ESP_OK&&e!=ESP_ERR_NVS_NOT_FOUND)goto done;snprintf(key,sizeof(key),"seed_p%u",i);e=nvs_erase_key(nv,key);if(e!=ESP_OK&&e!=ESP_ERR_NVS_NOT_FOUND)goto done;}
 e=nvs_commit(nv);if(e!=ESP_OK)goto done;
 e=nvs_erase_key(nv,"identity_next");if(e==ESP_OK)e=nvs_commit(nv);
 done:crypto_wipe(&next,sizeof(next));return e;
}
