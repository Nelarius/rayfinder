#include "flattened_model.hpp"
#include "gltf_model.hpp"

#include <span>

namespace nlrs
{
FlattenedModel::FlattenedModel(const GltfModel& gltfModel)
    : positions(),
      normals(),
      texCoords(),
      baseColorTextureIndices()
{
    for (const auto& mesh : gltfModel.meshes)
    {
        const auto meshPositions = std::span(mesh.positions);
        const auto meshNormals = std::span(mesh.normals);
        const auto meshTexCoords = std::span(mesh.texCoords);
        const auto indices = std::span(mesh.indices);
        const auto baseColorTextureIndex = mesh.baseColorTextureIndex;

        for (std::size_t i = 0; i < indices.size(); i += 3)
        {
            const std::uint32_t idx0 = indices[i + 0];
            const std::uint32_t idx1 = indices[i + 1];
            const std::uint32_t idx2 = indices[i + 2];

            const glm::vec3& p0 = meshPositions[idx0];
            const glm::vec3& p1 = meshPositions[idx1];
            const glm::vec3& p2 = meshPositions[idx2];
            positions.push_back(Positions{.v0 = p0, .v1 = p1, .v2 = p2});

            const glm::vec3& n0 = meshNormals[idx0];
            const glm::vec3& n1 = meshNormals[idx1];
            const glm::vec3& n2 = meshNormals[idx2];
            normals.push_back(Normals{.n0 = n0, .n1 = n1, .n2 = n2});

            const glm::vec2& uv0 = meshTexCoords[idx0];
            const glm::vec2& uv1 = meshTexCoords[idx1];
            const glm::vec2& uv2 = meshTexCoords[idx2];
            texCoords.push_back(TexCoords{.uv0 = uv0, .uv1 = uv1, .uv2 = uv2});

            baseColorTextureIndices.push_back(static_cast<std::uint32_t>(baseColorTextureIndex));
        }
    }
}
} // namespace nlrs
