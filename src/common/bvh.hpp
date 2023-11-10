#pragma once

#include "geometry.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace nlrs
{
// 48-byte size BvhNode, for 16-byte aligned GPU memory.
struct BvhNode
{
    Aabb          aabb;              // offset: 0, size: 32
    std::uint32_t trianglesOffset;   // offset: 32, size: 4
    std::uint32_t secondChildOffset; // offset: 36, size: 4,
    std::uint32_t triangleCount;     // offset: 40, size: 4
    std::uint32_t splitAxis;         // offset: 44, size: 4
};

struct Bvh
{
    std::vector<BvhNode>       nodes;
    std::vector<Positions>     positions;
    std::vector<Normals>       normals;
    std::vector<TexCoords>     texCoords;
    std::vector<std::uint32_t> textureIndices;
};

Bvh buildBvh(
    std::span<const Positions>     positions,
    std::span<const Normals>       normals,
    std::span<const TexCoords>     texCoords,
    std::span<const std::uint32_t> textureIndices);
} // namespace nlrs
