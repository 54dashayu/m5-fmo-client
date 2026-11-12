#pragma once
#include "sdkconfig.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_FMO_BOARD_XINGZHI)
#error "ESP32-S3 requires the Xingzhi board configuration"
#endif
#ifdef CONFIG_FMO_BOARD_XINGZHI
#define FMO_BOARD_ID "xingzhi-cube-1.54tft-wifi"
#define FMO_BOARD_TITLE "FMO / S3"
#define FMO_VERSION "0.7.29-s3.1"
#define FMO_WIDTH 240
#define FMO_LCD_MOSI 10
#define FMO_LCD_SCLK 9
#define FMO_LCD_DC 8
#define FMO_LCD_CS 14
#define FMO_LCD_RESET 18
#define FMO_BACKLIGHT 13
#define FMO_POWER_HOLD 21
#define FMO_MIC_BCLK 5
#define FMO_MIC_WS 4
#define FMO_MIC_DIN 6
#define FMO_SPK_BCLK 15
#define FMO_SPK_WS 16
#define FMO_SPK_DOUT 7
#define FMO_KEY_A 39
#define FMO_KEY_B 0
#define FMO_KEY_C 40
#else
#define FMO_BOARD_ID "m5-fire"
#define FMO_BOARD_TITLE "FMO / M5"
#define FMO_VERSION "0.7.29"
#define FMO_WIDTH 320
#define FMO_KEY_A 39
#define FMO_KEY_B 38
#define FMO_KEY_C 37
#endif
