#include <assert.h>
#include <stdio.h>
#include "display_idle.h"
int main(void){
 display_idle_t idle={0};screen_sleep_t sleep={0};
 assert(display_idle_screen_step(&idle,&sleep,1000,false,0)==100);
 assert(display_idle_screen_step(&idle,&sleep,300999,false,0)==100);
 assert(display_idle_screen_step(&idle,&sleep,301000,false,0)==1);
 assert(display_idle_screen_step(&idle,&sleep,600999,false,0)==1);
 assert(display_idle_screen_step(&idle,&sleep,601000,false,0)==0&&sleep.sleeping);
 // All three wake keys are consumed until released, including PTT.
 for(unsigned key=1;key<=4;key*=2){
  assert(display_idle_screen_step(&idle,&sleep,700000*key,false,key)==100&&!sleep.sleeping&&sleep.consume==key);
  assert(display_idle_screen_step(&idle,&sleep,700000*key+50,false,key)==100&&sleep.consume==key);
  assert(display_idle_screen_step(&idle,&sleep,700000*key+100,false,0)==100&&!sleep.consume);
  assert(display_idle_screen_step(&idle,&sleep,700000*key+600050,false,0)==0&&sleep.sleeping);
 }
 assert(display_idle_screen_step(&idle,&sleep,4000000,true,0)==100&&!sleep.sleeping);
 // Continuous reception holds full brightness and postpones both deadlines.
 assert(display_idle_screen_step(&idle,&sleep,5000000,true,0)==100);
 assert(display_idle_screen_step(&idle,&sleep,5299999,false,0)==100);
 assert(display_idle_screen_step(&idle,&sleep,5300000,false,0)==1);
 assert(display_idle_screen_step(&idle,&sleep,5600000,false,0)==0);
 assert(display_idle_screen_step(&idle,&sleep,5600001,true,0)==100&&!sleep.sleeping);
 screen_sleep_enter(&sleep);
 assert(display_idle_screen_step(&idle,&sleep,5600002,false,0)==0);
 assert(display_idle_screen_step(&idle,&sleep,5600003,false,2)==100&&sleep.consume==2);
 assert(display_idle_step(&idle,0,false)==100);
 puts("PASS 5min dim/10min off boundaries, all wake keys consumed, incoming audio wake, continued call, manual sleep and clock reset");
 return 0;
}
