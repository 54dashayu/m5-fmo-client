/* One background card operation; result is published before active becomes false. */
static atomic_bool card_job_active,card_job_pending;
static atomic_uint card_job_id;
static atomic_bool card_job_format,card_job_ok;
static _Atomic(const char*) card_job_error="";
static void card_job_tick(void){
 if(!atomic_exchange(&card_job_pending,false))return;
 const char *error=(voice_busy()||sstv_rx_active())?"请先结束通联并退出 SSTV 接收":sstv_store_card_manage(card_job_format);
 card_job_ok=error==NULL;card_job_error=error?error:"";
 atomic_store(&card_job_active,false);
}
static const char *card_job_start(bool format){
 if(!sstv_sd_supported())return "本设备不支持 TF 卡管理";
 if(!atomic_load(&card_worker_ready))return "后台尚未就绪，请稍后重试";
 if(atomic_exchange(&card_job_active,true))return "TF 卡操作正在进行，请等待完成";
 card_job_format=format;card_job_ok=false;card_job_error="";sstv_sd_manage_reset();atomic_fetch_add(&card_job_id,1);
 atomic_store(&card_job_pending,true);
 return NULL;
}
static esp_err_t card_job_status(request_t *r){
 if(!auth(r))return ESP_OK;
 bool active=atomic_load(&card_job_active);unsigned id=atomic_load(&card_job_id);
 cJSON *j=cJSON_CreateObject();cJSON_AddNumberToObject(j,"jobId",id);cJSON_AddBoolToObject(j,"active",active);
 cJSON_AddNumberToObject(j,"completedStages",active?sstv_sd_manage_stage():id&&card_job_ok?4:0);
 cJSON_AddNumberToObject(j,"totalStages",4);cJSON_AddBoolToObject(j,"format",atomic_load(&card_job_format));
 if(!active){cJSON_AddBoolToObject(j,"success",id&&card_job_ok);cJSON_AddStringToObject(j,"error",card_job_error);}
 return response(r,j);
}
