#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef bool (*voice_publish_t)(const uint8_t*,size_t);
typedef struct {bool connected,tx,rx,muted,mic_test,ready;int volume,mic_peak;uint32_t tx_pending,tx_retries,tx_dropped;uint32_t rx_packets,tx_packets,dropped,decoded_frames,decode_errors,output_errors;int last_codec,speaker_peak,dac_rate,dma_rate,mic_read_ms,stop_ms,mic_rate,mic_initial_rate,mic_clock_request;char caller[13],notice[80],stop_reason[48];} voice_view_t;
esp_err_t voice_init(voice_publish_t publish);
void voice_identity(uint32_t uid,const char*call,uint32_t server);
void voice_connected(bool on);
void voice_receive(const uint8_t*data,size_t n);
bool voice_ptt(bool down);
bool voice_toggle_ptt(void);
bool voice_busy(void);
void voice_view(voice_view_t*out);
void voice_volume(int delta);
void voice_mute(void);
void voice_mic_test(bool on);
void voice_tone(void);

uint32_t voice_stack_free(void);
