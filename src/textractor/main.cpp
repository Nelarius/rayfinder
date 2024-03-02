#include <common/assert.hpp>
#include <common/gltf_model.hpp>
#include <common/texture.hpp>

#include <fmt/core.h>
#include <stb_image_write.h>

#include <cstdio>

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
        const auto  pixels = texture.pixels();

        const std::string filename = fmt::format("base_color_texture_{}.png", textureIdx);
        const int         numChannels = 4;
        const int         strideBytes = dimensions.width * numChannels;

        NLRS_ASSERT(
            stbi_write_png(
                filename.c_str(),
                dimensions.width,
                dimensions.height,
                numChannels,
                pixels.data(),
                strideBytes) != 0);
    }

    return 0;
}
