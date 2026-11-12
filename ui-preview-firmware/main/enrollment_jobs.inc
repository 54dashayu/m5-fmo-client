// Durable per-provider tasks. The private seed stays in NVS and is NEVER sent.
typedef struct { uint32_t version; uint8_t seed[32]; char request[1024]; } enrollment_job_t;
static const char*enroll_keys[]={"enroll_bh","enroll_bi"};
static char enroll_states[2][120];
static bool enroll_read(int id,enrollment_job_t*j){size_t n=sizeof(*j);return nvs_get_blob(nv,enroll_keys[id],j,&n)==ESP_OK&&n==sizeof(*j)&&j->version==1&&memchr(j->request,0,sizeof(j->request));}
static esp_err_t enroll_save(int id,const char*request){
 if(!request||strlen(request)>=1024)return ESP_ERR_INVALID_ARG;
 enrollment_job_t*j=calloc(1,sizeof(*j));if(!j)return ESP_ERR_NO_MEM;j->version=1;size_t n=32;esp_err_t e=nvs_get_blob(nv,"seed",j->seed,&n);
 if(e==ESP_OK&&n!=32)e=ESP_ERR_INVALID_ARG;
 if(e==ESP_OK){snprintf(j->request,sizeof(j->request),"%s",request);e=nvs_set_blob(nv,enroll_keys[id],j,sizeof(*j));}if(e==ESP_OK)e=nvs_commit(nv);crypto_wipe(j,sizeof(*j));free(j);return e;
}
static esp_err_t enroll_cancel(int id){esp_err_t e=nvs_erase_key(nv,enroll_keys[id]);if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;if(e==ESP_OK)e=nvs_commit(nv);if(e==ESP_OK)snprintf(enroll_states[id],120,"本体系自动领取已停止");return e;}
static void enroll_status(cJSON*j){
 cJSON_AddNumberToObject(j,"selectedProfileId",selected_ca);cJSON_AddStringToObject(j,"selectedProfileName",selected_ca_name);
 cJSON*states=cJSON_AddObjectToObject(j,"enrollments");
 for(int i=0;i<2;i++){size_t n=0;const char*text=enroll_states[i];if(!*text){if(nvs_get_blob(nv,enroll_keys[i],NULL,&n)==ESP_OK)text="待处理申请已恢复，联网后自动领取";else if(nvs_get_str(nv,i?"cert_bi":"cert_bh",NULL,&n)==ESP_OK)text="设备证书已就绪，可在体系列表选择使用";else text="尚未开启本体系自动领取";}cJSON_AddStringToObject(states,i?"bi4tes":"bh1jss",text);}
}
static esp_err_t enroll_migrate(void){
 size_t n=0;if(nvs_get_str(nv,"enroll_req",NULL,&n)!=ESP_OK)return ESP_OK;if(n<2||n>1024)return ESP_ERR_INVALID_ARG;
 char*raw=malloc(n);if(!raw)return ESP_ERR_NO_MEM;esp_err_t e=nvs_get_str(nv,"enroll_req",raw,&n);cJSON*j=e==ESP_OK?cJSON_Parse(raw):NULL;
 int id=js(j,"provider")&&!strcmp(js(j,"provider"),"bi4tes");size_t have=0;
 if(e==ESP_OK&&j){uint8_t seed[32],secret[64],pub[32],requested[32];size_t z=32;e=nvs_get_blob(nv,"seed",seed,&z);if(e==ESP_OK&&z==32){crypto_ed25519_key_pair(secret,pub,seed);if(!wire_unb64(js(j,"publicKey"),requested,32)||memcmp(pub,requested,32))e=ESP_ERR_INVALID_ARG;}else e=ESP_ERR_INVALID_ARG;crypto_wipe(seed,32);crypto_wipe(secret,64);
  if(e==ESP_OK&&nvs_get_blob(nv,enroll_keys[id],NULL,&have)==ESP_ERR_NVS_NOT_FOUND)e=enroll_save(id,raw);
 }else if(e==ESP_OK)e=ESP_ERR_INVALID_ARG;
 if(e==ESP_OK){e=nvs_erase_key(nv,"enroll_req");}
 if(e==ESP_OK){e=nvs_commit(nv);}
 cJSON_Delete(j);free(raw);return e;
}
static const char*enroll_store_ready(int id,enrollment_job_t*job,cJSON*b){
 uint8_t secret[64],pub[32],requested[32],seed_copy[32];memcpy(seed_copy,job->seed,32);crypto_ed25519_key_pair(secret,pub,seed_copy);crypto_wipe(secret,64);
 cJSON*q=cJSON_Parse(job->request);const char*call=js(q,"callsign");bool match=wire_unb64(js(q,"publicKey"),requested,32)&&!memcmp(pub,requested,32);
 const char*error=match?wire_validate_bundle(b,pub,call,time(NULL)):"申请密钥不匹配";cJSON_Delete(q);if(error)return error;
 const char*root=root_key(cJSON_GetObjectItemCaseSensitive(b,"rootCert"));if(!root||strcmp(root,root_keys[id]))return "签发根不匹配";
 const char*ck=id?"cert_bi":"cert_bh",*skn=id?"seed_bi":"seed_bh";
 cJSON*old=stored_json(ck,6145);if(old){uint8_t previous[32];const char*k=js(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(old,"userCert"),"subject"),"publicKey");bool same=wire_unb64(k,previous,32)&&!memcmp(pub,previous,32);cJSON_Delete(old);if(!same)return "该体系已有不同密钥身份，保留原身份，请先处理身份冲突";}
 char*raw=cJSON_PrintUnformatted(b);if(!raw)return "内存不足";if(strlen(raw)>6144){free(raw);return "证书包过大";}
 // Seed first, certificate last: existing bank was verified to use this key.
 esp_err_t e=nvs_set_blob(nv,skn,job->seed,32);if(e==ESP_OK)e=nvs_commit(nv);if(e==ESP_OK)e=nvs_set_str(nv,ck,raw);if(e==ESP_OK)e=nvs_commit(nv);free(raw);if(e!=ESP_OK)return "证书保存失败，将自动重试";
 e=enroll_cancel(id);return e==ESP_OK?NULL:"证书已保存，领取任务收尾失败，将自动重试";
}
