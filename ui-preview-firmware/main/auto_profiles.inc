// Explicit relay selection is the only trigger. Background discovery never calls this.
static bool auto_profile_valid(int id,char root[44],uint8_t digest[32]){
 if(root)root[0]=0;
 if(time(NULL)<=1700000000||!profile_exists(id))return false;
 char cert_key[16],seed_key[16];profile_bank_keys(id,cert_key,seed_key);
 bool active=id==selected_ca&&bundle;char*raw=NULL;size_t n=0;
 if(active){n=strlen(bundle)+1;raw=strdup(bundle);}else if(nvs_get_str(nv,cert_key,NULL,&n)==ESP_OK&&n>1&&n<=6145){raw=malloc(n);if(raw&&nvs_get_str(nv,cert_key,raw,&n)!=ESP_OK){free(raw);raw=NULL;}}
 if(!raw)return false;
 uint8_t public_key[32],seed[32]={0},secret[64]={0};bool have_key=active;
 if(active)memcpy(public_key,pk,32);else{size_t size=32;have_key=nvs_get_blob(nv,seed_key,seed,&size)==ESP_OK&&size==32;if(have_key)crypto_ed25519_key_pair(secret,public_key,seed);}
 crypto_wipe(seed,32);crypto_wipe(secret,64);
 cJSON*b=cJSON_Parse(raw);cJSON*r=cJSON_GetObjectItemCaseSensitive(b,"rootCert");const char*key=root_key(r),*error=NULL;
 bool ok=have_key&&root_selectable(key)&&root_slot(r,false,&error)==id&&!wire_validate_bundle(b,public_key,callsign,time(NULL));
 if(ok){if(root)snprintf(root,44,"%s",key);if(digest)ok=mbedtls_sha256((const uint8_t*)raw,strlen(raw),digest,0)==0;}
 cJSON_Delete(b);free(raw);return ok;
}
static void auto_profiles_describe(cJSON*out){
 uint32_t mask=atomic_load(&auto_ca_mask);cJSON_AddNumberToObject(out,"autoMask",mask);
 cJSON*row;cJSON_ArrayForEach(row,cJSON_GetObjectItemCaseSensitive(out,"profiles")){
  int id=(int)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(row,"id"));bool valid=auto_profile_valid(id,NULL,NULL);
  cJSON_AddBoolToObject(row,"autoAvailable",valid);cJSON_AddBoolToObject(row,"autoEnabled",id>0&&id<=10&&(mask&(1u<<(id-1))));
  cJSON_AddStringToObject(row,"autoReason",valid?"":time(NULL)<=1700000000?"等待网络校时":"需要同呼号、有效且匹配本机密钥的设备证书");
 }
}
static esp_err_t auto_profiles_save(request_t*r,const cJSON*q){
 if(identity_restart_pending||atomic_load(&auto_relay_pending)||atomic_load(&ui_profile_request))return fail(r,"已有切换请求，请稍后保存");
 const cJSON*a=cJSON_GetObjectItemCaseSensitive(q,"profiles"),*v;uint32_t mask=0;
 if(!cJSON_IsArray(a)||cJSON_GetArraySize(a)>10)return fail(r,"请选择允许自动使用的体系");
 cJSON_ArrayForEach(v,a){int id=v->valueint;if(!cJSON_IsNumber(v)||v->valuedouble!=id||id<1||id>10||(mask&(1u<<(id-1)))||!auto_profile_valid(id,NULL,NULL))return fail(r,"所选体系没有有效设备证书，请刷新后重试");mask|=1u<<(id-1);}
 uint32_t old=atomic_load(&auto_ca_mask);esp_err_t e=nvs_set_u32(nv,"auto_ca",mask);if(e==ESP_OK)e=nvs_commit(nv);
 if(e!=ESP_OK){nvs_set_u32(nv,"auto_ca",old);nvs_commit(nv);return fail(r,"自动体系设置保存失败，原设置保留");}
 atomic_store(&auto_ca_mask,mask);return success(r,mask?"已保存；主动接入服务器时自动匹配证书体系":"已关闭自动切换；保留手动选择体系");
}
static void auto_note(const char*text){xSemaphoreTake(lock,portMAX_DELAY);snprintf(view.profile_message,sizeof(view.profile_message),"%s",text);auto_message_until=esp_timer_get_time()+20000000;xSemaphoreGive(lock);}
typedef struct {uint32_t magic,profile;uint8_t identity[32];relay_t relay;} auto_relay_intent_t;
#define AUTO_RELAY_MAGIC 0x41524331u
static cJSON*auto_relay_json(const relay_t*t){
 cJSON*j=cJSON_CreateObject();if(!j)return NULL;
 cJSON_AddStringToObject(j,"name",t->name);cJSON_AddStringToObject(j,"callsign",t->call);cJSON_AddStringToObject(j,"host",t->host);cJSON_AddStringToObject(j,"fingerprint",t->fingerprint);cJSON_AddNumberToObject(j,"uid",t->uid);cJSON_AddNumberToObject(j,"port",t->port);return j;
}
static bool auto_relay_same(const relay_t*a,const relay_t*b){return a->uid==b->uid&&a->port==b->port&&!strcmp(a->call,b->call)&&!strcmp(a->host,b->host)&&!strcmp(a->fingerprint,b->fingerprint);}
static bool auto_use_relay(const relay_t*t){
 for(int i=0;i<relays_count();i++){relay_t r;if(relays_get(i,&r)&&auto_relay_same(t,&r))return relays_select(i);}
 cJSON*j=auto_relay_json(t);if(!j)return false;const char*error=relays_add(j);cJSON_Delete(j);if(error)return false;
 for(int i=0;i<relays_count();i++){relay_t r;if(relays_get(i,&r)&&auto_relay_same(t,&r))return relays_select(i);}
 return false;
}
static void auto_resume_relay(void){
 auto_relay_intent_t intent;size_t size=sizeof(intent);esp_err_t e=nvs_get_blob(nv,"auto_target",&intent,&size);if(e==ESP_ERR_NVS_NOT_FOUND)return;
 bool shape=e==ESP_OK&&size==sizeof(intent)&&intent.magic==AUTO_RELAY_MAGIC&&intent.profile>=1&&intent.profile<=10&&memchr(intent.relay.name,0,sizeof(intent.relay.name))&&memchr(intent.relay.call,0,sizeof(intent.relay.call))&&memchr(intent.relay.host,0,sizeof(intent.relay.host))&&memchr(intent.relay.fingerprint,0,sizeof(intent.relay.fingerprint));
 uint8_t pending=0;if(shape&&intent.profile!=selected_ca&&nvs_get_u8(nv,"profile_next",&pending)==ESP_OK&&pending==intent.profile){auto_connection_blocked=true;auto_note("身份切换未完成，请重启或手动选择体系");return;}
 uint8_t digest[32];bool bound=shape&&bundle&&intent.profile==selected_ca&&mbedtls_sha256((const uint8_t*)bundle,strlen(bundle),digest,0)==0&&!memcmp(digest,intent.identity,32);
 if(bound){auto_connection_blocked=!auto_use_relay(&intent.relay);auto_note(auto_connection_blocked?"目标中继恢复失败，请重新选择中继":"证书体系已切换，正在接入所选中继");}
 else if(shape&&intent.profile==selected_ca){auto_connection_blocked=true;auto_note("身份已变化，请重新选择目标中继");}
 // A staged target without a committed profile switch never changes the old identity.
 nvs_erase_key(nv,"auto_target");nvs_commit(nv);
}
static void auto_route_relay(const relay_t*t){
 if(voice_busy()||identity_restart_pending){auto_note("通联中或正在切换，请稍后重试");return;}
 char accepted[10][44]={{0}};int count=discovery_accepted_roots(t,accepted);uint32_t mask=atomic_load(&auto_ca_mask);
 if(!mask||!count){if(auto_use_relay(t)){auto_connection_blocked=false;voice_connected(false);auto_note(count?"使用当前体系接入":"体系资料未确认，保持当前身份接入");}else auto_note("选择中继失败，原身份保留");return;}
 bool valid[11]={0};char roots[11][44]={{0}};
 for(int id=1;id<=10;id++)if(id==selected_ca||(mask&(1u<<(id-1))))valid[id]=auto_profile_valid(id,roots[id],NULL);
 int target=auto_ca_choose(mask,selected_ca,valid,roots,accepted,count);
 if(!target){auto_note("没有匹配的有效证书，请检查自动体系勾选");return;}
 if(target==selected_ca){if(auto_use_relay(t)){auto_connection_blocked=false;voice_connected(false);auto_note("当前证书可接入，无需切换体系");}else auto_note("选择中继失败，原身份保留");return;}
 auto_relay_intent_t intent={.magic=AUTO_RELAY_MAGIC,.profile=target,.relay=*t};if(!auto_profile_valid(target,NULL,intent.identity)){auto_note("目标证书已变化，未切换");return;}
 // Persist the outgoing identity bank before staging a switch, including custom CAs.
 esp_err_t e=ESP_OK;
 if(bundle&&selected_ca>0){char ck[16],skn[16];profile_bank_keys(selected_ca,ck,skn);uint8_t seed[32];size_t size=32;e=nvs_get_blob(nv,"seed",seed,&size);
 if(e==ESP_OK&&size!=32)e=ESP_FAIL;
 if(e==ESP_OK)e=nvs_set_blob(nv,skn,seed,32);
 crypto_wipe(seed,32);
 if(e==ESP_OK)e=nvs_set_str(nv,ck,bundle);
 if(e==ESP_OK)e=nvs_commit(nv);}
 if(e==ESP_OK){e=nvs_set_blob(nv,"auto_target",&intent,sizeof(intent));}
 if(e==ESP_OK)e=nvs_commit(nv);
 if(e!=ESP_OK){nvs_erase_key(nv,"auto_target");nvs_commit(nv);auto_note("切换准备保存失败，原身份保留");return;}
 request_t r={.usb=true,.input=cJSON_CreateObject()};if(!r.input){nvs_erase_key(nv,"auto_target");nvs_commit(nv);auto_note("内存不足，未切换体系");return;}
 if(target<=2)cJSON_AddStringToObject(r.input,"provider",target==1?"bh1jss":"bi4tes");else cJSON_AddNumberToObject(r.input,"profileId",target);
 rollback_page(&r);const char*error=js(r.output,"error");
 if(error){nvs_erase_key(nv,"profile_next");nvs_erase_key(nv,"auto_target");nvs_commit(nv);auto_note(error);}else auto_note("自动切换证书体系，重启后接入所选中继");
 cJSON_Delete(r.input);cJSON_Delete(r.output);
}
static void auto_relay_tick(void){
 if(auto_message_until&&esp_timer_get_time()>auto_message_until){xSemaphoreTake(lock,portMAX_DELAY);view.profile_message[0]=0;auto_message_until=0;xSemaphoreGive(lock);}
 int expected=1;if(!atomic_compare_exchange_strong(&auto_relay_pending,&expected,2))return;
 xSemaphoreTake(lock,portMAX_DELAY);relay_t target=auto_relay_target;xSemaphoreGive(lock);
 xSemaphoreTake(api_lock,portMAX_DELAY);auto_route_relay(&target);xSemaphoreGive(api_lock);atomic_store(&auto_relay_pending,0);
}
