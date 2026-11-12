#include <assert.h>
#include "battery.h"
#include "boards/xingzhi/battery_curve.h"
int main(void){assert(battery_decode(0x00)==100);assert(battery_decode(0x8f)==75);assert(battery_decode(0xc0)==50);assert(battery_decode(0xe0)==25);assert(battery_decode(0xf0)==0);assert(battery_decode(0x40)==-1);assert(xingzhi_battery_percent(-1)==-1);assert(xingzhi_battery_percent(4096)==-1);assert(xingzhi_battery_percent(1900)==0);assert(xingzhi_battery_percent(2457)==100);for(int i=0;i<=5;i++)assert(xingzhi_battery_percent(1970+i*92)==i*20);assert(xingzhi_battery_percent(2016)==10);for(int i=1971;i<=2430;i++)assert(xingzhi_battery_percent(i)>=xingzhi_battery_percent(i-1));return 0;}
