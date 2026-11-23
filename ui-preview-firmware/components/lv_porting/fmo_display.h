#pragma once
#include "sdkconfig.h"
#include "lv_port_disp.h"
#if defined(CONFIG_FMO_BOARD_XINGZHI) || defined(CONFIG_FMO_BOARD_CYD28_PLUS)
void disp_driver_set_brightness(int percent);
#else
#include "disp_driver.h"
#endif

#ifdef CONFIG_FMO_BOARD_CYD28_PLUS
void cyd_display_probe_light(int level);
#endif
