#include <common/gltf_model.hpp>
#include <common/texture.hpp>

#include <cassert>
#include <fstream>

int main()
{
    const pt::GltfModel model("Duck.glb");

    const auto baseColorTextures = model.baseColorTextures();
    for (std::size_t i = 0; i < baseColorTextures.size(); ++i)
    {
        const auto& texture = baseColorTextures[i];
        const auto  dimensions = texture.dimensions();
        const auto  pixels = texture.pixels();

        const std::string filename = "base_color_texture_" + std::to_string(i) + ".ppm";
        std::ofstream     file(filename, std::ios::out | std::ios::binary);
        assert(file.is_open());
        // Outputs binary PPM image format, https://netpbm.sourceforge.net/doc/ppm.html
        file << "P6\n";
        file << dimensions.width << ' ' << dimensions.height << '\n';
        file << "255\n";
        for (std::uint32_t i = 0; i < dimensions.height; ++i)
        {
            for (std::uint32_t j = 0; j < dimensions.width; ++j)
            {
                const std::size_t idx = i * dimensions.width + j;
                const auto        pixel = pixels[idx];

                const auto r = static_cast<std::uint8_t>(pixel.r * 255.0f);
                const auto g = static_cast<std::uint8_t>(pixel.g * 255.0f);
                const auto b = static_cast<std::uint8_t>(pixel.b * 255.0f);

                file << r << g << b;
            }
        }
    }

    return 0;
}
