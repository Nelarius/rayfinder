#pragma once

#include "texture.hpp"

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace nlrs
{
struct GltfMesh
{
    GltfMesh(
        std::vector<glm::vec3>     positions,
        std::vector<glm::vec3>     normals,
        std::vector<glm::vec2>     texCoords,
        std::vector<std::uint32_t> indices,
        size_t                     baseColorTextureIndex)
        : positions(std::move(positions)),
          normals(std::move(normals)),
          texCoords(std::move(texCoords)),
          indices(std::move(indices)),
          baseColorTextureIndex(baseColorTextureIndex)
    {
    }

    GltfMesh(const GltfMesh&) = delete;
    GltfMesh& operator=(const GltfMesh&) = delete;

    GltfMesh(GltfMesh&&) noexcept = default;
    GltfMesh& operator=(GltfMesh&&) noexcept = default;

    std::vector<glm::vec3>     positions;
    std::vector<glm::vec3>     normals;
    std::vector<glm::vec2>     texCoords;
    std::vector<std::uint32_t> indices;
    std::size_t                baseColorTextureIndex;
};

struct GltfModel
{
public:
    GltfModel() = default;
    GltfModel(std::filesystem::path gltfPath);
    GltfModel(std::vector<GltfMesh> meshes, std::vector<Texture> baseColorTextures);

    GltfModel(const GltfModel&) = delete;
    GltfModel& operator=(const GltfModel&) = delete;

    GltfModel(GltfModel&&) = default;
    GltfModel& operator=(GltfModel&&) = default;

    std::vector<GltfMesh> meshes;
    std::vector<Texture>  baseColorTextures;
};
} // namespace nlrs
