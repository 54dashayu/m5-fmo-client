#pragma once
// Presentation and voice packet prefix only; certificate callsign and UID stay unchanged.
#if defined(CONFIG_FMO_BOARD_XINGZHI) || defined(CONFIG_FMO_BOARD_CYD28_PLUS)
#define FMO_CALL_PREFIX "E."
#else
#define FMO_CALL_PREFIX "M."
#endif
