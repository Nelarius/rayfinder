#pragma once

#include <cmath>

namespace nlrs
{
inline float fract(const float x)
{
    if (x >= 0.0f)
    {
        return x - std::floor(x);
    }
    else
    {
        return x - std::ceil(x);
    }
}
} // namespace nlrs
