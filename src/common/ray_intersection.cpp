#include "aabb.hpp"
#include "bvh.hpp"
#include "ray.hpp"
#include "ray_intersection.hpp"
#include "triangle_attributes.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>

namespace nlrs
{
namespace
{
glm::vec3 offsetRay(const glm::vec3& p, const glm::vec3& n)
{
    constexpr float ORIGIN = 1.0f / 32.0f;
    constexpr float FLOAT_SCALE = 1.0f / 65536.0f;
    constexpr float INT_SCALE = 256.0f;

    const glm::ivec3 offset =
        glm::ivec3(int(INT_SCALE * n.x), int(INT_SCALE * n.y), int(INT_SCALE * n.z));

    const glm::vec3 po = glm::vec3(
        std::bit_cast<float>(std::bit_cast<int>(p.x) + (p.x < 0 ? -offset.x : offset.x)),
        std::bit_cast<float>(std::bit_cast<int>(p.y) + (p.y < 0 ? -offset.y : offset.y)),
        std::bit_cast<float>(std::bit_cast<int>(p.z) + (p.z < 0 ? -offset.z : offset.z)));

    return glm::vec3(
        std::abs(p.x) < ORIGIN ? p.x + FLOAT_SCALE * n.x : po.x,
        std::abs(p.y) < ORIGIN ? p.y + FLOAT_SCALE * n.y : po.y,
        std::abs(p.z) < ORIGIN ? p.z + FLOAT_SCALE * n.z : po.z);
}
} // namespace

bool rayIntersectTriangle(
    const Ray&       ray,
    const Positions& tri,
    const float      rayTMax,
    Intersection&    intersect)
{
    constexpr float EPSILON = 0.00001f;

    // Mäller-Trumbore algorithm
    // https://en.wikipedia.org/wiki/Möller–Trumbore_intersection_algorithm
    const glm::vec3 e1 = tri.v1 - tri.v0;
    const glm::vec3 e2 = tri.v2 - tri.v0;

    const glm::vec3 h = glm::cross(ray.direction, e2);
    const float     det = glm::dot(e1, h);

    if (det > -EPSILON && det < EPSILON)
    {
        return false;
    }

    const float     invDet = 1.0f / det;
    const glm::vec3 s = ray.origin - tri.v0;
    const float     u = invDet * glm::dot(s, h);

    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    const glm::vec3 q = glm::cross(s, e1);
    const float     v = invDet * glm::dot(ray.direction, q);

    if (v < 0.0f || u + v > 1.0f)
    {
        return false;
    }

    const float t = invDet * glm::dot(e2, q);

    if (t > EPSILON && t < rayTMax)
    {
        const glm::vec3 p = tri.v0 + u * e1 + v * e2;
        const glm::vec3 n = glm::normalize(glm::cross(e1, e2));
        intersect.p = offsetRay(p, n);
        intersect.t = t;
        return true;
    }
    else
    {
        return false;
    }
}

RayAabbIntersector::RayAabbIntersector(const Ray& ray)
{
    origin = ray.origin;
    invDir = 1.0f / ray.direction;
    dirNeg[0] = static_cast<std::uint32_t>(invDir.x < 0.0f);
    dirNeg[1] = static_cast<std::uint32_t>(invDir.y < 0.0f);
    dirNeg[2] = static_cast<std::uint32_t>(invDir.z < 0.0f);
}

bool rayIntersectAabb(const RayAabbIntersector& intersector, const Aabb& aabb, const float rayTMax)
{
    const glm::vec3 bounds[2] = {aabb.min, aabb.max};

    float tmin = (bounds[intersector.dirNeg[0]].x - intersector.origin.x) * intersector.invDir.x;
    float tmax =
        (bounds[1 - intersector.dirNeg[0]].x - intersector.origin.x) * intersector.invDir.x;

    const float tymin =
        (bounds[intersector.dirNeg[1]].y - intersector.origin.y) * intersector.invDir.y;
    const float tymax =
        (bounds[1 - intersector.dirNeg[1]].y - intersector.origin.y) * intersector.invDir.y;

    if ((tmin > tymax) || (tymin > tmax))
    {
        return false;
    }

    tmin = std::max(tymin, tmin);
    tmax = std::min(tymax, tmax);

    const float tzmin =
        (bounds[intersector.dirNeg[2]].z - intersector.origin.z) * intersector.invDir.z;
    const float tzmax =
        (bounds[1 - intersector.dirNeg[2]].z - intersector.origin.z) * intersector.invDir.z;

    if ((tmin > tzmax) || (tzmin > tmax))
    {
        return false;
    }

    tmin = std::max(tzmin, tmin);
    tmax = std::min(tzmax, tmax);

    return (tmin < rayTMax) && (tmax > 0.0f);
}

bool rayIntersectBvh(
    const Ray&                       ray,
    const std::span<const BvhNode>   bvhNodes,
    const std::span<const Positions> triangles,
    float                            rayTMax,
    Intersection&                    intersect,
    BvhStats*                        stats)
{
    const RayAabbIntersector intersector(ray);

    constexpr std::size_t STACK_SIZE = 32;

    std::uint32_t nodesVisited = 0;
    std::size_t   toVisitOffset = 0;
    std::size_t   currentNodeIdx = 0;
    std::size_t   nodesToVisit[STACK_SIZE];
    bool          didIntersect = false;

    while (true)
    {
        ++nodesVisited;
        const BvhNode& node = bvhNodes[currentNodeIdx];

        // Check ray against BVH node
        if (rayIntersectAabb(intersector, node.aabb, rayTMax))
        {
            if (node.triangleCount > 0)
            {
                // Check for intersection with primitives in BVH node
                for (std::size_t idx = 0; idx < node.triangleCount; ++idx)
                {
                    const Positions& triangle = triangles[node.trianglesOffset + idx];
                    if (rayIntersectTriangle(ray, triangle, rayTMax, intersect))
                    {
                        rayTMax = intersect.t;
                        didIntersect = true;
                    }
                }
                if (toVisitOffset == 0)
                {
                    break;
                }
                currentNodeIdx = nodesToVisit[--toVisitOffset];
            }
            else
            {
                if (intersector.dirNeg[node.splitAxis])
                {
                    nodesToVisit[toVisitOffset++] = currentNodeIdx + 1;
                    currentNodeIdx = node.secondChildOffset;
                }
                else
                {
                    nodesToVisit[toVisitOffset++] = node.secondChildOffset;
                    currentNodeIdx = currentNodeIdx + 1;
                }
                assert(toVisitOffset < STACK_SIZE);
            }
        }
        else
        {
            if (toVisitOffset == 0)
            {
                break;
            }
            currentNodeIdx = nodesToVisit[--toVisitOffset];
        }
    }

    if (stats != nullptr)
    {
        stats->nodesVisited = nodesVisited;
    }

    return didIntersect;
}
} // namespace nlrs
