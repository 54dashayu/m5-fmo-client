#include <assert.h>
#include "battery.h"
int main(void){assert(battery_decode(0x00)==100);assert(battery_decode(0x8f)==75);assert(battery_decode(0xc0)==50);assert(battery_decode(0xe0)==25);assert(battery_decode(0xf0)==0);assert(battery_decode(0x40)==-1);return 0;}
