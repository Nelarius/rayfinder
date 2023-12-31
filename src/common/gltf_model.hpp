#pragma once

#include "texture.hpp"
#include "triangle_attributes.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace nlrs
{
class GltfModel
{
public:
    GltfModel(std::filesystem::path gltfPath);

    std::span<const Positions>     positions() const { return mPositions; }
    std::span<const Normals>       normals() const { return mNormals; }
    std::span<const TexCoords>     texCoords() const { return mTexCoords; }
    std::span<const std::uint32_t> baseColorTextureIndices() const
    {
        return mBaseColorTextureIndices;
    }
    std::span<const Texture> baseColorTextures() const { return mBaseColorTextures; }

private:
    std::vector<Positions>     mPositions;
    std::vector<Normals>       mNormals;
    std::vector<TexCoords>     mTexCoords;
    std::vector<std::uint32_t> mBaseColorTextureIndices;

    std::vector<Texture> mBaseColorTextures;
};
} // namespace nlrs
