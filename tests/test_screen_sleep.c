#include <assert.h>
#include <stdio.h>
#include "screen_sleep.h"
int main(void){screen_sleep_t s={0};assert(screen_sleep_step(&s,false,2)==2);
 for(int key=1;key<=4;key*=2){screen_sleep_enter(&s);assert(screen_sleep_step(&s,false,0)==0&&s.sleeping);assert(screen_sleep_step(&s,false,key)==0&&!s.sleeping);assert(screen_sleep_step(&s,false,key)==0);assert(screen_sleep_step(&s,false,0)==0);assert(screen_sleep_step(&s,false,key)==key);}
 screen_sleep_enter(&s);assert(screen_sleep_step(&s,true,0)==0&&!s.sleeping);
 screen_sleep_enter(&s);assert(screen_sleep_step(&s,true,2)==0&&!s.sleeping);assert(screen_sleep_step(&s,true,0)==0);assert(screen_sleep_step(&s,false,2)==2);
 puts("PASS manual screen sleep, incoming-call wake, consume every wake key until release without accidental PTT");}
