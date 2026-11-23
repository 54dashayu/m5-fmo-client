#include <stdatomic.h>
#include "boards/cyd28_plus/remote_probe.h"
// UI thread owns LEDC changes; USB only submits an atomic bounded request.
static atomic_int cyd_probe_phase,cyd_probe_light,cyd_probe_pcm;
static int64_t cyd_probe_deadline;
static bool cyd_probe_seen;
int cyd_remote_probe_status(void){return atomic_load(&cyd_probe_phase);}
static bool cyd_probe_request(int light,bool pcm){
 if(light<0||light>2)return false;
 voice_view_t v;voice_view(&v);if(!v.ready||v.muted||!v.volume||voice_busy())return false;
 int old=atomic_load(&cyd_probe_phase);
 if(old==-1||old==1||old==2)return false;
 if(!atomic_compare_exchange_strong(&cyd_probe_phase,&old,-1))return false;
 atomic_store(&cyd_probe_pcm,pcm);atomic_store(&cyd_probe_light,light);atomic_store(&cyd_probe_phase,1);return true;
}
bool cyd_remote_probe_request(int light){return cyd_probe_request(light,false);}
bool cyd_remote_pcm_probe_request(int light){return cyd_probe_request(light,true);}
static void cyd_remote_probe_tick(int64_t now){
 int phase=atomic_load(&cyd_probe_phase);
 if(phase==1){
  voice_view_t v;voice_view(&v);if(voice_busy()||v.muted||!v.volume){atomic_store(&cyd_probe_phase,4);return;}
  int light=atomic_load(&cyd_probe_light);
  cyd_display_probe_light(light==2?-1:light);
  cyd_probe_seen=false;cyd_probe_deadline=now+10000000;
  if(atomic_load(&cyd_probe_pcm))cyd_voice_pcm_tone();else voice_tone();atomic_store(&cyd_probe_phase,2);
 }else if(phase==2){
  bool active=cyd_voice_tone_active();if(active)cyd_probe_seen=true;
  if((cyd_probe_seen&&!active)||now>=cyd_probe_deadline){
   cyd_display_probe_light(-1);
   atomic_store(&cyd_probe_phase,cyd_probe_seen&&!active?3:5);
  }
 }
}
