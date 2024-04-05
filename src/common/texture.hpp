#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace nlrs
{
class Texture
{
public:
    using BgraPixel = std::uint32_t;

    struct Dimensions
    {
        std::uint32_t width;
        std::uint32_t height;

        bool operator==(const Dimensions&) const = default;
    };

    Texture() = default;
    Texture(std::vector<BgraPixel>&& pixels, Dimensions dimensions)
        : mPixels(std::move(pixels)),
          mDimensions(dimensions)
    {
    }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&&) = default;
    Texture& operator=(Texture&&) = default;

    bool operator==(const Texture&) const = default;

    std::span<const BgraPixel> pixels() const noexcept { return mPixels; }
    Dimensions                 dimensions() const noexcept { return mDimensions; }

    // `data` is expected to be in RGBA or RGB format, with each component 8 bits.
    static Texture fromMemory(std::span<const std::uint8_t> data);
    static Texture fromPixel(float r, float g, float b, float a);

private:
    std::vector<BgraPixel> mPixels;
    Dimensions             mDimensions;
};
} // namespace nlrs
