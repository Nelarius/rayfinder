#include "gltf_model.hpp"

#include <cgltf.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <iterator> // std::distance
#include <stdexcept>

namespace pt
{
namespace
{
void traverseNodeHierarchy(
    const cgltf_node* const node,
    const cgltf_mesh* const meshBegin,
    const glm::mat4&        parentTransform,
    std::vector<glm::mat4>& transforms)
{
    // TODO: what if node->has_matrix is set?
    // NOTE: in that case, node->rotation, node->scale, and node->translation are identity
    // operations, and the matrix should be used instead.
    const glm::quat& q = *reinterpret_cast<const glm::quat*>(node->rotation);
    const glm::mat4  scale =
        glm::scale(glm::mat4(1.0), glm::vec3(node->scale[0], node->scale[1], node->scale[2]));
    const glm::mat4 rotation = glm::toMat4(q);
    const glm::mat4 translation = glm::translate(
        glm::mat4(1.0),
        glm::vec3(node->translation[0], node->translation[1], node->translation[2]));

    const glm::mat4 transform = parentTransform * translation * rotation * scale;

    if (node->mesh != nullptr)
    {
        const std::ptrdiff_t distance =
            std::distance(meshBegin, const_cast<const cgltf_mesh*>(node->mesh));
        assert(distance >= 0);
        assert(static_cast<std::size_t>(distance) < transforms.size());
        const std::size_t meshIdx = static_cast<std::size_t>(distance);
        transforms[meshIdx] = transform;
    }

    if (node->children != nullptr)
    {
        for (std::size_t childIdx = 0; childIdx < node->children_count; ++childIdx)
        {
            traverseNodeHierarchy(node->children[childIdx], meshBegin, transform, transforms);
        }
    }
}
} // namespace

GltfModel::GltfModel(const std::string_view gltfPath)
    : mTriangles()
{
    const std::filesystem::path path(gltfPath);
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error(std::format("The gltf file {} does not exist.", gltfPath));
    }

    cgltf_options                 options = {};
    cgltf_data*                   data = nullptr;
    [[maybe_unused]] cgltf_result result = cgltf_parse_file(&options, gltfPath.data(), &data);
    assert(result == cgltf_result_success);

    result = cgltf_load_buffers(&options, data, gltfPath.data());
    assert(result == cgltf_result_success);

    std::vector<glm::mat4> meshTransforms(data->meshes_count, glm::mat4(1.0));
    {
        assert(data->scenes_count == 1);
        const size_t count = data->scene->nodes_count;
        for (std::size_t nodeIdx = 0; nodeIdx < count; ++nodeIdx)
        {
            const cgltf_node* const node = data->scene->nodes[nodeIdx];
            traverseNodeHierarchy(node, data->meshes, glm::mat4(1.0f), meshTransforms);
        }
    }

    assert(data->scenes_count == 1);

    {
        std::vector<glm::vec3>     positions;
        std::vector<std::uint32_t> indices;

        const std::size_t meshCount = data->meshes_count;
        for (std::size_t meshIdx = 0; meshIdx < meshCount; ++meshIdx)
        {
            const cgltf_mesh& mesh = data->meshes[meshIdx];
            for (std::size_t primitiveIdx = 0; primitiveIdx < mesh.primitives_count; ++primitiveIdx)
            {
                const cgltf_primitive& primitive = mesh.primitives[primitiveIdx];
                assert(primitive.type == cgltf_primitive_type_triangles);

                const cgltf_accessor* const indexAccessor = primitive.indices;
                assert(indexAccessor != nullptr);
                assert(indexAccessor->type == cgltf_type_scalar);

                const std::size_t indexOffset = static_cast<std::uint32_t>(positions.size());
                const std::size_t indexCount = indexAccessor->count;
                indices.resize(indexOffset + indexCount);
                std::span<std::uint32_t> indicesSpan =
                    std::span(indices).subspan(indexOffset, indexCount);
                for (std::size_t i = 0; i < indexCount; ++i)
                {
                    std::uint32_t&              idx = indicesSpan[i];
                    [[maybe_unused]] const bool readSuccess =
                        cgltf_accessor_read_uint(indexAccessor, i, &idx, 1);
                    assert(readSuccess);
                    idx += static_cast<std::uint32_t>(indexOffset);
                }

                const cgltf_accessor* positionAccessor = nullptr;

                const std::size_t attributeCount = primitive.attributes_count;
                for (std::size_t i = 0; i < attributeCount; ++i)
                {
                    const cgltf_attribute& attribute = primitive.attributes[i];
                    if (attribute.type == cgltf_attribute_type_position)
                    {
                        positionAccessor = attribute.data;
                    }
                }

                assert(positionAccessor != nullptr);
                assert(positionAccessor->type == cgltf_type_vec3);
                assert(positionAccessor->component_type == cgltf_component_type_r_32f);

                const std::size_t positionCount = positionAccessor->count;
                positions.resize(indexOffset + positionCount);
                for (std::size_t idx = 0; idx < positionCount; ++idx)
                {
                    glm::vec3             position;
                    [[maybe_unused]] bool readSuccess =
                        cgltf_accessor_read_float(positionAccessor, idx, &position.x, 3);
                    assert(readSuccess);

                    positions[idx] = meshTransforms[meshIdx] * glm::vec4(position, 1.0f);
                }
            }
        }

        mTriangles.reserve(indices.size() / 3);
        for (std::size_t i = 0; i < indices.size(); i += 3)
        {
            const std::uint32_t idx0 = indices[i + 0];
            const std::uint32_t idx1 = indices[i + 1];
            const std::uint32_t idx2 = indices[i + 2];

            const glm::vec3& p0 = positions[idx0];
            const glm::vec3& p1 = positions[idx1];
            const glm::vec3& p2 = positions[idx2];

            Triangle t{p0, p1, p2};
            assert(surfaceArea(t) > 0.00001f);
            mTriangles.push_back(t);
        }
    }

    cgltf_free(data);
}
} // namespace pt
