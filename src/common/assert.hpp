#pragma once

#include <cassert>

#define NLRS_ASSERT(condition)                                                                     \
    do                                                                                             \
    {                                                                                              \
        if (!(condition)) [[unlikely]]                                                             \
        {                                                                                          \
            assert(condition);                                                                     \
        }                                                                                          \
    } while (false)
