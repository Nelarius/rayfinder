#pragma once

#include <cstdint>

namespace nlrs
{
template<typename T>
struct Extent2
{
    T x = T(0);
    T y = T(0);

    constexpr Extent2() noexcept = default;

    constexpr Extent2(T xx, T yy) noexcept
        : x(xx),
          y(yy)
    {
    }

    template<typename U>
    constexpr explicit Extent2(const Extent2<U>& other) noexcept
        : x(static_cast<T>(other.x)),
          y(static_cast<T>(other.y))
    {
    }

    constexpr bool operator==(const Extent2& rhs) const noexcept
    {
        return x == rhs.x && y == rhs.y;
    }
};

using Extent2i = Extent2<std::int32_t>;
using Extent2u = Extent2<std::uint32_t>;

template<typename T>
constexpr float aspectRatio(const Extent2<T>& extent) noexcept
{
    return static_cast<float>(extent.x) / static_cast<float>(extent.y);
}

template<typename T>
constexpr bool operator==(const Extent2<T>& lhs, const Extent2<T>& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
} // namespace nlrs
