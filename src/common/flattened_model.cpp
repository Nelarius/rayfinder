#include "flattened_model.hpp"
#include "gltf_model.hpp"

namespace nlrs
{
FlattenedModel::FlattenedModel(const GltfModel& gltfModel)
    : mPositions(),
      mNormals(),
      mTexCoords(),
      mBaseColorTextureIndices()
{
    for (const auto& mesh : gltfModel.meshes())
    {
        const auto positions = mesh.positions();
        const auto normals = mesh.normals();
        const auto texCoords = mesh.texCoords();
        const auto indices = mesh.indices();
        const auto baseColorTextureIndex = mesh.baseColorTextureIndex();

        for (std::size_t i = 0; i < indices.size(); i += 3)
        {
            const std::uint32_t idx0 = indices[i + 0];
            const std::uint32_t idx1 = indices[i + 1];
            const std::uint32_t idx2 = indices[i + 2];

            const glm::vec3& p0 = positions[idx0];
            const glm::vec3& p1 = positions[idx1];
            const glm::vec3& p2 = positions[idx2];
            mPositions.push_back(Positions{.v0 = p0, .v1 = p1, .v2 = p2});

            const glm::vec3& n0 = normals[idx0];
            const glm::vec3& n1 = normals[idx1];
            const glm::vec3& n2 = normals[idx2];
            mNormals.push_back(Normals{.n0 = n0, .n1 = n1, .n2 = n2});

            const glm::vec2& uv0 = texCoords[idx0];
            const glm::vec2& uv1 = texCoords[idx1];
            const glm::vec2& uv2 = texCoords[idx2];
            mTexCoords.push_back(TexCoords{.uv0 = uv0, .uv1 = uv1, .uv2 = uv2});

            mBaseColorTextureIndices.push_back(baseColorTextureIndex);
        }
    }
}
} // namespace nlrs
