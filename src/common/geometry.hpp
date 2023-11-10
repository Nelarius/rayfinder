#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <limits>

namespace nlrs
{
// Vector elements are 16-byte aligned, for GPU memory.
struct Aabb
{
    glm::vec3 min = glm::vec3(std::numeric_limits<float>::max());
    float     pad0 = 0.0f;
    glm::vec3 max = glm::vec3(std::numeric_limits<float>::lowest());
    float     pad1 = 0.0f;

    Aabb() = default;
    Aabb(const glm::vec3& p1, const glm::vec3& p2)
        : min(glm::min(p1, p2)),
          pad0(0.0f),
          max(glm::max(p1, p2)),
          pad1(0.0f)
    {
    }
};

// Contains the vertex positions of a triangle. Has 16-byte alignment for use as a storage buffer of
// array<array<vec3f, 3>>.
struct Positions
{
    glm::vec3 v0;
    float     pad0;
    glm::vec3 v1;
    float     pad1;
    glm::vec3 v2;
    float     pad2;
};

// Contains the vertex normals of a triangle. Has 16-byte alignment for use as a storage buffer of
// array<array<vec3f, 3>>.
struct Normals
{
    glm::vec3 n0;
    float     pad0;
    glm::vec3 n1;
    float     pad1;
    glm::vec3 n2;
    float     pad2;
};

// Contains the vertex texture coordinates of a triangle. Has 8-byte alignment for use as a storage
// buffer of array<array<vec2f, 3>>.
struct TexCoords
{
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec2 uv2;
};

// Contains the vertex positions of a triangle. Is intended to have the same layout as the Positions
// struct.
struct Triangle
{
    glm::vec3 v0;
    float     pad0;
    glm::vec3 v1;
    float     pad1;
    glm::vec3 v2;
    float     pad2;
};

static_assert(
    sizeof(Positions) == sizeof(Triangle),
    "Positions and Triangle must have the same layout");
static_assert(
    offsetof(Positions, v0) == offsetof(Triangle, v0),
    "Positions and Triangle must have the same layout");
static_assert(
    offsetof(Positions, v1) == offsetof(Triangle, v1),
    "Positions and Triangle must have the same layout");
static_assert(
    offsetof(Positions, v2) == offsetof(Triangle, v2),
    "Positions and Triangle must have the same layout");

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;
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

inline float surfaceArea(const Triangle& t)
{
    return 0.5f * glm::length(glm::cross(t.v1 - t.v0, t.v2 - t.v0));
}

inline Aabb aabb(const Triangle& triangle)
{
    return Aabb(
        glm::min(glm::min(triangle.v0, triangle.v1), triangle.v2),
        glm::max(glm::max(triangle.v0, triangle.v1), triangle.v2));
}
} // namespace nlrs
