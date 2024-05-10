#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
// Array contains consecutive R, G values. Pixels are indexed from the top-left.
extern const uint8_t blueNoiseValues[32768];

extern const size_t blueNoiseWidth;
extern const size_t blueNoiseHeight;
#ifdef __cplusplus
}
#endif
