#pragma once

#include "texture.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace nlrs
{
class GltfMesh
{
public:
    GltfMesh(
        std::vector<glm::vec3>     positions,
        std::vector<glm::vec3>     normals,
        std::vector<glm::vec2>     texCoords,
        std::vector<std::uint32_t> indices,
        size_t                     baseColorTextureIndex)
        : mPositions(std::move(positions)),
          mNormals(std::move(normals)),
          mTexCoords(std::move(texCoords)),
          mIndices(std::move(indices)),
          mBaseColorTextureIndex(baseColorTextureIndex)
    {
    }

    std::span<const glm::vec3>     positions() const { return mPositions; }
    std::span<const glm::vec3>     normals() const { return mNormals; }
    std::span<const glm::vec2>     texCoords() const { return mTexCoords; }
    std::span<const std::uint32_t> indices() const { return mIndices; }
    size_t                         baseColorTextureIndex() const { return mBaseColorTextureIndex; }

    bool operator==(const GltfMesh&) const = default;

private:
    std::vector<glm::vec3>     mPositions;
    std::vector<glm::vec3>     mNormals;
    std::vector<glm::vec2>     mTexCoords;
    std::vector<std::uint32_t> mIndices;
    std::size_t                mBaseColorTextureIndex;
};

class GltfModel
{
public:
    GltfModel() = default;
    GltfModel(std::filesystem::path gltfPath);
    GltfModel(std::vector<GltfMesh> meshes, std::vector<Texture> baseColorTextures);

    GltfModel(const GltfModel&) = delete;
    GltfModel& operator=(const GltfModel&) = delete;

    GltfModel(GltfModel&&) = default;
    GltfModel& operator=(GltfModel&&) = default;

    std::span<const GltfMesh> meshes() const { return mMeshes; }
    std::span<const Texture>  baseColorTextures() const { return mBaseColorTextures; }

    bool operator==(const GltfModel&) const = default;

private:
    std::vector<GltfMesh> mMeshes;
    std::vector<Texture>  mBaseColorTextures;
};
} // namespace nlrs
