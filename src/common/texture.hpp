#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace pt
{
class Texture
{
public:
    struct Pixel
    {
        float r, g, b, a;
    };

    struct Dimensions
    {
        std::uint32_t width;
        std::uint32_t height;
    };

    Texture(std::vector<Pixel>&& pixels, Dimensions dimensions)
        : mPixels(std::move(pixels)),
          mDimensions(dimensions)
    {
    }

    std::span<const Pixel> pixels() const { return mPixels; }
    Dimensions             dimensions() const { return mDimensions; }

    // `data` is expected to be in RGBA or RGB format, with each component 8 bits.
    static Texture fromMemory(std::span<const std::uint8_t> data);

private:
    std::vector<Pixel> mPixels;
    Dimensions         mDimensions;
};
} // namespace pt
