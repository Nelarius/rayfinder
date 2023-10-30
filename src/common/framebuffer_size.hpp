#pragma once

namespace pt
{
struct FramebufferSize
{
    int width;
    int height;
};

constexpr bool operator==(const FramebufferSize& lhs, const FramebufferSize& rhs) noexcept
{
    return lhs.width == rhs.width && lhs.height == rhs.height;
}
} // namespace pt
