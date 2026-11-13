#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/* Native RGB565 LE, 320x240. Caller owns returned JPEG allocation. */
bool sstv_jpeg_encode(const uint8_t *pixels,uint8_t **jpeg,size_t *length);
bool sstv_jpeg_decode(const uint8_t *jpeg,size_t length,uint8_t *pixels);
