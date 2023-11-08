#include "texture.hpp"

#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <iterator>

namespace pt
{
Texture Texture::fromMemory(std::span<const std::uint8_t> data)
{
    int width;
    int height;
    int channelCount;

    unsigned char* const pixelData = stbi_load_from_memory(
        data.data(), static_cast<int>(data.size()), &width, &height, &channelCount, 0);

    assert(channelCount == 3 || channelCount == 4);
    assert(pixelData != nullptr);

    const std::size_t  numPixels = static_cast<std::size_t>(width * height);
    std::vector<Pixel> pixelVec;
    pixelVec.reserve(numPixels);

    if (channelCount == 3)
    {
        using RgbU8 = std::uint8_t[3];

        const RgbU8* pixels = reinterpret_cast<const RgbU8*>(pixelData);
        std::transform(
            pixels, pixels + numPixels, std::back_inserter(pixelVec), [](const RgbU8& p) -> Pixel {
                return Pixel{
                    .r = static_cast<float>(p[0]) / 255.0f,
                    .g = static_cast<float>(p[1]) / 255.0f,
                    .b = static_cast<float>(p[2]) / 255.0f,
                    .a = 1.0f,
                };
            });
    }
    else if (channelCount == 4)
    {
        using RgbaU8 = std::uint8_t[4];

        const RgbaU8* pixels = reinterpret_cast<const RgbaU8*>(pixelData);
        std::transform(
            pixels, pixels + numPixels, std::back_inserter(pixelVec), [](const RgbaU8& p) -> Pixel {
                return Pixel{
                    .r = static_cast<float>(p[0]) / 255.0f,
                    .g = static_cast<float>(p[1]) / 255.0f,
                    .b = static_cast<float>(p[2]) / 255.0f,
                    .a = static_cast<float>(p[3]) / 255.0f,
                };
            });
    }

    stbi_image_free(pixelData);

    return Texture(
        std::move(pixelVec),
        Dimensions{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)});
}
} // namespace pt
