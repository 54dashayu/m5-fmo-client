#pragma once
#include "sdkconfig.h"
#include "lv_port_disp.h"
#ifdef CONFIG_FMO_BOARD_XINGZHI
void disp_driver_set_brightness(int percent);
#else
#include "disp_driver.h"
#endif
