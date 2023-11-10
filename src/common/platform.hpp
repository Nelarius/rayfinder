#pragma once

#define NLRS_MACOS 1
#define NLRS_WINDOWS 2
#define NLRS_EMSCRIPTEN 3

#define NLRS_UNKNOWN 0xFFFF

#if defined(__EMSCRIPTEN__)
#define NLRS_PLATFORM NLRS_EMSCRIPTEN
#elif defined(_WIN32)
#define NLRS_PLATFORM NLRS_WINDOWS
#elif defined(__APPLE__)
#define NLRS_PLATFORM NLRS_MACOS
#else
#define NLRS_PLATFORM NLRS_UNKNOWN
#endif

static_assert(NLRS_PLATFORM != NLRS_UNKNOWN, "Platform detection failed.");
