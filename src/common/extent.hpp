#pragma once

namespace pt
{
template<typename T>
struct Extent2
{
    T x = T(0);
    T y = T(0);
};

using Extent2i = Extent2<int>;

template<typename T>
constexpr bool operator==(const Extent2<T>& lhs, const Extent2<T>& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
} // namespace pt
