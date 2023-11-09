#include "bvh.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>

namespace pt
{
namespace
{
struct BvhPrimitive
{
    Aabb        aabb;
    glm::vec3   centroid;
    std::size_t triangleIdx;
};

struct BvhSplitBucket
{
    std::size_t count;
    Aabb        aabb;

    BvhSplitBucket()
        : count(0),
          aabb()
    {
    }
};

void initLeafNode(
    BvhNode&            node,
    const Aabb&         bounds,
    const std::uint32_t trianglesOffset,
    const std::uint32_t count)
{
    node.aabb = bounds;
    node.secondChildOffset = 0;
    node.trianglesOffset = trianglesOffset;
    node.triangleCount = count;
    node.splitAxis = static_cast<std::uint32_t>(-1);
}

void initInteriorNode(
    BvhNode&      node,
    std::uint32_t axis,
    std::uint32_t secondChildOffset,
    const Aabb&   childAabb)
{
    node.aabb = childAabb;
    node.secondChildOffset = secondChildOffset;
    node.trianglesOffset = 0;
    node.triangleCount = 0;
    node.splitAxis = axis;
}

void buildLeafNode(
    BvhNode&                            node,
    const Aabb&                         nodeAabb,
    const std::span<const Positions>    positions,
    const std::span<const Normals>      normals,
    const std::span<const TexCoords>    texCoords,
    const std::span<const BvhPrimitive> bvhPrimitives,
    std::span<Positions>                orderedPositions,
    std::span<Normals>                  orderedNormals,
    std::span<TexCoords>                orderedTexCoords,
    const std::size_t                   orderedTrianglesOffset)
{
    const std::size_t trianglesOffset = orderedTrianglesOffset;
    const std::size_t triangleCount = bvhPrimitives.size();
    for (std::size_t spanIdx = 0; spanIdx < triangleCount; ++spanIdx)
    {
        const std::size_t triangleIdx = bvhPrimitives[spanIdx].triangleIdx;
        orderedPositions[trianglesOffset + spanIdx] = positions[triangleIdx];
        orderedNormals[trianglesOffset + spanIdx] = normals[triangleIdx];
        orderedTexCoords[trianglesOffset + spanIdx] = texCoords[triangleIdx];
    }
    assert(trianglesOffset < std::numeric_limits<std::uint32_t>::max());
    assert(triangleCount < std::numeric_limits<std::uint32_t>::max());
    initLeafNode(
        node,
        nodeAabb,
        static_cast<std::uint32_t>(trianglesOffset),
        static_cast<std::uint32_t>(triangleCount));
}

std::size_t buildRecursive(
    const std::span<const Positions> positions,
    const std::span<const Normals>   normals,
    const std::span<const TexCoords> texCoords,
    std::span<BvhPrimitive>          bvhPrimitives,
    std::vector<BvhNode>&            bvhNodes,
    std::vector<Positions>&          orderedPositions,
    std::vector<Normals>&            orderedNormals,
    std::vector<TexCoords>&          orderedTexCoords,
    const std::size_t                orderedTrianglesOffset)
{
    assert(positions.size() == orderedPositions.size());
    assert(normals.size() == orderedNormals.size());
    assert(texCoords.size() == orderedTexCoords.size());
    assert(bvhPrimitives.size() >= 1);

    // Insert new node in memory. Even though we don't reference it yet, recursive function calls
    // need to account for it.
    const std::size_t currentNodeIdx = bvhNodes.size();
    bvhNodes.emplace_back();

    // Compute AABBs for node primitives and primitive centroids

    Aabb nodeAabb;
    Aabb centroidAabb;
    for (const BvhPrimitive& primitive : bvhPrimitives)
    {
        nodeAabb = merge(nodeAabb, primitive.aabb);
        centroidAabb = merge(centroidAabb, primitive.centroid);
    }
    const int splitAxis = maxDimension(centroidAabb);

    // Validate node & centroid AABBs. Terminate as leaf node if degenerate.
    // Check for leaf node conditions (primitive count 1).

    const std::size_t primitiveCount = bvhPrimitives.size();
    if (surfaceArea(nodeAabb) == 0.0f ||
        centroidAabb.min[splitAxis] == centroidAabb.max[splitAxis] || primitiveCount == 1)
    {
        buildLeafNode(
            bvhNodes[currentNodeIdx],
            nodeAabb,
            positions,
            normals,
            texCoords,
            bvhPrimitives,
            orderedPositions,
            orderedNormals,
            orderedTexCoords,
            orderedTrianglesOffset);
        return currentNodeIdx;
    }

    // Partition primitives into two sets using the surface area heuristic (SAH).

    std::size_t splitIdx;
    if (bvhPrimitives.size() < 3)
    {
        // Not worth evaluating SAH for less then 3 primitives. Do equal count split.
        splitIdx = bvhPrimitives.size() / 2;
        std::nth_element(
            bvhPrimitives.begin(),
            bvhPrimitives.begin() + splitIdx,
            bvhPrimitives.end(),
            [splitAxis](const BvhPrimitive& a, const BvhPrimitive& b) -> bool {
                return a.centroid[splitAxis] < b.centroid[splitAxis];
            });
    }
    else
    {
        // Partition triangles using SAH heuristic.

        constexpr std::size_t maxTrianglesInNode = 255;
        constexpr std::size_t numBuckets = 12;
        constexpr float       traversalCost = 0.5f;
        constexpr float       intersectionCost = 1.0f;

        BvhSplitBucket buckets[numBuckets];

        // Initialize buckets
        for (const BvhPrimitive& tri : bvhPrimitives)
        {
            std::size_t bucketIdx = static_cast<std::size_t>(
                numBuckets * (tri.centroid[splitAxis] - centroidAabb.min[splitAxis]) /
                (centroidAabb.max[splitAxis] - centroidAabb.min[splitAxis]));
            bucketIdx = std::min(bucketIdx, numBuckets - 1);
            buckets[bucketIdx].count++;
            buckets[bucketIdx].aabb = merge(buckets[bucketIdx].aabb, tri.aabb);
        }

        // Compute cost for each split
        {
            constexpr std::size_t numSplits = numBuckets - 1;
            float                 intersectionCosts[numSplits] = {0};

            std::size_t countBelow = 0;
            Aabb        aabbBelow;
            for (std::size_t i = 0; i < numSplits; ++i)
            {
                countBelow += buckets[i].count;
                aabbBelow = merge(aabbBelow, buckets[i].aabb);
                intersectionCosts[i] += intersectionCost * countBelow * surfaceArea(aabbBelow);
            }

            std::size_t countAbove = 0;
            Aabb        aabbAbove;
            for (std::size_t i = numSplits; i > 0; --i)
            {
                countAbove += buckets[i].count;
                aabbAbove = merge(aabbAbove, buckets[i].aabb);
                intersectionCosts[i - 1] += intersectionCost * countAbove * surfaceArea(aabbAbove);
            }

            // Find the intersection which minimizes the SAH metric
            float       minCost = std::numeric_limits<float>::max();
            std::size_t splitBucketIdx = static_cast<std::size_t>(-1);
            for (std::size_t i = 0; i < numSplits; ++i)
            {
                if (intersectionCosts[i] < minCost)
                {
                    minCost = intersectionCosts[i];
                    splitBucketIdx = i;
                }
            }

            // Compute the leaf cost and total cost.
            //
            // Leaf cost is defined as sum(intersection cost over primitives in leaf).
            //
            // Total cost is defined as (traverse cost) + p_A * sum(leaf cost A) + p_B *
            // sum(leaf cost B), where p_A and p_B are the probabilities of traversing to the
            // left and right child respectively. The probabilities are derived from the surface
            // area ratios.
            const float leafCost = intersectionCost * static_cast<float>(bvhPrimitives.size());
            const float totalCost = traversalCost + minCost / surfaceArea(nodeAabb);

            if (bvhPrimitives.size() > maxTrianglesInNode || totalCost < leafCost)
            {
                auto splitIter = std::partition(
                    bvhPrimitives.begin(),
                    bvhPrimitives.end(),
                    [centroidAabb, splitBucketIdx, splitAxis](const BvhPrimitive& prim) -> bool {
                        std::size_t bucketIdx = static_cast<std::size_t>(
                            numBuckets * (prim.centroid[splitAxis] - centroidAabb.min[splitAxis]) /
                            (centroidAabb.max[splitAxis] - centroidAabb.min[splitAxis]));
                        bucketIdx = std::min(bucketIdx, numBuckets - 1);
                        return bucketIdx <= splitBucketIdx;
                    });
                splitIdx =
                    static_cast<std::size_t>(std::distance(bvhPrimitives.begin(), splitIter));
                assert(splitIdx > 0);
                assert(splitIdx < bvhPrimitives.size());
            }
            else
            {
                buildLeafNode(
                    bvhNodes[currentNodeIdx],
                    nodeAabb,
                    positions,
                    normals,
                    texCoords,
                    bvhPrimitives,
                    orderedPositions,
                    orderedNormals,
                    orderedTexCoords,
                    orderedTrianglesOffset);
                return currentNodeIdx;
            }
        }
    }

    // Build children recursively

    buildRecursive(
        positions,
        normals,
        texCoords,
        bvhPrimitives.subspan(0, splitIdx),
        bvhNodes,
        orderedPositions,
        orderedNormals,
        orderedTexCoords,
        orderedTrianglesOffset);
    const std::size_t secondChildOffset = buildRecursive(
        positions,
        normals,
        texCoords,
        bvhPrimitives.subspan(splitIdx),
        bvhNodes,
        orderedPositions,
        orderedNormals,
        orderedTexCoords,
        orderedTrianglesOffset + splitIdx);

    assert(splitAxis <= 2);
    assert(secondChildOffset < std::numeric_limits<std::uint32_t>::max());
    initInteriorNode(
        bvhNodes[currentNodeIdx],
        static_cast<std::uint32_t>(splitAxis),
        static_cast<std::uint32_t>(secondChildOffset),
        nodeAabb);

    return currentNodeIdx;
}
} // namespace

Bvh buildBvh(
    std::span<const Positions> positions,
    std::span<const Normals>   normals,
    std::span<const TexCoords> texCoords)
{
    assert(!positions.empty());
    assert(!normals.empty());
    assert(!texCoords.empty());
    assert(positions.size() == normals.size());
    assert(positions.size() == texCoords.size());

    const std::size_t         numTriangles = positions.size();
    std::vector<BvhPrimitive> bvhPrimitives;
    bvhPrimitives.reserve(positions.size());
    for (std::size_t idx = 0; idx < numTriangles; ++idx)
    {
        const Triangle& tri = reinterpret_cast<const Triangle&>(positions[idx]);
        const Aabb      triAabb = aabb(tri);
        bvhPrimitives.push_back(BvhPrimitive{
            .aabb = triAabb,
            .centroid = centroid(triAabb),
            .triangleIdx = idx,
        });
    }

    std::vector<Positions> orderedPositions(numTriangles);
    std::vector<Normals>   orderedNormals(numTriangles);
    std::vector<TexCoords> orderedTexCoords(numTriangles);
    std::vector<BvhNode>   bvhNodes;
    bvhNodes.reserve(1024);

    buildRecursive(
        positions,
        normals,
        texCoords,
        bvhPrimitives,
        bvhNodes,
        orderedPositions,
        orderedNormals,
        orderedTexCoords,
        0);

    return Bvh{
        .nodes = std::move(bvhNodes),
        .positions = std::move(orderedPositions),
        .normals = std::move(orderedNormals),
        .texCoords = std::move(orderedTexCoords),
    };
}
} // namespace pt
