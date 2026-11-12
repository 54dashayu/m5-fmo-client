// Private hardware backend for voice.c. Standard I2S, not ESP32 ADC/DAC.
#include "driver/i2s_std.h"
#include "digital_audio_dsp.h"
static i2s_chan_handle_t digital_channel;
static digital_fir_t digital_rx_filter,digital_tx_filter;
static bool digital_overflow(i2s_chan_handle_t ch,i2s_event_data_t *event,void *ctx){
 (void)ch;(void)event;(void)ctx;
 portENTER_CRITICAL_ISR(&mux);state.tx_adc_overflows++;portEXIT_CRITICAL_ISR(&mux);return false;
}
static void audio_stop(void){
 if(digital_channel){i2s_channel_disable(digital_channel);i2s_del_channel(digital_channel);digital_channel=NULL;}
 mode=-1;last_pcm_feed=0;
}
static esp_err_t audio_mode(bool mic){
 if(mode==(int)mic)return ESP_OK;
 audio_stop();
 i2s_chan_config_t cfg=I2S_CHANNEL_DEFAULT_CONFIG(mic?I2S_NUM_1:I2S_NUM_0,I2S_ROLE_MASTER);
 cfg.dma_desc_num=mic?4:6;cfg.dma_frame_num=mic?320:480;cfg.auto_clear_after_cb=true;
 esp_err_t e=i2s_new_channel(&cfg,mic?NULL:&digital_channel,mic?&digital_channel:NULL);
 if(e!=ESP_OK)return e;
 i2s_std_config_t std={
  .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(mic?16000:24000),
  .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_MONO),
  .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=mic?FMO_MIC_BCLK:FMO_SPK_BCLK,.ws=mic?FMO_MIC_WS:FMO_SPK_WS,.dout=mic?I2S_GPIO_UNUSED:FMO_SPK_DOUT,.din=mic?FMO_MIC_DIN:I2S_GPIO_UNUSED}};
 std.slot_cfg.slot_mask=I2S_STD_SLOT_LEFT;
 e=i2s_channel_init_std_mode(digital_channel,&std);
 if(e==ESP_OK&&mic){i2s_event_callbacks_t cb={.on_recv_q_ovf=digital_overflow};e=i2s_channel_register_event_callback(digital_channel,&cb,NULL);}
 if(e==ESP_OK)e=i2s_channel_enable(digital_channel);
 if(e!=ESP_OK){audio_stop();return e;}
 mode=mic;memset(&digital_rx_filter,0,sizeof(digital_rx_filter));memset(&digital_tx_filter,0,sizeof(digital_tx_filter));return ESP_OK;
}
// Reads return protocol-rate signed PCM (8 kHz), never packed ADC values.
static esp_err_t digital_read(int port,void *out,size_t bytes,size_t *got,TickType_t ticks){
 (void)port;*got=0;if(mode!=1||bytes%2)return ESP_ERR_INVALID_STATE;
 int32_t input[320];int16_t *pcm=out;int64_t deadline=esp_timer_get_time()+(int64_t)ticks*portTICK_PERIOD_MS*1000;
 while(*got<bytes){size_t n=(bytes-*got)/2;if(n>160)n=160;size_t used=0,want=n*2*sizeof(int32_t);
  while(used<want){int64_t left=deadline-esp_timer_get_time();if(left<=0)return ESP_ERR_TIMEOUT;size_t chunk=0;
   esp_err_t e=i2s_channel_read(digital_channel,(uint8_t*)input+used,want-used,&chunk,(left+999)/1000);used+=chunk;
   if(e!=ESP_OK&&e!=ESP_ERR_TIMEOUT)return e;
  }
  digital_down2(&digital_rx_filter,input,pcm+*got/2,n);*got+=n*2;
 }
 return ESP_OK;
}
#define i2s_read digital_read
static void play(const int16_t *pcm,int count){
 int32_t output[480];
 while(count>0){int n=count>160?160:count;voice_view_t v;voice_view(&v);
  digital_up3(&digital_tx_filter,pcm,output,n,v.muted?0:v.volume);
  int64_t now=esp_timer_get_time();
  if(pcm&&last_pcm_feed){uint32_t gap=now-last_pcm_feed;portENTER_CRITICAL(&mux);if(gap>state.play_gap_us_max)state.play_gap_us_max=gap;if(gap>100000)state.play_late_feeds++;portEXIT_CRITICAL(&mux);}
  size_t done=0;esp_err_t e=i2s_channel_write(digital_channel,output,n*3*sizeof(int32_t),&done,60);
  portENTER_CRITICAL(&mux);if(pcm)state.pcm_samples_written+=done/(3*sizeof(int32_t));
  if(e!=ESP_OK||done!=(size_t)n*3*sizeof(int32_t))state.output_errors++;
  portEXIT_CRITICAL(&mux);
  last_pcm_feed=pcm?esp_timer_get_time():0;
  if(e!=ESP_OK||done!=(size_t)n*3*sizeof(int32_t))break;
  if(pcm)pcm+=n;
  count-=n;
 }
}
// Configured protocol-equivalent rate; DMA measurement is separately reported.
static int dac_clock_rate(void){return mode==0?8000:0;}
static int measure_dac_rate(void){
 play(NULL,160*12);int64_t start=esp_timer_get_time();
 for(int i=0;i<20;i++)play(NULL,160);
 int64_t elapsed=esp_timer_get_time()-start;return elapsed>0?(int)(3200LL*1000000/elapsed):0;
}
