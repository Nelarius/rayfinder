#pragma once

#include "vertex_attributes.hpp"

#include <common/bvh.hpp>
#include <common/triangle_attributes.hpp>
#include <common/stream.hpp>
#include <common/texture.hpp>

#include <filesystem>
#include <vector>

namespace nlrs
{
struct PtFormat
{
    PtFormat() = default;
    PtFormat(std::filesystem::path gltfPath);

    std::vector<BvhNode>           bvhNodes;
    std::vector<Positions>         bvhPositionAttributes;
    std::vector<PositionAttribute> gpuPositionAttributes;
    std::vector<VertexAttributes>  gpuVertexAttributes;
    std::vector<Texture>           baseColorTextures;
};
} // namespace nlrs
