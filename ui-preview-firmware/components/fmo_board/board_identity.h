#pragma once
// Presentation and voice packet prefix only; certificate callsign and UID stay unchanged.
#ifdef CONFIG_FMO_BOARD_XINGZHI
#define FMO_CALL_PREFIX "E."
#else
#define FMO_CALL_PREFIX "M."
#endif
