#pragma once

#include "vertex_attributes.hpp"

#include <common/bvh.hpp>
#include <common/triangle_attributes.hpp>
#include <common/texture.hpp>

#include <filesystem>
#include <vector>

namespace nlrs
{
class InputStream;
class OutputStream;

struct PtFormat
{
    PtFormat() = default;
    PtFormat(std::filesystem::path gltfPath);

    std::vector<BvhNode>           bvhNodes;
    std::vector<Positions>         bvhPositionAttributes;
    std::vector<PositionAttribute> trianglePositionAttributes;
    std::vector<VertexAttributes>  triangleVertexAttributes;

    std::vector<Texture> baseColorTextures;
};

void serialize(OutputStream&, const PtFormat&);
void deserialize(InputStream&, PtFormat&);
} // namespace nlrs
