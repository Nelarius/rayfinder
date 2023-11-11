#pragma once

#include "geometry.hpp"

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
    std::vector<BvhNode>   nodes;
    std::vector<Positions> positions;
    // The positions are sorted so that leaf nodes point to contiguous ranges of triangle
    // attributes. `positionIndices` contains the new index of the position. It can be used to
    // reorder the remaining triangle attributes to match the order of the positions.
    std::vector<std::size_t> positionIndices;
};

Bvh buildBvh(std::span<const Positions> positions);

template<std::copyable T>
std::vector<T> reorderAttributes(
    const std::span<const T>           attributes,
    const std::span<const std::size_t> positionIndices)
{
    std::vector<T> reorderedAttributes(attributes.size());
    for (std::size_t i = 0; i < attributes.size(); ++i)
    {
        reorderedAttributes[positionIndices[i]] = attributes[i];
    }
    return reorderedAttributes;
}
} // namespace nlrs
