#pragma once
#include "sdkconfig.h"
#include "board_identity.h"
#if defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_FMO_BOARD_XINGZHI)
#error "ESP32-S3 requires the Xingzhi board configuration"
#endif
#ifdef CONFIG_FMO_BOARD_XINGZHI
#include "fmo_profile.h"
#define FMO_BOARD_TITLE "FMO / S3"
#define FMO_VERSION "0.7.78"
#define FMO_HAS_SIDE_LEDS 0

#elif defined(CONFIG_FMO_BOARD_CYD28_PLUS)
#define FMO_BOARD_ID "esp32-2432s028r-plus"
#define FMO_BOARD_TITLE "FMO / Plus"
#define FMO_VERSION "0.1.0"
#define FMO_HAS_SIDE_LEDS 0
#define FMO_WIDTH 320
#define FMO_HAS_MIC 1
#define FMO_HAS_TF 0
#define FMO_HAS_TOUCH 1
#else
#define FMO_BOARD_ID "m5-fire"
#define FMO_BOARD_TITLE "FMO / M5"
#define FMO_VERSION "0.7.77"
#define FMO_HAS_SIDE_LEDS 1
#define FMO_WIDTH 320
#define FMO_KEY_A 39
#define FMO_KEY_B 38
#define FMO_KEY_C 37
#endif

#ifndef FMO_HAS_MIC
#define FMO_HAS_MIC 1
#endif
#ifndef FMO_HAS_TF
#define FMO_HAS_TF FMO_HAS_SIDE_LEDS
#endif
#ifndef FMO_HAS_TOUCH
#define FMO_HAS_TOUCH 0
#endif
