#pragma once
#include <stdbool.h>
// USB diagnostic only: 0=backlight off, 1=steady on, 2=unchanged.
bool cyd_remote_probe_request(int light);
int cyd_remote_probe_status(void);
bool cyd_voice_tone_active(void);

bool cyd_remote_pcm_probe_request(int light);
void cyd_voice_pcm_tone(void);
