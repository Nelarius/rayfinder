#pragma once

#include "vertex_attributes.hpp"

#include <common/bvh.hpp>
#include <common/triangle_attributes.hpp>
#include <common/texture.hpp>

#include <filesystem>
#include <span>
#include <vector>

namespace nlrs
{
class InputStream;
class OutputStream;

struct PtFormat
{
    PtFormat() = default;
    PtFormat(std::filesystem::path gltfPath);

    std::vector<BvhNode> bvhNodes;
    // TODO: is this field actually used somewhere? from triangle_attributes.hpp
    std::vector<Positions>         bvhPositionAttributes;
    std::vector<PositionAttribute> trianglePositionAttributes;
    std::vector<VertexAttributes>  triangleVertexAttributes;

    std::vector<glm::vec4>                      vertexPositions;
    std::vector<glm::vec4>                      vertexNormals;
    std::vector<glm::vec2>                      vertexTexCoords;
    std::vector<std::uint32_t>                  vertexIndices;
    std::vector<std::span<const glm::vec4>>     modelVertexPositions;
    std::vector<std::span<const glm::vec4>>     modelVertexNormals;
    std::vector<std::span<const glm::vec2>>     modelVertexTexCoords;
    std::vector<std::span<const std::uint32_t>> modelvertexIndices;
    std::vector<std::size_t>                    modelBaseColorTextureIndices;

    std::vector<Texture> baseColorTextures;
};

void serialize(OutputStream&, const PtFormat&);
void deserialize(InputStream&, PtFormat&);
} // namespace nlrs
