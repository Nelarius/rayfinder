#pragma once

namespace pt
{
template<typename T>
struct Extent2
{
    T x;
    T y;
};

using Extent2i = Extent2<int>;

template<typename T>
constexpr bool operator==(const Extent2<T>& lhs, const Extent2<T>& rhs) noexcept
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}
} // namespace pt
