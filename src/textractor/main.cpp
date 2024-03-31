#include <common/assert.hpp>
#include <common/gltf_model.hpp>
#include <common/texture.hpp>

#include <fmt/core.h>
#include <stb_image_write.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <iterator>
#include <vector>

void printHelp() { std::printf("Usage: textractor <input_gltf_file>\n"); }

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printHelp();
        return 0;
    }

    nlrs::GltfModel model(argv[1]);

    for (std::size_t textureIdx = 0; textureIdx < model.baseColorTextures.size(); ++textureIdx)
    {
        const auto& texture = model.baseColorTextures[textureIdx];
        const auto  dimensions = texture.dimensions();
        const auto  pixelsBgra = texture.pixels();

        const std::string filename = fmt::format("base_color_texture_{}.png", textureIdx);
        const int         numChannels = 4;
        const int         strideBytes = dimensions.width * numChannels;

        static_assert(sizeof(nlrs::Texture::BgraPixel) == sizeof(std::uint32_t));

        std::vector<std::uint32_t> pixelsRgba;
        pixelsRgba.reserve(dimensions.width * dimensions.height);
        std::transform(
            pixelsBgra.begin(),
            pixelsBgra.end(),
            std::back_inserter(pixelsRgba),
            [](const auto& bgra) {
                const std::uint32_t b = bgra & 0xffu;
                const std::uint32_t g = (bgra >> 8) & 0xffu;
                const std::uint32_t r = (bgra >> 16) & 0xffu;
                const std::uint32_t a = (bgra >> 24) & 0xffu;
                return r | (g << 8) | (b << 16) | (a << 24);
            });

        NLRS_ASSERT(
            stbi_write_png(
                filename.c_str(),
                dimensions.width,
                dimensions.height,
                numChannels,
                pixelsRgba.data(),
                strideBytes) != 0);
    }

    return 0;
}
