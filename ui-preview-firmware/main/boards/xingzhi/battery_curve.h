#pragma once
// Xingzhi Cube reference firmware: 12-bit ADC2 CH6, 12 dB attenuation.
// Raw 1970/2062/2154/2246/2338/2430 maps to 0/20/40/60/80/100%.
static inline int xingzhi_battery_percent(int raw){
 if(raw<0||raw>4095)return -1;
 if(raw<=1970)return 0;
 if(raw>=2430)return 100;
 return (raw-1970)*100/460;
}
