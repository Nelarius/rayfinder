#pragma once

#include "geometry.hpp"
#include "texture.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace nlrs
{
struct Positions
{
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
};

struct Normals
{
    glm::vec3 n0;
    glm::vec3 n1;
    glm::vec3 n2;
};

struct TexCoords
{
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec2 uv2;
};

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
