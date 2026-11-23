#include "driver/dac_continuous.h"
#if !CONFIG_DAC_DMA_AUTO_16BIT_ALIGN
#error Plus DAC input is unsigned 8-bit; enable DAC_DMA_AUTO_16BIT_ALIGN
#endif
static dac_continuous_handle_t cyd_dac;
static bool cyd_dac_enabled;
static uint64_t cyd_done_samples;
static int64_t cyd_done_time;
static bool cyd_dac_done(dac_continuous_handle_t h,const dac_event_data_t*e,void*ctx){
 (void)h;(void)ctx;
 portENTER_CRITICAL_ISR(&mux);cyd_done_samples+=e->write_bytes/2;cyd_done_time=esp_timer_get_time();portEXIT_CRITICAL_ISR(&mux);
 return false;
}
static bool cyd_dac_stopped(dac_continuous_handle_t h,const dac_event_data_t*e,void*ctx){
 (void)h;(void)e;(void)ctx;portENTER_CRITICAL_ISR(&mux);state.dac_dma_stops++;portEXIT_CRITICAL_ISR(&mux);return false;
}
static void audio_stop(void){
 gpio_set_level(4,1);last_pcm_feed=0;
 if(cyd_dac){if(cyd_dac_enabled)dac_continuous_disable(cyd_dac);dac_continuous_del_channels(cyd_dac);}
 cyd_dac=NULL;cyd_dac_enabled=false;mode=-1;
}
static esp_err_t audio_mode(bool mic){
 if(mic)return ESP_ERR_NOT_SUPPORTED;
 if(mode==0)return ESP_OK;
 audio_stop();
 dac_continuous_config_t cfg={.chan_mask=DAC_CHANNEL_MASK_CH1,.desc_num=6,.buf_size=320,.freq_hz=8000,.offset=0,.clk_src=DAC_DIGI_CLK_SRC_APLL,.chan_mode=DAC_CHANNEL_MODE_SIMUL};
 esp_err_t e=dac_continuous_new_channels(&cfg,&cyd_dac);
 if(e!=ESP_OK)return e;
 dac_event_callbacks_t cb={.on_convert_done=cyd_dac_done,.on_stop=cyd_dac_stopped};
 e=dac_continuous_register_event_callback(cyd_dac,&cb,NULL);
 if(e==ESP_OK)e=dac_continuous_enable(cyd_dac);
 if(e==ESP_OK){cyd_dac_enabled=true;uint8_t midpoint[960];memset(midpoint,128,sizeof(midpoint));size_t done=0;
  e=dac_continuous_write(cyd_dac,midpoint,sizeof(midpoint),&done,200);
  if(e==ESP_OK&&done!=sizeof(midpoint))e=ESP_ERR_TIMEOUT;
 }
 if(e!=ESP_OK){audio_stop();return e;}
 vTaskDelay(pdMS_TO_TICKS(140)); // Settle analog midpoint with amplifier muted.
 mode=0;gpio_set_level(4,0);return ESP_OK;
}
static void play(const int16_t*pcm,int count){
 uint8_t out[160];
 while(count>0){voice_view_t v;voice_view(&v);int n=count>160?160:count;
  for(int i=0;i<n;i++)out[i]=cyd_dac_byte(pcm?pcm[i]:0,v.muted?0:v.volume);
  int64_t now=esp_timer_get_time();
  if(pcm&&last_pcm_feed){uint32_t gap=now-last_pcm_feed;portENTER_CRITICAL(&mux);if(gap>state.play_gap_us_max)state.play_gap_us_max=gap;if(gap>100000)state.play_late_feeds++;portEXIT_CRITICAL(&mux);}
  size_t done=0;esp_err_t e=dac_continuous_write(cyd_dac,out,n,&done,100);
  portENTER_CRITICAL(&mux);if(pcm)state.pcm_samples_written+=done;
  if(e!=ESP_OK||done!=(size_t)n)state.output_errors++;
  portEXIT_CRITICAL(&mux);last_pcm_feed=pcm?esp_timer_get_time():0;
  if(e!=ESP_OK||done!=(size_t)n)break;
  if(pcm)pcm+=n;
  count-=n;
 }
}
// Requested rate is reported separately from the measured conversion-complete cadence.
static int dac_clock_rate(void){return mode==0?8000:0;}
static int measure_dac_rate(void){
 for(int i=0;i<20;i++)play(NULL,160);
 portENTER_CRITICAL(&mux);uint64_t first=cyd_done_samples;int64_t start=cyd_done_time;portEXIT_CRITICAL(&mux);
 for(int i=0;i<80;i++)play(NULL,160);
 portENTER_CRITICAL(&mux);uint64_t last=cyd_done_samples;int64_t end=cyd_done_time;portEXIT_CRITICAL(&mux);
 return end>start?(int)((last-first)*1000000/(end-start)):0;
}
