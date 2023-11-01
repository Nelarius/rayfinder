#include "geometry.hpp"
#include "ray_intersection.hpp"

namespace pt
{
bool rayIntersectTriangle(
    const Ray&      ray,
    const Triangle& tri,
    const float     rayTMax,
    Intersection&   intersect)
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
        intersect.p = ray.origin + ray.direction * t;
        intersect.t = t;
        return true;
    }
    else
    {
        return false;
    }
}
} // namespace pt
