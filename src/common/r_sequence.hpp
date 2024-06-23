#pragma once

#include "math.hpp"

#include <glm/glm.hpp>

#include <cmath>

namespace nlrs
{
inline glm::vec2 r2Sequence(const std::uint32_t n, const std::uint32_t sequenceLength)
{
    // https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
    constexpr float G = 1.32471795f;
    constexpr float A1 = 1.0f / G;
    constexpr float A2 = 1.0f / (G * G);

    const float i = static_cast<float>(n % sequenceLength);
    const float x = fract(0.5f + A1 * i);
    const float y = fract(0.5f + A2 * i);
    return glm::vec2(x, y);
}
} // namespace nlrs
