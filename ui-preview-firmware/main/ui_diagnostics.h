#pragma once
#include "sdkconfig.h"
#ifdef CONFIG_FMO_BOARD_XINGZHI
#include <stdatomic.h>
extern atomic_uint ui_stage,ui_stage_ms,ui_loop_ms,ui_loop_max_ms,ui_refresh_max_ms,ui_render_max_ms;
extern atomic_uint ui_flush_sent,ui_flush_done,ui_flush_errors,ui_flush_non_dma,ui_flush_last_error;
#endif
