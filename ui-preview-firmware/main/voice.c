#include "voice.h"
#include "voice_wire.h"
#include "voice_tx.h"
#include "voice_dsp.h"
#include "driver/i2s.h"
#include "driver/adc.h"
#include "driver/dac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "nvs.h"
#include "opus.h"
#include "esp_clk_tree.h"
#include "soc/i2s_struct.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
typedef struct{uint16_t n;int64_t arrival;uint8_t data[1400];} packet_t;
static voice_view_t state={.volume=25};static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;static QueueHandle_t queue;static SemaphoreHandle_t qlock;static nvs_handle_t nv;static voice_publish_t publish_packet;
static TaskHandle_t audio_handle;
static QueueHandle_t audio_events;
uint32_t voice_stack_free(void){return audio_handle?uxTaskGetStackHighWaterMark(audio_handle):0;}
static int64_t stop_requested_at;
static voice_dac_filter_t dac_filter;
static int64_t rx_first_arrival;
static uint32_t connection_epoch;
static bool requested,test_requested,tone_requested;static uint32_t uid,server_uid;static char own_call[13];static int64_t rx_until;static int mic_clock_request=8000;static int mode=-1;static OpusDecoder*decoder;
static void reason(const char*s){portENTER_CRITICAL(&mux);snprintf(state.stop_reason,sizeof(state.stop_reason),"%s",s);portEXIT_CRITICAL(&mux);ESP_LOGI("voice","stop=%s",s);}
static void notice(const char*s){portENTER_CRITICAL(&mux);snprintf(state.notice,sizeof(state.notice),"%s",s);portEXIT_CRITICAL(&mux);}
void voice_view(voice_view_t*out){portENTER_CRITICAL(&mux);*out=state;out->rx=esp_timer_get_time()<rx_until;if(!out->rx&&out->caller[0])snprintf(out->last_caller,sizeof(out->last_caller),"%s",out->caller);portEXIT_CRITICAL(&mux);}
bool voice_busy(void){voice_view_t v;voice_view(&v);portENTER_CRITICAL(&mux);bool pending=requested||test_requested;portEXIT_CRITICAL(&mux);return pending||v.tx||v.rx||v.mic_test||v.tx_pending;}
void voice_identity(uint32_t u,const char*c,uint32_t srv){portENTER_CRITICAL(&mux);uid=u;server_uid=srv;snprintf(own_call,sizeof(own_call),"%s",c);portEXIT_CRITICAL(&mux);}
void voice_connected(bool on){portENTER_CRITICAL(&mux);bool interrupted=!on&&requested;state.connected=on;if(!on){if(state.caller[0])snprintf(state.last_caller,sizeof(state.last_caller),"%s",state.caller);connection_epoch++;if(requested)stop_requested_at=esp_timer_get_time();requested=false;rx_until=0;}bool recovered=on&&!state.tx&&!requested&&!strcmp(state.stop_reason,"mqtt_disconnected");portEXIT_CRITICAL(&mux);if(interrupted)reason("mqtt_disconnected");if(recovered)notice("已重新接入，守听中");if(!on&&queue){xSemaphoreTake(qlock,portMAX_DELAY);xQueueReset(queue);xSemaphoreGive(qlock);}}
bool voice_ptt(bool down){portENTER_CRITICAL(&mux);bool ok=!down||(state.connected&&state.ready&&!test_requested&&esp_timer_get_time()>=rx_until);if(ok){if(!down&&requested)stop_requested_at=esp_timer_get_time();requested=down;if(down){stop_requested_at=0;state.stop_reason[0]=0;}}portEXIT_CRITICAL(&mux);if(!down)reason("key_or_page");if(down&&!ok)notice("尚未接入或频道忙，请稍后重试");return ok;}
bool voice_toggle_ptt(void){portENTER_CRITICAL(&mux);bool active=requested||state.tx;portEXIT_CRITICAL(&mux);return voice_ptt(!active);}
void voice_volume(int d){portENTER_CRITICAL(&mux);int n=state.volume+d;state.volume=n<0?0:n>100?100:n;int v=state.volume;portEXIT_CRITICAL(&mux);nvs_set_u8(nv,"volume",v);nvs_commit(nv);}
void voice_mute(void){portENTER_CRITICAL(&mux);state.muted=!state.muted;int m=state.muted;portEXIT_CRITICAL(&mux);nvs_set_u8(nv,"mute",m);nvs_commit(nv);}
void voice_mic_test(bool on){portENTER_CRITICAL(&mux);test_requested=on;requested=false;portEXIT_CRITICAL(&mux);}
void voice_tone(void){if(!voice_busy()){portENTER_CRITICAL(&mux);tone_requested=true;portEXIT_CRITICAL(&mux);}}
void voice_receive(const uint8_t*p,size_t n){voice_packet_t v;if(!queue||!voice_parse(p,n,&v))return;portENTER_CRITICAL(&mux);bool drop=v.uid==uid||state.tx||test_requested;portEXIT_CRITICAL(&mux);if(drop)return;packet_t item={.n=n,.arrival=esp_timer_get_time()};memcpy(item.data,p,n);xSemaphoreTake(qlock,portMAX_DELAY);if(xQueueSend(queue,&item,0)!=pdTRUE){packet_t old;xQueueReceive(queue,&old,0);xQueueSend(queue,&item,0);portENTER_CRITICAL(&mux);state.dropped++;state.rx_overflow++;portEXIT_CRITICAL(&mux);}xSemaphoreGive(qlock);portENTER_CRITICAL(&mux);if(esp_timer_get_time()>=rx_until||strcmp(state.caller,v.call)){if(state.caller[0])snprintf(state.last_caller,sizeof(state.last_caller),"%s",state.caller);rx_first_arrival=esp_timer_get_time();}rx_until=esp_timer_get_time()+700000;snprintf(state.caller,sizeof(state.caller),"%s",v.call);state.rx_packets++;portEXIT_CRITICAL(&mux);}
static void audio_stop(void){if(mode<0)return;if(mode)i2s_adc_disable(I2S_NUM_0);i2s_stop(I2S_NUM_0);i2s_driver_uninstall(I2S_NUM_0);i2s_set_dac_mode(I2S_DAC_CHANNEL_DISABLE);mode=-1;audio_events=NULL;}
// DAC silence is unsigned midscale, never zero-filled DMA. Keep output running
// between packets for a 2-second jitter tail; release idle DMA resources.
static esp_err_t audio_mode(bool mic){if(mode==(int)mic)return ESP_OK;audio_stop();i2s_config_t cfg={.mode=I2S_MODE_MASTER|(mic?(I2S_MODE_RX|I2S_MODE_ADC_BUILT_IN):(I2S_MODE_TX|I2S_MODE_DAC_BUILT_IN)),.sample_rate=mic?mic_clock_request:8000,.bits_per_sample=I2S_BITS_PER_SAMPLE_16BIT,.channel_format=mic?I2S_CHANNEL_FMT_ONLY_LEFT:I2S_CHANNEL_FMT_RIGHT_LEFT,.communication_format=I2S_COMM_FORMAT_STAND_MSB,.dma_buf_count=4,.dma_buf_len=160,.use_apll=true,.tx_desc_auto_clear=false};esp_err_t e=i2s_driver_install(I2S_NUM_0,&cfg,mic?0:32,mic?NULL:&audio_events);if(e!=ESP_OK)return e;mode=mic;if(!mic)memset(&dac_filter,0,sizeof(dac_filter));if(mic){adc1_config_width(ADC_WIDTH_BIT_12);adc1_config_channel_atten(ADC1_CHANNEL_6,ADC_ATTEN_DB_11);e=i2s_set_adc_mode(ADC_UNIT_1,ADC1_CHANNEL_6);if(e==ESP_OK)e=i2s_adc_enable(I2S_NUM_0);}else e=i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);return e;}
static void play(const int16_t*pcm,int count){uint16_t out[320];while(count>0){voice_view_t v;voice_view(&v);int n=count>160?160:count;for(int i=0;i<n;i++){int sample=pcm?pcm[i]:0;out[i*2]=out[i*2+1]=voice_dac_sample(&dac_filter,sample,v.muted?0:v.volume,pcm==NULL);}size_t done=0;esp_err_t e=i2s_write(I2S_NUM_0,out,n*4,&done,pdMS_TO_TICKS(60));if(e!=ESP_OK||done!=(size_t)n*4){portENTER_CRITICAL(&mux);state.output_errors++;portEXIT_CRITICAL(&mux);break;}if(pcm)pcm+=n;count-=n;}}
// Read the programmed clock source and fractional divider, not the requested rate.
static int dac_clock_rate(void){
 uint32_t source=0;esp_err_t e=esp_clk_tree_src_get_freq_hz(I2S0.clkm_conf.clka_en?SOC_MOD_CLK_APLL:SOC_MOD_CLK_PLL_F160M,ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT,&source);
 uint32_t num=I2S0.clkm_conf.clkm_div_num,a=I2S0.clkm_conf.clkm_div_a,b=I2S0.clkm_conf.clkm_div_b,bck=I2S0.sample_rate_conf.tx_bck_div_num;
 if(e!=ESP_OK||num<2||!bck){return 0;}
 if(!a){a=1;b=0;}
 uint64_t denominator=((uint64_t)num*a+b)*bck*2;
 return (int)(((uint64_t)source*a+denominator/2)/denominator);
}
// Measure DMA cadence, not i2s_get_clk (which only echoes the requested rate).
static int measure_dac_rate(void){
 if(!audio_events)return 0;
 play(NULL,640); // All four DMA buffers now contain unsigned midpoint silence.
 xQueueReset(audio_events);i2s_event_t event;int count=0;int64_t first=0,last=0;
 for(int tries=0;tries<40&&count<17;tries++){
  if(xQueueReceive(audio_events,&event,pdMS_TO_TICKS(100))!=pdTRUE)return 0;
  if(event.type!=I2S_EVENT_TX_DONE)continue;
  int64_t now=esp_timer_get_time();if(!count)first=now;last=now;count++;
 }
 return count==17&&last>first?(int)((160LL*16*1000000+(last-first)/2)/(last-first)):0;
}
// Count actual ADC DMA samples over multiple blocks; no samples are transmitted.
static int measure_mic_rate(void){
 uint16_t samples[640];size_t got=0;
 for(int i=0;i<4;i++)if(i2s_read(I2S_NUM_0,samples,sizeof(samples),&got,pdMS_TO_TICKS(200))!=ESP_OK||got!=sizeof(samples))return 0;
 int64_t start=esp_timer_get_time();
 for(int i=0;i<20;i++)if(i2s_read(I2S_NUM_0,samples,sizeof(samples),&got,pdMS_TO_TICKS(200))!=ESP_OK||got!=sizeof(samples))return 0;
 int64_t elapsed=esp_timer_get_time()-start;
 return elapsed>0?(int)((12800LL*1000000+elapsed/2)/elapsed):0;
}
static int calibrate_mic_rate(int*initial){
 int measured=0;*initial=0;
 for(int pass=0;pass<3;pass++){
  if(audio_mode(true)!=ESP_OK)break;
  measured=measure_mic_rate();audio_stop();if(!pass)*initial=measured;
  if(measured>=7920&&measured<=8080)return measured;
  if(measured<5000||measured>12000)break;
  int next=(int)(((int64_t)mic_clock_request*8000+measured/2)/measured);
  if(next<6000||next>14000)break;
  if(pass<2)mic_clock_request=next;
 }
 return measured;
}
static void tx_status(voice_tx_t*q){portENTER_CRITICAL(&mux);state.dropped+=q->dropped-state.tx_dropped;state.tx_dropped=q->dropped;state.tx_packets=q->sent;state.tx_retries=q->retries;state.tx_pending=q->count;portEXIT_CRITICAL(&mux);}
static void flush(voice_tx_t*q,uint8_t*frames,int count,uint32_t base,uint16_t first){
 if(!count)return;
 uint8_t packet[VOICE_TX_BYTES];uint32_t u,s;char call[13];
 portENTER_CRITICAL(&mux);u=uid;s=server_uid;snprintf(call,sizeof(call),"%s",own_call);bool connected=state.connected;portEXIT_CRITICAL(&mux);
 if(!connected)return;
 size_t n=voice_pack(packet,sizeof(packet),frames,count,u,call,s,base,base+(first-1)*80,first);
 uint32_t drops=q->dropped;
 if(n)voice_tx_push(q,packet,n,esp_timer_get_time());
 voice_tx_pump(q,esp_timer_get_time(),publish_packet);tx_status(q);
 if(q->dropped>drops)notice("网络繁忙，部分语音已跳过");
}
static void audio_task(void*a){(void)a;int error=0;decoder=opus_decoder_create(8000,1,&error);esp_err_t init=audio_mode(false);int hardware=init==ESP_OK?dac_clock_rate():0;int measured=init==ESP_OK?measure_dac_rate():0;audio_stop();int mic_initial=0;int mic_measured=init==ESP_OK?calibrate_mic_rate(&mic_initial):0;portENTER_CRITICAL(&mux);state.mic_rate=mic_measured;state.mic_initial_rate=mic_initial;state.mic_clock_request=mic_clock_request;state.dac_rate=hardware;state.dma_rate=measured;state.ready=init==ESP_OK&&decoder!=NULL&&hardware>=7800&&hardware<=8200&&mic_measured>=7840&&mic_measured<=8160;portEXIT_CRITICAL(&mux);if(!state.ready){notice("音频初始化失败");ESP_LOGE("voice","init=%d opus=%d dac_rate=%d",init,error,measured);audio_handle=NULL;vTaskDelete(NULL);return;}notice("音频已就绪");packet_t packet;int16_t pcm[640];uint16_t raw[640];uint8_t frames[656];voice_tx_t txq={0};uint32_t tx_epoch=0;int collected=0,predictor=0,index=0;voice_mic_filter_t mic_filter={0};bool previous=false,was_test=false;uint16_t sequence=1;uint32_t base=0,last_uid=0,last_base=0,last_stamp=0;bool seen=false,underrun=false;int64_t started=0,silence_until=0;
 while(1){portENTER_CRITICAL(&mux);bool want=requested&&state.connected,testing=test_requested,tone=tone_requested;tone_requested=false;bool connected=state.connected,receiving=esp_timer_get_time()<rx_until;uint32_t epoch=connection_epoch;portEXIT_CRITICAL(&mux);
  if(epoch!=tx_epoch||!connected||receiving||testing||(want&&!previous)){voice_tx_clear(&txq);if(epoch!=tx_epoch||!connected||receiving||testing)collected=0;tx_epoch=epoch;}
  else {uint32_t dropped=txq.dropped;voice_tx_pump(&txq,esp_timer_get_time(),publish_packet);if(want&&txq.dropped>dropped)notice("网络繁忙，部分语音已跳过");}
  tx_status(&txq);
  if(want&&esp_timer_get_time()-started>60000000&&previous){portENTER_CRITICAL(&mux);requested=false;portEXIT_CRITICAL(&mux);want=false;reason("time_limit");notice("已达一分钟，请重新短按 B");}
  if(want&&!previous){voice_tx_clear(&txq);tx_status(&txq);started=esp_timer_get_time();struct timeval tv;gettimeofday(&tv,NULL);base=(uint32_t)((uint64_t)tv.tv_sec*1000+tv.tv_usec/1000);sequence=1;collected=0;predictor=index=0;memset(&mic_filter,0,sizeof(mic_filter));xSemaphoreTake(qlock,portMAX_DELAY);xQueueReset(queue);xSemaphoreGive(qlock);notice("正在发话，短按 B 停止");}
  if(!want&&previous){flush(&txq,frames,collected,base,sequence-collected);collected=0;portENTER_CRITICAL(&mux);state.stop_ms=stop_requested_at?(int)((esp_timer_get_time()-stop_requested_at)/1000):0;portEXIT_CRITICAL(&mux);voice_view_t ended;voice_view(&ended);notice(!strcmp(ended.stop_reason,"key_or_page")?"发话结束":!strcmp(ended.stop_reason,"mqtt_disconnected")?(ended.connected?"已重新接入，守听中":"连接断开，已停止发话"):!strcmp(ended.stop_reason,"mqtt_outbox_full")?"网络拥堵，已停止发话":"发话已停止，请查看后台");}
  if((want||testing)&&audio_mode(true)!=ESP_OK){notice("音频切换失败，请重启");voice_ptt(false);reason("audio_mode_failed");vTaskDelay(pdMS_TO_TICKS(500));continue;}
  portENTER_CRITICAL(&mux);state.tx=want;state.mic_test=testing;portEXIT_CRITICAL(&mux);previous=want;was_test=testing;(void)was_test;
  if(want||testing){size_t got=0;int64_t read_started=esp_timer_get_time();esp_err_t e=ESP_OK;for(int part=0;part<4;part++){size_t chunk=0;e=i2s_read(I2S_NUM_0,raw+part*160,320,&chunk,pdMS_TO_TICKS(60));got+=chunk;if(e!=ESP_OK||chunk!=320)break;voice_tx_pump(&txq,esp_timer_get_time(),publish_packet);tx_status(&txq);}portENTER_CRITICAL(&mux);state.mic_read_ms=(esp_timer_get_time()-read_started)/1000;portEXIT_CRITICAL(&mux);if(e!=ESP_OK||got!=sizeof(raw)){notice("麦克风采样中断");voice_ptt(false);reason("microphone_read_failed");continue;}int peak=0;for(int i=0;i<640;i++){int v=raw[i]&4095;int sample=voice_mic_sample(&mic_filter,v);if(v<16||v>4079){portENTER_CRITICAL(&mux);state.mic_clipped++;portEXIT_CRITICAL(&mux);}pcm[i]=sample;if(abs(sample)>peak)peak=abs(sample);}portENTER_CRITICAL(&mux);state.mic_peak=peak;bool still=requested&&state.connected;portEXIT_CRITICAL(&mux);if(want&&still){radpcm_encode(pcm,frames+collected*328,sequence-1,&predictor,&index);collected++;sequence++;if(collected==2){flush(&txq,frames,2,base,sequence-2);collected=0;}}continue;}
  if(tone){if(audio_mode(false)!=ESP_OK){notice("音频切换失败，请重启");continue;}for(int b=0;b<20;b++){for(int i=0;i<160;i++)pcm[i]=(i%10<5)?1800:-1800;play(pcm,160);}play(NULL,640);silence_until=esp_timer_get_time()+2000000;continue;}
  portENTER_CRITICAL(&mux);int64_t first_arrival=rx_first_arrival;portEXIT_CRITICAL(&mux);
  if(uxQueueMessagesWaiting(queue)&&esp_timer_get_time()-first_arrival<240000){if(mode==0)play(NULL,160);else vTaskDelay(pdMS_TO_TICKS(5));continue;}
  // A returned I2S buffer is ahead of the DAC by the other three blocks.
  // Allow one bounded 40ms packet wait before inserting silence; once empty,
  // refill silence continuously so old DMA audio is never repeatedly replayed.
  bool available=xQueueReceive(queue,&packet,pdMS_TO_TICKS(mode==0&&receiving&&!underrun?40:0))==pdTRUE;if(!available){if(seen&&receiving&&!underrun){portENTER_CRITICAL(&mux);state.rx_underflows++;portEXIT_CRITICAL(&mux);underrun=true;}if(mode==0&&esp_timer_get_time()<silence_until)play(NULL,160);else{audio_stop();vTaskDelay(pdMS_TO_TICKS(10));}continue;}if(esp_timer_get_time()-packet.arrival>1000000){portENTER_CRITICAL(&mux);state.dropped++;state.rx_late++;portEXIT_CRITICAL(&mux);continue;}
  underrun=false;voice_packet_t v;if(!voice_parse(packet.data,packet.n,&v))continue;if(audio_mode(false)!=ESP_OK){notice("音频切换失败，请重启");continue;}if(seen&&v.uid==last_uid&&v.base==last_base&&v.timestamp==last_stamp)continue;if(!seen||v.uid!=last_uid||v.base!=last_base)opus_decoder_ctl(decoder,OPUS_RESET_STATE);seen=true;last_uid=v.uid;last_base=v.base;last_stamp=v.timestamp;
  for(int i=0;i<v.count;i++){portENTER_CRITICAL(&mux);bool stop=requested||test_requested;portEXIT_CRITICAL(&mux);if(stop)break;int n=0;if(v.mode[i]==2){if(radpcm_decode(v.raw[i],pcm))n=640;}else n=opus_decode(decoder,v.raw[i],v.len[i],pcm,640,0);if(n>0&&n<=640){int peak=0;for(int k=0;k<n;k++){int value=abs((int)pcm[k]);if(value>peak)peak=value;}portENTER_CRITICAL(&mux);state.decoded_frames++;state.decoded_samples+=n;state.last_codec=v.mode[i];state.speaker_peak=peak;portEXIT_CRITICAL(&mux);portENTER_CRITICAL(&mux);if(rx_until<esp_timer_get_time()+n*125+150000)rx_until=esp_timer_get_time()+n*125+150000;portEXIT_CRITICAL(&mux);play(pcm,n);silence_until=esp_timer_get_time()+2000000;}else{portENTER_CRITICAL(&mux);state.decode_errors++;portEXIT_CRITICAL(&mux);}}
 }
}
esp_err_t voice_init(voice_publish_t publisher){publish_packet=publisher;esp_err_t e=nvs_open("m5voice",NVS_READWRITE,&nv);if(e!=ESP_OK)return e;uint8_t volume=25,mute=0;nvs_get_u8(nv,"volume",&volume);nvs_get_u8(nv,"mute",&mute);state.volume=volume<=100?volume:25;state.muted=mute;queue=xQueueCreate(4,sizeof(packet_t));qlock=xSemaphoreCreateMutex();if(!queue||!qlock)return ESP_ERR_NO_MEM;return xTaskCreate(audio_task,"voice_audio",24576,NULL,5,&audio_handle)==pdPASS?ESP_OK:ESP_ERR_NO_MEM;}
