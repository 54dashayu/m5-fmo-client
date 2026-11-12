// Included after request helpers. Public roots are retained independently from
// device identities; importing a root never activates a device certificate.
#define CUSTOM_ROOT_SLOTS 8
static const char*root_keys[]={"5y6IrwX4_Ju5vgdgKHOVrxeseLqlpjIorugzCvcpE0c","QJGn93YtVCiU6klgM_fynYJ5s228OoILw77HSTLTeyc"};
static const char*root_key(const cJSON*r){return js(cJSON_GetObjectItemCaseSensitive(r,"subject"),"publicKey");}
static cJSON*stored_json(const char*key,size_t cap){size_t n=0;if(nvs_get_str(nv,key,NULL,&n)!=ESP_OK||n<2||n>cap)return NULL;char*s=malloc(n);if(!s)return NULL;cJSON*j=nvs_get_str(nv,key,s,&n)==ESP_OK?cJSON_Parse(s):NULL;free(s);return j;}
static int root_slot(cJSON*r,bool create,const char**error){
 const char*k=root_key(r);if(!k){*error="根公钥缺失";return 0;}
 for(int i=0;i<2;i++)if(!strcmp(k,root_keys[i]))return i+1;
 int empty=-1;for(int i=0;i<CUSTOM_ROOT_SLOTS;i++){char key[16];snprintf(key,sizeof(key),"root_%d",i);size_t n=0;if(nvs_get_str(nv,key,NULL,&n)==ESP_ERR_NVS_NOT_FOUND){if(empty<0)empty=i;continue;}cJSON*j=stored_json(key,2049);const char*existing=root_key(j);bool match=existing&&!strcmp(existing,k);cJSON_Delete(j);if(match)return i+3;}
 if(!create)return 0;
 if(empty<0){*error="根证书列表已满（最多 8 个额外体系）";return 0;}
 char*raw=cJSON_PrintUnformatted(r);if(!raw){*error="内存不足";return 0;}if(strlen(raw)>2048){free(raw);*error="根证书超过 2 KB";return 0;}
 char key[16];snprintf(key,sizeof(key),"root_%d",empty);esp_err_t e=nvs_set_str(nv,key,raw);free(raw);if(e==ESP_OK)e=nvs_commit(nv);if(e!=ESP_OK){*error="根证书保存失败，设备存储空间可能不足；现有身份保留";return 0;}return empty+3;
}
static void root_profile_row(cJSON*rows,int id,const char*name,const char*key,const char*active,bool connected){
 char bank[24];if(id<=2)snprintf(bank,sizeof(bank),"%s",id==1?"cert_bh":"cert_bi");else snprintf(bank,sizeof(bank),"cert_p%d",id-3);
 size_t n=0;bool activated=active&&key&&!strcmp(active,key),current=activated||(!active&&selected_ca==id),has=nvs_get_str(nv,bank,NULL,&n)==ESP_OK;
 cJSON*j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"id",id);cJSON_AddStringToObject(j,"name",name?name:"未命名根 CA");cJSON_AddStringToObject(j,"publicKey",key?key:"");cJSON_AddBoolToObject(j,"current",current);cJSON_AddBoolToObject(j,"connected",activated&&connected);cJSON_AddBoolToObject(j,"hasCertificate",has||activated);cJSON_AddBoolToObject(j,"canSwitch",!current||(has&&!activated));cJSON_AddBoolToObject(j,"active",activated);cJSON_AddBoolToObject(j,"builtin",id<=2);cJSON_AddItemToArray(rows,j);
}
static esp_err_t roots_page(request_t*r){
 if(!auth(r)){return ESP_OK;}if(voice_busy())return fail(r,"通联中暂停证书列表操作，请空闲后重试");
 cJSON*q=body(r);const char*action=js(q,"action");
 if(action&&!strcmp(action,"import")){
  cJSON*root=cJSON_GetObjectItemCaseSensitive(q,"root");const char*error=!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(q,"trustRoot"))?"请确认信任此根 CA":wire_validate_root(root,time(NULL));
  int id=error?0:root_slot(root,true,&error);cJSON_Delete(q);return id?success(r,"根 CA 已保存；可选择体系后申请设备证书"):fail(r,error?error:"保存失败");
 }
 cJSON_Delete(q);cJSON*active=bundle?cJSON_Parse(bundle):NULL;const char*active_key=root_key(cJSON_GetObjectItemCaseSensitive(active,"rootCert"));device_view_t v;device_web_view(&v);
 cJSON*out=cJSON_CreateObject(),*rows=cJSON_AddArrayToObject(out,"profiles");root_profile_row(rows,1,"BH1JSS M5 Root CA",root_keys[0],active_key,v.mqtt_online);root_profile_row(rows,2,"FMO Passport Root CA · BI4TES",root_keys[1],active_key,v.mqtt_online);
 for(int i=0;i<CUSTOM_ROOT_SLOTS;i++){char key[16];snprintf(key,sizeof(key),"root_%d",i);cJSON*root=stored_json(key,2049);if(root)root_profile_row(rows,i+3,js(cJSON_GetObjectItemCaseSensitive(root,"subject"),"name"),root_key(root),active_key,v.mqtt_online);cJSON_Delete(root);}
 cJSON_Delete(active);return response(r,out);
}

#include "preset_qfm_root.inc"
#include "preset_bg5esn_root.inc"
#include "preset_rflink_root.inc"
static esp_err_t roots_seed_defaults(void){
 const char*presets[]={preset_qfm_root,preset_bg5esn_root,preset_rflink_root};
 esp_err_t result=ESP_OK;
 for(size_t i=0;i<sizeof(presets)/sizeof(presets[0]);i++){
  cJSON*r=cJSON_Parse(presets[i]);if(!r){result=ESP_ERR_NO_MEM;continue;}
  const char*error=NULL;int id=root_slot(r,false,&error);if(!id)id=root_slot(r,true,&error);
  cJSON_Delete(r);if(!id)result=ESP_FAIL;
 }
 return result;
}
