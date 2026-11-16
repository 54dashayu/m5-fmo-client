#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef struct {bool probe;uint32_t generated,submitted,retries,dropped,maxEncodeUs,maxRowReadUs,maxLateUs,durationMs;char reason[48];}voice_sstv_diag_t;
void voice_sstv_diagnostics(voice_sstv_diag_t *out);
bool voice_sstv_probe(uint32_t id);
