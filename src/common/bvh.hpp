#pragma once

#include "geometry.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace pt
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
    std::vector<BvhNode>  nodes;
    std::vector<Triangle> triangles;
};

Bvh buildBvh(std::span<const Triangle> triangles);
} // namespace pt
