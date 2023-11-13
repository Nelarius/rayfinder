#pragma once

#include <cassert>
#include <cmath>
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

    inline float asDegrees() const { return mRadians * 180.0f / std::numbers::pi_v<float>; }
    inline float asRadians() const { return mRadians; }

    Angle operator+(const Angle& rhs) const { return Angle(mRadians + rhs.mRadians); }
    bool  operator<(const Angle& rhs) const
    {
        assert(!std::isnan(mRadians));
        assert(!std::isnan(rhs.mRadians));
        return mRadians < rhs.mRadians;
    }

private:
    Angle(float radians)
        : mRadians(radians)
    {
    }

    float mRadians = 0.0f;
};
} // namespace nlrs
