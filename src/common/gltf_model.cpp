#include "assert.hpp"
#include "gltf_model.hpp"
#include "texture.hpp"
#include "vector_set.hpp"

#include <cgltf.h>
#include <fmt/core.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <stb_image.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <tuple>

namespace fs = std::filesystem;

namespace nlrs
{
namespace
{
void traverseNodeHierarchy(
    const cgltf_node* const                     node,
    const cgltf_mesh* const                     meshBegin,
    const glm::mat4&                            parentTransform,
    std::span<std::tuple<glm::mat4, glm::mat4>> transforms)
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

    const glm::mat4 transformMatrix = parentTransform * local;
    const glm::mat4 normalMatrix = glm::transpose(glm::inverse(transformMatrix));

    if (node->mesh != nullptr)
    {
        const auto distance = std::distance(meshBegin, const_cast<const cgltf_mesh*>(node->mesh));
        NLRS_ASSERT(distance >= 0);
        NLRS_ASSERT(static_cast<std::size_t>(distance) < transforms.size());
        const std::size_t meshIdx = static_cast<std::size_t>(distance);
        transforms[meshIdx] = std::make_tuple(transformMatrix, normalMatrix);
    }

    if (node->children != nullptr)
    {
        for (std::size_t childIdx = 0; childIdx < node->children_count; ++childIdx)
        {
            traverseNodeHierarchy(node->children[childIdx], meshBegin, transformMatrix, transforms);
        }
    }
}
} // namespace

GltfModel::GltfModel(const fs::path gltfPath)
    : meshes(),
      baseColorTextures()
{
    if (!fs::exists(gltfPath))
    {
        throw std::runtime_error(
            fmt::format("The gltf file {} does not exist.", gltfPath.string()));
    }

    cgltf_options options = {};
    cgltf_data*   data = nullptr;
    {
        cgltf_result result = cgltf_parse_file(&options, gltfPath.string().c_str(), &data);
        if (result != cgltf_result_success)
        {
            cgltf_free(data);
            throw std::runtime_error(
                fmt::format("Failed to parse gltf file {}.", gltfPath.string()));
        }

        result = cgltf_load_buffers(&options, data, gltfPath.string().c_str());
        if (result != cgltf_result_success)
        {
            cgltf_free(data);
            throw std::runtime_error(
                fmt::format("Failed to load gltf buffers for {}.", gltfPath.string()));
        }
    }
    NLRS_ASSERT(data != nullptr);

    std::vector<std::tuple<glm::mat4, glm::mat4>> meshTransforms(
        data->meshes_count, std::make_tuple(glm::mat4(1.0), glm::mat4(1.0)));
    {
        NLRS_ASSERT(data->scenes_count == 1);
        const size_t count = data->scene->nodes_count;
        for (std::size_t nodeIdx = 0; nodeIdx < count; ++nodeIdx)
        {
            const cgltf_node* const node = data->scene->nodes[nodeIdx];
            traverseNodeHierarchy(node, data->meshes, glm::mat4(1.0f), meshTransforms);
        }
    }

    NLRS_ASSERT(data->scenes_count == 1);

    std::vector<std::vector<glm::vec3>>     meshPositions;
    std::vector<std::vector<glm::vec3>>     meshNormals;
    std::vector<std::vector<glm::vec2>>     meshTexCoords;
    std::vector<std::vector<std::uint32_t>> meshIndices;
    std::vector<const cgltf_image*>         meshBaseColorImageAttributes;
    VectorSet<const cgltf_image*>           uniqueBaseColorImages;

    const std::size_t meshCount = data->meshes_count;
    for (std::size_t meshIdx = 0; meshIdx < meshCount; ++meshIdx)
    {
        const cgltf_mesh& mesh = data->meshes[meshIdx];
        for (std::size_t primitiveIdx = 0; primitiveIdx < mesh.primitives_count; ++primitiveIdx)
        {
            const cgltf_primitive& primitive = mesh.primitives[primitiveIdx];
            NLRS_ASSERT(primitive.type == cgltf_primitive_type_triangles);

            // Material
            {
                NLRS_ASSERT(primitive.material);
                NLRS_ASSERT(primitive.material->has_pbr_metallic_roughness);
                const cgltf_pbr_metallic_roughness& pbrMetallicRoughness =
                    primitive.material->pbr_metallic_roughness;

                NLRS_ASSERT(pbrMetallicRoughness.base_color_texture.texcoord == 0);
                NLRS_ASSERT(pbrMetallicRoughness.base_color_texture.scale == 1.f);
                NLRS_ASSERT(pbrMetallicRoughness.base_color_texture.texture);

                const cgltf_texture& baseColorTexture =
                    *pbrMetallicRoughness.base_color_texture.texture;
                // Look up OpenGL texture wrap modes:
                // https://registry.khronos.org/OpenGL/api/GL/glcorearb.h
                // GL_REPEAT: 10497
                // GL_MIRRORED_REPEAT: 33648
                // GL_CLAMP_TO_EDGE: 33071
                // GL_CLAMP_TO_BORDER: 33069
                NLRS_ASSERT(baseColorTexture.sampler->wrap_s == 10497);
                NLRS_ASSERT(baseColorTexture.sampler->wrap_t == 10497);
                NLRS_ASSERT(baseColorTexture.image);
                const cgltf_image* const baseColorImage = baseColorTexture.image;
                meshBaseColorImageAttributes.push_back(baseColorImage);
                uniqueBaseColorImages.insert(baseColorImage);
            }

            // Indices
            {
                const cgltf_accessor* const indexAccessor = primitive.indices;
                NLRS_ASSERT(indexAccessor != nullptr);
                NLRS_ASSERT(indexAccessor->type == cgltf_type_scalar);

                const std::size_t indexCount = indexAccessor->count;
                NLRS_ASSERT(indexCount % 3 == 0);

                std::vector<std::uint32_t> indices;
                indices.resize(indexCount);
                for (std::size_t i = 0; i < indexCount; i += 3)
                {
                    NLRS_ASSERT(cgltf_accessor_read_uint(indexAccessor, i + 0, &indices[i + 0], 1));
                    NLRS_ASSERT(cgltf_accessor_read_uint(indexAccessor, i + 1, &indices[i + 1], 1));
                    NLRS_ASSERT(cgltf_accessor_read_uint(indexAccessor, i + 2, &indices[i + 2], 1));
                }
                meshIndices.emplace_back(std::move(indices));
            }

            // Attributes
            {
                const cgltf_accessor* positionAccessor = nullptr;
                const cgltf_accessor* normalAccessor = nullptr;
                const cgltf_accessor* texCoordAccessor = nullptr;

                const std::size_t attributeCount = primitive.attributes_count;
                for (std::size_t i = 0; i < attributeCount; ++i)
                {
                    const cgltf_attribute& attribute = primitive.attributes[i];
                    if (attribute.type == cgltf_attribute_type_position)
                    {
                        NLRS_ASSERT(positionAccessor == nullptr);
                        positionAccessor = attribute.data;
                    }
                    else if (attribute.type == cgltf_attribute_type_normal)
                    {
                        NLRS_ASSERT(normalAccessor == nullptr);
                        normalAccessor = attribute.data;
                    }
                    else if (attribute.type == cgltf_attribute_type_texcoord)
                    {
                        NLRS_ASSERT(texCoordAccessor == nullptr);
                        texCoordAccessor = attribute.data;
                    }
                }

                NLRS_ASSERT(positionAccessor != nullptr);
                NLRS_ASSERT(positionAccessor->type == cgltf_type_vec3);
                NLRS_ASSERT(positionAccessor->component_type == cgltf_component_type_r_32f);

                NLRS_ASSERT(normalAccessor != nullptr);
                NLRS_ASSERT(normalAccessor->type == cgltf_type_vec3);
                NLRS_ASSERT(normalAccessor->component_type == cgltf_component_type_r_32f);

                NLRS_ASSERT(texCoordAccessor != nullptr);
                NLRS_ASSERT(texCoordAccessor->type == cgltf_type_vec2);
                NLRS_ASSERT(texCoordAccessor->component_type == cgltf_component_type_r_32f);

                NLRS_ASSERT(positionAccessor->count == normalAccessor->count);
                NLRS_ASSERT(positionAccessor->count == texCoordAccessor->count);

                const std::size_t vertexCount = positionAccessor->count;

                std::vector<glm::vec3> localPositions;
                localPositions.resize(vertexCount);
                NLRS_ASSERT(
                    cgltf_accessor_unpack_floats(
                        positionAccessor, &localPositions[0][0], 3 * vertexCount) ==
                    3 * vertexCount);
                std::vector<glm::vec3> positions;
                std::transform(
                    localPositions.begin(),
                    localPositions.end(),
                    std::back_inserter(positions),
                    [&matrices = meshTransforms[meshIdx]](const glm::vec3& p) -> glm::vec3 {
                        return std::get<0>(matrices) * glm::vec4(p, 1.0f);
                    });
                meshPositions.emplace_back(std::move(positions));

                std::vector<glm::vec3> localNormals;
                localNormals.resize(vertexCount);
                NLRS_ASSERT(
                    cgltf_accessor_unpack_floats(
                        normalAccessor, &localNormals[0][0], 3 * vertexCount) == 3 * vertexCount);
                std::vector<glm::vec3> normals;
                std::transform(
                    localNormals.begin(),
                    localNormals.end(),
                    std::back_inserter(normals),
                    [&matrices = meshTransforms[meshIdx]](const glm::vec3& n) -> glm::vec3 {
                        return std::get<1>(matrices) * glm::vec4(n, 0.0f);
                    });
                meshNormals.emplace_back(std::move(normals));

                std::vector<glm::vec2> texCoords;
                texCoords.resize(vertexCount);
                NLRS_ASSERT(
                    cgltf_accessor_unpack_floats(
                        texCoordAccessor, &texCoords[0][0], 2 * vertexCount) == 2 * vertexCount);
                meshTexCoords.emplace_back(std::move(texCoords));
            }
        }
    }

    // Mesh texture indices

    NLRS_ASSERT(meshPositions.size() == meshBaseColorImageAttributes.size());
    std::vector<std::uint32_t> meshBaseColorTextureIndices;
    meshBaseColorTextureIndices.reserve(meshBaseColorImageAttributes.size());
    std::transform(
        meshBaseColorImageAttributes.begin(),
        meshBaseColorImageAttributes.end(),
        std::back_inserter(meshBaseColorTextureIndices),
        [&](const cgltf_image* image) -> std::uint32_t {
            const auto imageIter = uniqueBaseColorImages.find(image);
            NLRS_ASSERT(imageIter != uniqueBaseColorImages.end());
            const auto distance = std::distance(uniqueBaseColorImages.begin(), imageIter);
            NLRS_ASSERT(distance >= 0);
            const auto textureIdx = static_cast<std::uint32_t>(distance);
            NLRS_ASSERT(textureIdx < meshBaseColorImageAttributes.size());
            return textureIdx;
        });

    // Model textures

    auto bufferViewData =
        [](const cgltf_buffer_view* const bufferView) -> std::span<const std::uint8_t> {
        // NOTE: cgltf_buffer_view_data used as a reference for this function
        NLRS_ASSERT(bufferView != nullptr);
        const std::size_t byteLength = bufferView->size;

        // See cgltf_buffer_view::data comment, overrides buffer->data if present
        if (bufferView->data != nullptr)
        {
            const std::uint8_t* const bufferPtr =
                static_cast<const std::uint8_t*>(bufferView->data);
            return std::span(bufferPtr, byteLength);
        }

        NLRS_ASSERT(bufferView->buffer != nullptr);
        const std::size_t         bufferOffset = bufferView->offset;
        const std::uint8_t* const bufferPtr =
            static_cast<const std::uint8_t*>(bufferView->buffer->data);

        return std::span(bufferPtr + bufferOffset, byteLength);
    };

    baseColorTextures.reserve(uniqueBaseColorImages.size());
    std::transform(
        uniqueBaseColorImages.begin(),
        uniqueBaseColorImages.end(),
        std::back_inserter(baseColorTextures),
        [&](const cgltf_image* image) -> Texture {
            if (image->buffer_view)
            {
                const auto pixelData = bufferViewData(image->buffer_view);
                return Texture::fromMemory(pixelData);
            }
            else
            {
                NLRS_ASSERT(image->uri);
                const fs::path imagePath = gltfPath.parent_path() / image->uri;
                if (!fs::exists(imagePath))
                {
                    throw std::runtime_error(
                        fmt::format("The image {} does not exist.", imagePath.string()));
                }

                const std::size_t fileSize = static_cast<std::size_t>(fs::file_size(imagePath));
                NLRS_ASSERT(fileSize > 0);
                std::ifstream file(imagePath, std::ios::binary);
                NLRS_ASSERT(file.is_open());

                std::vector<std::uint8_t> fileData(fileSize, 0);
                file.read(reinterpret_cast<char*>(fileData.data()), fileSize);
                return Texture::fromMemory(std::span(fileData));
            }
        });

    meshes.reserve(meshPositions.size());
    for (std::size_t i = 0; i < meshPositions.size(); ++i)
    {
        meshes.emplace_back(
            std::move(meshPositions[i]),
            std::move(meshNormals[i]),
            std::move(meshTexCoords[i]),
            std::move(meshIndices[i]),
            meshBaseColorTextureIndices[i]);
    }

    cgltf_free(data);

    std::sort(meshes.begin(), meshes.end(), [](const GltfMesh& a, const GltfMesh& b) -> bool {
        return a.baseColorTextureIndex < b.baseColorTextureIndex;
    });
}

GltfModel::GltfModel(std::vector<GltfMesh> meshes, std::vector<Texture> baseColorTextures)
    : meshes(std::move(meshes)),
      baseColorTextures(std::move(baseColorTextures))
{
}
} // namespace nlrs
