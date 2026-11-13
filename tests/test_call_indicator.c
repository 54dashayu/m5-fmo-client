#include "call_indicator.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 assert(call_flashing_enabled(false,false));assert(call_flashing_enabled(false,true));
 assert(call_flashing_enabled(true,false));assert(!call_flashing_enabled(true,true));
 for(int tx=0;tx<2;tx++)for(int rx=0;rx<2;rx++)for(int on=0;on<2;on++)for(int t=0;t<1600;t+=100){
  call_indicator_t v=call_indicator(tx,rx,on,t);
  assert(v.color==(tx?0xff3030u:rx?0x20ff70u:0xffd166u));
  assert(v.opacity==((on&&(tx||rx)&&t%800>=400)?0:255));
 }
 assert(call_indicator(false,false,true,500).opacity==255);
 puts("PASS callsign indication: red TX/green RX, TX priority, 400ms visible/hidden phases, off and idle restore full opacity");
}
