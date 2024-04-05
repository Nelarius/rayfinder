#include "texture.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iterator>

namespace nlrs
{
Texture Texture::fromMemory(std::span<const std::uint8_t> data)
{
    int width;
    int height;
    int sourceChannels;

    const int desiredChannels = 4;
    // NOTE: desired channels results in RGBA output, regardless of number of source channels.
    // Missing values are filled in -- e.g. when sourceChannels is 3, then the texture is opaque. If
    // desiredChannels is 0, then sourceChannels is used as the number of channels.
    unsigned char* const pixelPtr = stbi_load_from_memory(
        data.data(),
        static_cast<int>(data.size()),
        &width,
        &height,
        &sourceChannels,
        desiredChannels);

    assert(sourceChannels == 3 || sourceChannels == 4);
    assert(pixelPtr != nullptr);

    const auto numPixels = static_cast<std::size_t>(width * height);
    const auto pixelData =
        std::span<const std::uint32_t>(reinterpret_cast<const std::uint32_t*>(pixelPtr), numPixels);
    std::vector<BgraPixel> pixels;
    pixels.reserve(numPixels);
    std::transform(
        pixelData.begin(),
        pixelData.end(),
        std::back_inserter(pixels),
        [](const std::uint32_t& px) -> BgraPixel {
            const std::uint32_t r = px & 0xffu;
            const std::uint32_t g = (px >> 8) & 0xffu;
            const std::uint32_t b = (px >> 16) & 0xffu;
            return b | (g << 8) | (r << 16) | (255 << 24);
        });

    stbi_image_free(pixelPtr);

    return Texture(
        std::move(pixels),
        Dimensions{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)});
}

Texture Texture::fromPixel(float r, float g, float b, float a)
{
    const std::uint32_t r8 = static_cast<std::uint32_t>(r * 255.0f);
    const std::uint32_t g8 = static_cast<std::uint32_t>(g * 255.0f);
    const std::uint32_t b8 = static_cast<std::uint32_t>(b * 255.0f);
    const std::uint32_t a8 = static_cast<std::uint32_t>(a * 255.0f);

    return Texture(
        std::vector<BgraPixel>{b8 | (g8 << 8) | (r8 << 16) | (a8 << 24)}, Dimensions{1, 1});
}
} // namespace nlrs
