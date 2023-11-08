#pragma once

#include "geometry.hpp"
#include "texture.hpp"

#include <filesystem>
#include <span>
#include <vector>

namespace pt
{
class GltfModel
{
public:
    GltfModel(std::filesystem::path gltfPath);

    std::span<const Triangle>    triangles() const { return mTriangles; }
    std::span<const std::size_t> baseColorTextureIndices() const
    {
        return mBaseColorTextureIndices;
    }
    std::span<const Texture> baseColorTextures() const { return mBaseColorTextures; }

private:
    std::vector<Triangle>    mTriangles;
    std::vector<std::size_t> mBaseColorTextureIndices;

    std::vector<Texture> mBaseColorTextures;
};
} // namespace pt
