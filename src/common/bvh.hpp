#pragma once

#include "geometry.hpp"

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace pt
{
// 32 byte-sized Aabb, for 16 byte aligned GPU memory.
struct Aabb32
{
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    float     pad0;
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());
    float     pad1;

    Aabb32() = default;
    explicit Aabb32(const Aabb& aabb)
        : min(aabb.min),
          pad0(0.0f),
          max(aabb.max),
          pad1(0.0f)
    {
    }
};

// 48-byte sized Triangle, for 16 byte aligned GPU memory.
struct Triangle48
{
    glm::vec3 v0;
    float     pad0;
    glm::vec3 v1;
    float     pad1;
    glm::vec3 v2;
    float     pad2;

    Triangle48() = default;
    explicit Triangle48(const Triangle& triangle)
        : v0(triangle.v0),
          pad0(0.0f),
          v1(triangle.v1),
          pad1(0.0f),
          v2(triangle.v2),
          pad2(0.0f)
    {
    }
};

// 48-byte size BvhNode, for 16-byte aligned GPU memory.
struct BvhNode
{
    Aabb32        aabb;              // offset: 0, size: 32
    std::uint32_t trianglesOffset;   // offset: 32, size: 4
    std::uint32_t secondChildOffset; // offset: 36, size: 4,
    std::uint32_t triangleCount;     // offset: 40, size: 4
    std::uint32_t splitAxis;         // offset: 44, size: 4
};

struct Bvh
{
    std::vector<BvhNode>    nodes;
    std::vector<Triangle48> triangles;
};

Bvh buildBvh(std::span<const Triangle> triangles);
} // namespace pt
