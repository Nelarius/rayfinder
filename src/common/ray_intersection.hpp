#pragma once

#include <glm/glm.hpp>

namespace pt
{
struct Aabb32;
struct Bvh;
struct Ray;
struct Triangle48;

struct Intersection
{
    glm::vec3 p;
    float     t;
};

bool rayIntersectTriangle(
    const Ray&        ray,
    const Triangle48& tri,
    float             tMax,
    Intersection&     intersect);

struct RayAabbIntersector
{
    glm::vec3 origin;
    glm::vec3 invDir;
    uint32_t  dirNeg[3];

    explicit RayAabbIntersector(const Ray& ray);
};

bool rayIntersectAabb(const RayAabbIntersector& intersector, const Aabb32& aabb, float rayTMax);

struct BvhStats
{
    uint32_t nodesVisited;
};

bool rayIntersectBvh(
    const Ray&    ray,
    const Bvh&    bvh,
    float         rayTMax,
    Intersection& intersect,
    BvhStats*     stats = nullptr);
} // namespace pt
