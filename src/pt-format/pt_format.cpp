#include "pt_format.hpp"

#include <common/assert.hpp>
#include <common/gltf_model.hpp>
#include <common/flattened_model.hpp>

#include <span>
#include <utility>

namespace nlrs
{
PtFormat::PtFormat(std::filesystem::path gltfPath)
    : bvhNodes(),
      bvhPositionAttributes(),
      gpuPositionAttributes(),
      gpuVertexAttributes(),
      baseColorTextures()
{
    nlrs::GltfModel      model{gltfPath};
    const FlattenedModel flattenedModel{model};
    auto [nodes, triangleIndices] = nlrs::buildBvh(flattenedModel.positions);

    auto positions = nlrs::reorderAttributes(std::span(flattenedModel.positions), triangleIndices);
    const auto normals =
        nlrs::reorderAttributes(std::span(flattenedModel.normals), triangleIndices);
    const auto texCoords =
        nlrs::reorderAttributes(std::span(flattenedModel.texCoords), triangleIndices);
    const auto textureIndices =
        nlrs::reorderAttributes(std::span(flattenedModel.baseColorTextureIndices), triangleIndices);
    NLRS_ASSERT(positions.size() == normals.size());
    NLRS_ASSERT(positions.size() == texCoords.size());
    NLRS_ASSERT(positions.size() == textureIndices.size());

    std::vector<nlrs::PositionAttribute> positionAttributes;
    std::vector<nlrs::VertexAttributes>  vertexAttributes;
    positionAttributes.reserve(positions.size());
    vertexAttributes.reserve(positions.size());
    for (std::size_t i = 0; i < positions.size(); ++i)
    {
        const auto& ps = positions[i];
        const auto& ns = normals[i];
        const auto& uvs = texCoords[i];
        const auto  textureIdx = textureIndices[i];

        positionAttributes.push_back(
            nlrs::PositionAttribute{.p0 = ps.v0, .p1 = ps.v1, .p2 = ps.v2});
        vertexAttributes.push_back(nlrs::VertexAttributes{
            .n0 = ns.n0,
            .n1 = ns.n1,
            .n2 = ns.n2,
            .uv0 = uvs.uv0,
            .uv1 = uvs.uv1,
            .uv2 = uvs.uv2,
            .textureIdx = textureIdx});
    }

    bvhNodes = std::move(nodes);
    bvhPositionAttributes = std::move(positions);
    gpuPositionAttributes = std::move(positionAttributes);
    gpuVertexAttributes = std::move(vertexAttributes);
    baseColorTextures = std::move(model.baseColorTextures);
}
} // namespace nlrs
