#pragma once

#include <numbers>

namespace nlrs
{
class Angle
{
public:
    static Angle degrees(float degrees)
    {
        return Angle(degrees * std::numbers::pi_v<float> / 180.0f);
    }
    static inline Angle radians(float radians) { return Angle(radians); }

    inline float as_degrees() const { return mRadians * 180.0f / std::numbers::pi_v<float>; }
    inline float as_radians() const { return mRadians; }

    Angle operator+(const Angle& rhs) const { return Angle(mRadians + rhs.mRadians); }

private:
    Angle(float radians)
        : mRadians(radians)
    {
    }

    float mRadians = 0.0f;
};
} // namespace nlrs
