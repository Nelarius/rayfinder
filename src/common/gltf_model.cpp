#include "gltf_model.hpp"
#include "texture.hpp"
#include "vector_set.hpp"

#include <cgltf.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator> // std::distance
#include <stdexcept>

namespace fs = std::filesystem;

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
    const glm::mat4 local = [node]() -> glm::mat4 {
        if (node->has_matrix)
        {
            return *reinterpret_cast<const glm::mat4*>(node->matrix);
        }
        else
        {
            const glm::quat& q = *reinterpret_cast<const glm::quat*>(node->rotation);
            const glm::mat4  scale = glm::scale(
                glm::mat4(1.0), glm::vec3(node->scale[0], node->scale[1], node->scale[2]));
            const glm::mat4 rotation = glm::toMat4(q);
            const glm::mat4 translation = glm::translate(
                glm::mat4(1.0),
                glm::vec3(node->translation[0], node->translation[1], node->translation[2]));
            return translation * rotation * scale;
        }
    }();

    const glm::mat4 transform = parentTransform * local;

    if (node->mesh != nullptr)
    {
        const auto distance = std::distance(meshBegin, const_cast<const cgltf_mesh*>(node->mesh));
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

GltfModel::GltfModel(const fs::path gltfPath)
    : mTriangles(),
      mBaseColorTextureIndices(),
      mBaseColorTextures()
{
    if (!fs::exists(gltfPath))
    {
        throw std::runtime_error(
            std::format("The gltf file {} does not exist.", gltfPath.string().c_str()));
    }

    cgltf_options                 options = {};
    cgltf_data*                   data = nullptr;
    [[maybe_unused]] cgltf_result result =
        cgltf_parse_file(&options, gltfPath.string().c_str(), &data);
    assert(result == cgltf_result_success);

    result = cgltf_load_buffers(&options, data, gltfPath.string().c_str());
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
        std::vector<glm::vec3>          positions;
        std::vector<std::uint32_t>      indices;
        std::vector<const cgltf_image*> baseColorImageAttributes;
        VectorSet<const cgltf_image*>   uniqueBaseColorImages;

        const std::size_t meshCount = data->meshes_count;
        for (std::size_t meshIdx = 0; meshIdx < meshCount; ++meshIdx)
        {
            const cgltf_mesh& mesh = data->meshes[meshIdx];
            for (std::size_t primitiveIdx = 0; primitiveIdx < mesh.primitives_count; ++primitiveIdx)
            {
                const cgltf_primitive& primitive = mesh.primitives[primitiveIdx];
                assert(primitive.type == cgltf_primitive_type_triangles);

                assert(primitive.material);
                assert(primitive.material->has_pbr_metallic_roughness);
                const cgltf_pbr_metallic_roughness& pbrMetallicRoughness =
                    primitive.material->pbr_metallic_roughness;

                assert(pbrMetallicRoughness.base_color_texture.texture);
                const cgltf_texture& baseColorTexture =
                    *pbrMetallicRoughness.base_color_texture.texture;

                assert(baseColorTexture.image);
                const cgltf_image* const baseColorImage = baseColorTexture.image;
                uniqueBaseColorImages.insert(baseColorImage);

                const cgltf_accessor* const indexAccessor = primitive.indices;
                assert(indexAccessor != nullptr);
                assert(indexAccessor->type == cgltf_type_scalar);

                const std::size_t indexOffset = static_cast<std::uint32_t>(positions.size());
                const std::size_t indexCount = indexAccessor->count;
                indices.resize(indexOffset + indexCount);
                std::span<std::uint32_t> indicesSpan =
                    std::span(indices).subspan(indexOffset, indexCount);
                assert((indexCount % 3) == 0);
                for (std::size_t i = 0; i < indexCount; i += 3)
                {
                    std::uint32_t&        idx1 = indicesSpan[i];
                    std::uint32_t&        idx2 = indicesSpan[i + 1];
                    std::uint32_t&        idx3 = indicesSpan[i + 2];
                    [[maybe_unused]] bool readSuccess =
                        cgltf_accessor_read_uint(indexAccessor, i, &idx1, 1);
                    assert(readSuccess);
                    readSuccess = cgltf_accessor_read_uint(indexAccessor, i + 1, &idx2, 1);
                    assert(readSuccess);
                    readSuccess = cgltf_accessor_read_uint(indexAccessor, i + 2, &idx3, 1);
                    assert(readSuccess);
                    idx1 += static_cast<std::uint32_t>(indexOffset);
                    idx2 += static_cast<std::uint32_t>(indexOffset);
                    idx3 += static_cast<std::uint32_t>(indexOffset);

                    baseColorImageAttributes.push_back(baseColorImage);
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

            Triangle tri{.v0 = p0, .v1 = p1, .v2 = p2};
            assert(surfaceArea(tri) > 0.00001f);
            mTriangles.push_back(tri);
        }

        auto bufferViewData =
            [](const cgltf_buffer_view* const bufferView) -> std::span<const std::uint8_t> {
            // NOTE: cgltf_buffer_view_data used as a reference for this function
            assert(bufferView != nullptr);
            const std::size_t byteLength = bufferView->size;

            // See cgltf_buffer_view::data comment, overrides buffer->data if present
            if (bufferView->data != nullptr)
            {
                const std::uint8_t* const bufferPtr =
                    static_cast<const std::uint8_t*>(bufferView->data);
                return std::span(bufferPtr, byteLength);
            }

            assert(bufferView->buffer != nullptr);
            const std::size_t         bufferOffset = bufferView->offset;
            const std::uint8_t* const bufferPtr =
                static_cast<const std::uint8_t*>(bufferView->buffer->data);

            return std::span(bufferPtr + bufferOffset, byteLength);
        };

        for (const cgltf_image* image : uniqueBaseColorImages)
        {
            if (image->buffer_view)
            {
                const auto pixelData = bufferViewData(image->buffer_view);
                mBaseColorTextures.push_back(Texture::fromMemory(pixelData));
            }
            else
            {
                assert(image->uri);
                const fs::path imagePath = gltfPath.parent_path() / image->uri;
                if (!fs::exists(imagePath))
                {
                    throw std::runtime_error(
                        std::format("The image {} does not exist.", imagePath.string()));
                }

                const std::size_t fileSize = static_cast<std::size_t>(fs::file_size(imagePath));
                assert(fileSize > 0);
                std::ifstream file(imagePath, std::ios::binary);
                assert(file.is_open());

                std::vector<std::uint8_t> fileData(fileSize, 0);
                file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
                mBaseColorTextures.push_back(Texture::fromMemory(std::span(fileData)));
            }
        }

        // Replace each triangle's base color image attribute pointer with an index into a unique
        // array of images.
        assert(baseColorImageAttributes.size() == mTriangles.size());

        for (const cgltf_image* image : baseColorImageAttributes)
        {
            const auto imageIter = uniqueBaseColorImages.find(image);
            assert(imageIter != uniqueBaseColorImages.end());
            const auto distance = std::distance(uniqueBaseColorImages.begin(), imageIter);
            assert(distance >= 0);
            const std::size_t textureIdx = static_cast<std::size_t>(distance);
            assert(textureIdx < mBaseColorTextures.size());
            mBaseColorTextureIndices.push_back(textureIdx);
        }
    }

    cgltf_free(data);
}
} // namespace pt
