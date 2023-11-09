#pragma once

#include "geometry.hpp"
#include "texture.hpp"

#include <glm/glm.hpp>

#include <filesystem>
#include <span>
#include <vector>

namespace pt
{
class GltfModel
{
public:
    GltfModel(std::filesystem::path gltfPath);

    std::span<const Positions>   positions() const { return mPositions; }
    std::span<const Normals>     normals() const { return mNormals; }
    std::span<const TexCoords>   texCoords() const { return mTexCoords; }
    std::span<const std::size_t> baseColorTextureIndices() const
    {
        return mBaseColorTextureIndices;
    }
    std::span<const Texture> baseColorTextures() const { return mBaseColorTextures; }

private:
    std::vector<Positions>   mPositions;
    std::vector<Normals>     mNormals;
    std::vector<TexCoords>   mTexCoords;
    std::vector<std::size_t> mBaseColorTextureIndices;

    std::vector<Texture> mBaseColorTextures;
};
} // namespace pt
