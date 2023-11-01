#pragma once

#include <glm/glm.hpp>
#include <limits>

namespace pt
{
struct Aabb
{
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());

    Aabb() = default;
    Aabb(const glm::vec3& p1, const glm::vec3& p2)
        : min(glm::min(p1, p2)),
          max(glm::max(p1, p2))
    {
    }
};

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;
};

struct Triangle
{
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
};

inline glm::vec3 centroid(const Aabb& aabb) { return 0.5f * (aabb.min + aabb.max); }

inline glm::vec3 diagonal(const Aabb& aabb) { return aabb.max - aabb.min; }

inline int maxDimension(const Aabb& aabb)
{
    const glm::vec3 d = diagonal(aabb);
    if (d.x > d.y && d.x > d.z)
    {
        return 0;
    }
    else if (d.y > d.z)
    {
        return 1;
    }
    else
    {
        return 2;
    }
}

inline Aabb merge(const Aabb& b, const glm::vec3& p)
{
    return Aabb(glm::min(b.min, p), glm::max(b.max, p));
}

inline Aabb merge(const Aabb& lhs, const Aabb& rhs)
{
    return Aabb(glm::min(lhs.min, rhs.min), glm::max(lhs.max, rhs.max));
}

inline float surfaceArea(const Aabb& aabb)
{
    const glm::vec3 d = diagonal(aabb);
    return 2.0f * (d.x * d.y + d.x * d.z + d.y * d.z);
}

inline Aabb aabb(const Triangle& triangle)
{
    return Aabb(
        glm::min(glm::min(triangle.v0, triangle.v1), triangle.v2),
        glm::max(glm::max(triangle.v0, triangle.v1), triangle.v2));
}
} // namespace pt
