#pragma once

#define PT_MACOS 1
#define PT_WINDOWS 2
#define PT_EMSCRIPTEN 3

#define PT_UNKNOWN 0xFFFF

#if defined(__EMSCRIPTEN__)
#define PT_PLATFORM PT_EMSCRIPTEN
#elif defined(_WIN32)
#define PT_PLATFORM PT_WINDOWS
#elif defined(__APPLE__)
#define PT_PLATFORM PT_MACOS
#else
#define PT_PLATFORM PT_UNKNOWN
#endif

static_assert(PT_PLATFORM != PT_UNKNOWN, "Platform detection failed.");
