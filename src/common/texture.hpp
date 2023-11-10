#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace nlrs
{
class Texture
{
public:
    using RgbaPixel = std::uint32_t;

    struct Dimensions
    {
        std::uint32_t width;
        std::uint32_t height;
    };

    Texture(std::vector<RgbaPixel>&& pixels, Dimensions dimensions)
        : mPixels(std::move(pixels)),
          mDimensions(dimensions)
    {
    }

    std::span<const RgbaPixel> pixels() const { return mPixels; }
    Dimensions                 dimensions() const { return mDimensions; }

    // `data` is expected to be in RGBA or RGB format, with each component 8 bits.
    static Texture fromMemory(std::span<const std::uint8_t> data);

private:
    std::vector<RgbaPixel> mPixels;
    Dimensions             mDimensions;
};
} // namespace nlrs
