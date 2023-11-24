#pragma once

#include "geometry.hpp"
#include "triangle_attributes.hpp"

#include <concepts>
#include <cstdint>
#include <cstddef>
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
    std::vector<BvhNode> nodes;
    // TODO: are these even needed?
    std::vector<Positions> triangles;
    // The positions are sorted so that leaf nodes point to contiguous ranges of triangle
    // attributes. `triangleIndices` contains the new index of the position. It can be used to
    // reorder the remaining triangle attributes to match the order of the positions.
    std::vector<std::size_t> triangleIndices;
};

Bvh buildBvh(std::span<const Positions> triangles);

template<std::copyable T>
std::vector<T> reorderAttributes(
    const std::span<const T>           attributes,
    const std::span<const std::size_t> triangleIndices)
{
    std::vector<T> reorderedAttributes(attributes.size());
    for (std::size_t i = 0; i < attributes.size(); ++i)
    {
        reorderedAttributes[triangleIndices[i]] = attributes[i];
    }
    return reorderedAttributes;
}
} // namespace nlrs
