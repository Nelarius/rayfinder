#pragma once

#include <glm/glm.hpp>

namespace pt
{
struct Aabb;
struct Ray;
struct Triangle;

struct Intersection
{
    glm::vec3 p;
    float     t;
};

bool rayIntersectTriangle(const Ray& ray, const Triangle& tri, float tMax, Intersection& intersect);

// TODO: ray-AABB intersection
} // namespace pt
