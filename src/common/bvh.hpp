#pragma once

#include "geometry.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace pt
{
struct BvhNode
{
    Aabb aabb; // 24 bytes size
    union
    {
        std::uint32_t trianglesOffset;
        std::uint32_t secondChildOffset;
    };                           // 4 bytes size
    std::uint16_t triangleCount; // 2 bytes size
    std::uint16_t splitAxis;     // 2 bytes size
};

struct Bvh
{
    std::vector<BvhNode>  nodes;
    std::vector<Triangle> triangles;
};

Bvh buildBvh(std::span<const Triangle> triangles);
} // namespace pt
