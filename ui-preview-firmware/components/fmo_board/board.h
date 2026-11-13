#pragma once
#include "sdkconfig.h"
#include "board_identity.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_FMO_BOARD_XINGZHI)
#error "ESP32-S3 requires the Xingzhi board configuration"
#endif
#ifdef CONFIG_FMO_BOARD_XINGZHI
#include "fmo_profile.h"
#define FMO_BOARD_TITLE "FMO / S3"
#define FMO_VERSION "0.7.63"
#define FMO_HAS_SIDE_LEDS 0

#else
#define FMO_BOARD_ID "m5-fire"
#define FMO_BOARD_TITLE "FMO / M5"
#define FMO_VERSION "0.7.60"
#define FMO_HAS_SIDE_LEDS 1
#define FMO_WIDTH 320
#define FMO_KEY_A 39
#define FMO_KEY_B 38
#define FMO_KEY_C 37
#endif
