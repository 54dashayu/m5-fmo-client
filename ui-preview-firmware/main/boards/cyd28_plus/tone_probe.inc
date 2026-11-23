// Diagnostic only: RC-clocked hardware cosine bypasses PCM, I2S DMA and APLL.
// This runs exclusively on the audio task, on DAC2/GPIO26; never touch DAC1/GPIO25.
static esp_err_t cyd_hardware_tone(void){
 voice_view_t v;voice_view(&v);if(v.muted||v.volume==0)return ESP_OK;
 gpio_set_level(4,1);audio_stop();
 esp_err_t e=dac_output_enable(DAC_CHAN_1);bool started=false;
 if(e==ESP_OK)e=dac_output_voltage(DAC_CHAN_1,128);
 if(e==ESP_OK){vTaskDelay(pdMS_TO_TICKS(150));
  dac_cw_config_t cfg={.en_ch=DAC_CHAN_1,.scale=DAC_CW_SCALE_8,.phase=DAC_CW_PHASE_0,.freq=800,.offset=0};
  e=dac_cw_generator_config(&cfg);
 }
 if(e==ESP_OK){e=dac_cw_generator_enable();started=e==ESP_OK;}
 if(e==ESP_OK){
  notice("硬件测试：5秒，固定低幅度");gpio_set_level(4,0);
  for(int i=0;i<100;i++){vTaskDelay(pdMS_TO_TICKS(50));voice_view(&v);if(v.muted||v.volume==0)break;}
 }
 gpio_set_level(4,1);
 if(started){esp_err_t stop=dac_cw_generator_disable();if(e==ESP_OK)e=stop;}
 // Writing midpoint also disconnects the cosine channel before normal I2S resumes.
 esp_err_t midpoint=dac_output_voltage(DAC_CHAN_1,128);if(e==ESP_OK)e=midpoint;
 vTaskDelay(pdMS_TO_TICKS(150));
 // Leave pad enabled at midpoint, as the legacy I2S path does not init RTC pads.
 gpio_set_level(4,0);
 ESP_LOGI("voice","hardware_tone rc_clock DAC2 800Hz scale1/8 result=%d",e);
 return e;
}

// Local PCM enters exactly the same play()/I2S path as decoded FMO audio.
static esp_err_t cyd_pcm_tone(void){
 voice_view_t v;voice_view(&v);if(v.muted||!v.volume)return ESP_OK;
 uint32_t errors=v.output_errors;esp_err_t e=audio_mode(false);if(e!=ESP_OK)return e;
 static const int16_t wave[16]={0,689,1273,1663,1800,1663,1273,689,0,-689,-1273,-1663,-1800,-1663,-1273,-689};
 int16_t pcm[160];notice("PCM测试：5秒，本地音源");
 for(int b=0;b<250;b++){
  voice_view(&v);if(v.muted||!v.volume)break;
  for(int i=0;i<160;i++){int sample=b*160+i;int envelope=sample<160?sample:sample>=39840?39999-sample:160;pcm[i]=wave[i%16]*envelope/160;}
  play(pcm,160);
 }
 play(NULL,640);voice_view(&v);return v.output_errors==errors?ESP_OK:ESP_FAIL;
}
