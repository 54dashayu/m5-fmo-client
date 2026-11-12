// Selecting a root without a device certificate is a durable offline identity.
// Preserve every bank and bind pending enrollment to its original key.
static bool profile_exists(int id){
 if(id==1||id==2)return true;
 if(id<3||id>10)return false;
 char k[32];snprintf(k,sizeof(k),"root_%d",id-3);cJSON*r=stored_json(k,2049);bool allowed=root_selectable(root_key(r));cJSON_Delete(r);return allowed;
}
static void profile_name_for(int id,char*out,size_t cap){
 if(id<=2){snprintf(out,cap,"%s",id==1?"BH1JSS M5":id==2?"BI4TES / Passport":"尚无证书体系");return;}
 char k[32];snprintf(k,sizeof(k),"root_%d",id-3);cJSON*j=stored_json(k,2049);const char*s=js(cJSON_GetObjectItemCaseSensitive(j,"subject"),"name");snprintf(out,cap,"%s",s?s:"其他证书体系");cJSON_Delete(j);
}
static void profile_bank_keys(int id,char*cert,char*seed){
 if(id<=2){snprintf(cert,16,"%s",id==1?"cert_bh":"cert_bi");snprintf(seed,16,"%s",id==1?"seed_bh":"seed_bi");}
 else{snprintf(cert,16,"cert_p%u",(unsigned)(uint8_t)(id-3));snprintf(seed,16,"seed_p%u",(unsigned)(uint8_t)(id-3));}
}
static const char*prepare_empty_profile(int id){
 if(!profile_exists(id))return "根 CA 不存在";
 if(identity_restart_pending)return "已有切换请求，请等待重启";
 uint8_t seed[32];size_t n=32;esp_err_t e=nvs_get_blob(nv,"seed",seed,&n);if(e!=ESP_OK||n!=32){crypto_wipe(seed,32);return "本机密钥读取失败";}
 if(bundle){cJSON*j=cJSON_Parse(bundle);const char*error=NULL;int old=root_slot(cJSON_GetObjectItemCaseSensitive(j,"rootCert"),false,&error);cJSON_Delete(j);if(!old){crypto_wipe(seed,32);return "原身份未能定位，未切换";}char ck[16],skn[16];profile_bank_keys(old,ck,skn);e=nvs_set_blob(nv,skn,seed,32);if(e==ESP_OK)e=nvs_set_str(nv,ck,bundle);if(e==ESP_OK)e=nvs_commit(nv);}
 char ck[16],skn[16];profile_bank_keys(id,ck,skn);
 // Prefer a durable enrollment key, then a previously selected empty bank.
 enrollment_job_t*job=id<=2?calloc(1,sizeof(*job)):NULL;
 if(id<=2&&!job)e=ESP_ERR_NO_MEM;
 if(job&&enroll_read(id-1,job))memcpy(seed,job->seed,32);
 else{uint8_t saved[32];size_t z=32;if(nvs_get_blob(nv,skn,saved,&z)==ESP_OK&&z==32)memcpy(seed,saved,32);crypto_wipe(saved,32);}
 if(job){crypto_wipe(job,sizeof(*job));free(job);}
 if(e==ESP_OK)e=nvs_set_blob(nv,skn,seed,32);
 crypto_wipe(seed,32);if(e==ESP_OK)e=nvs_commit(nv);
 if(e==ESP_OK)e=nvs_set_u8(nv,"empty_next",id);
 if(e==ESP_OK)e=nvs_commit(nv);
 return e==ESP_OK?NULL:"体系选择保存失败，原身份保留";
}
static esp_err_t apply_empty_profile(void){
 uint8_t id=0;esp_err_t e=nvs_get_u8(nv,"empty_next",&id);if(e==ESP_ERR_NVS_NOT_FOUND)return ESP_OK;if(e!=ESP_OK)return e;if(!profile_exists(id))return ESP_ERR_INVALID_ARG;
 char ck[16],skn[16];profile_bank_keys(id,ck,skn);uint8_t seed[32];size_t n=32;e=nvs_get_blob(nv,skn,seed,&n);if(e==ESP_OK&&n!=32)e=ESP_ERR_INVALID_ARG;
 if(e==ESP_OK)e=nvs_set_blob(nv,"seed",seed,32);
 crypto_wipe(seed,32);if(e==ESP_OK)e=nvs_set_u8(nv,"selected_ca",id);
 if(e==ESP_OK){e=nvs_erase_key(nv,"cert");if(e==ESP_ERR_NVS_NOT_FOUND)e=ESP_OK;}
 if(e==ESP_OK)e=nvs_set_u8(nv,"relay_notice",1);
 if(e==ESP_OK)e=nvs_commit(nv);
 if(e==ESP_OK)e=nvs_erase_key(nv,"empty_next");
 if(e==ESP_OK)e=nvs_commit(nv);
 return e;
}
