#include "texture.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <cstring>

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
    unsigned char* const pixelData = stbi_load_from_memory(
        data.data(),
        static_cast<int>(data.size()),
        &width,
        &height,
        &sourceChannels,
        desiredChannels);

    assert(sourceChannels == 3 || sourceChannels == 4);
    assert(pixelData != nullptr);

    const std::size_t numPixels = static_cast<std::size_t>(width * height);

    std::vector<RgbaPixel> pixels(numPixels, 0);
    std::memcpy(pixels.data(), pixelData, numPixels * sizeof(RgbaPixel));

    stbi_image_free(pixelData);

    return Texture(
        std::move(pixels),
        Dimensions{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)});
}
} // namespace nlrs
