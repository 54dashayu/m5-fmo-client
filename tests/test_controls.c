#include "buttons.h"
#include <assert.h>
#include <stdio.h>
static int counts[5],page,tx;static buttons_t b;
static void cb(int key,key_action_t action,void*x){(void)x;counts[action]++;if(action==KEY_CANCEL||action==KEY_PTT_STOP)tx=0;if(action==KEY_PTT_START)tx=1;if(!page&&key==1){if(action==KEY_SHORT)tx=!tx;if(action==KEY_LONG)tx=0;}if(action==KEY_LONG&&key==2){page=1;tx=0;buttons_block(&b);}}
static void step(int mask,int t){buttons_step(&b,mask,t,!page);}
static void reset(void){for(int i=0;i<5;i++)counts[i]=0;page=tx=0;buttons_init(&b,0,cb,0);}
int main(void){
 reset();step(2,1000);step(2,1031);step(0,1100);step(0,1131);assert(tx&&counts[KEY_SHORT]==1);step(0,5000);assert(tx);step(2,5100);step(2,5131);step(0,5200);step(0,5231);assert(!tx&&counts[KEY_SHORT]==2);
 reset();step(2,1000);step(2,1031);step(0,1100);step(0,1131);step(2,1150);step(2,1181);step(0,1200);step(0,1231);assert(tx&&counts[KEY_SHORT]==1);
 step(2,1500);step(2,1531);step(2,2332);assert(!tx&&counts[KEY_LONG]==1);step(0,2400);step(0,2431);assert(!tx&&counts[KEY_SHORT]==1);
 reset();step(4,1000);step(4,1031);step(4,1832);assert(page==1&&counts[KEY_LONG]==1);step(0,1850);step(0,1881);assert(!counts[KEY_SHORT]);step(2,1900);step(2,1931);step(0,2000);step(0,2031);assert(counts[KEY_SHORT]==1&&!tx);
 reset();tx=1;step(3,1000);step(3,1031);assert(counts[KEY_CANCEL]==1&&!tx);step(2,1100);step(2,1131);step(0,1200);step(0,1231);assert(!counts[KEY_SHORT]);
 reset();buttons_init(&b,2,cb,0);step(2,2000);step(0,2100);step(0,2131);assert(!counts[KEY_SHORT]&&!tx);
 reset();step(1,1000);step(0,1010);step(1,1015);step(0,1020);step(0,1051);assert(!counts[KEY_SHORT]);
 reset();buttons_set_momentary(&b,true);step(0,1);step(2,100);step(2,131);assert(tx&&counts[KEY_PTT_START]==1);step(2,1200);assert(tx&&!counts[KEY_LONG]);step(0,1250);step(0,1281);assert(!tx&&counts[KEY_PTT_STOP]==1&&!counts[KEY_SHORT]);
 reset();buttons_set_momentary(&b,true);step(0,1);step(3,100);step(3,131);assert(!tx&&!counts[KEY_PTT_START]);
 reset();page=1;buttons_set_momentary(&b,true);step(0,1);step(2,100);step(2,131);step(0,200);step(0,231);assert(!tx&&counts[KEY_SHORT]==1);
 reset();buttons_set_momentary(&b,true);step(0,1);step(2,100);step(2,131);buttons_set_momentary(&b,false);assert(!tx);step(0,200);step(0,231);assert(!tx&&!counts[KEY_SHORT]);
 puts("PASS: momentary press/release, menu selection, chord cancellation and mode switch; toggle persists after release, second click stops, bounce cooldown, long-B stop, menu/long release isolation, chord cancel, boot hold, debounce");
}
