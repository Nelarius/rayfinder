#pragma once

#include <glm/glm.hpp>

namespace nlrs
{
struct Positions
{
    glm::vec3 v0;
    glm::vec3 v1;
    glm::vec3 v2;
};

struct Normals
{
    glm::vec3 n0;
    glm::vec3 n1;
    glm::vec3 n2;
};

struct TexCoords
{
    glm::vec2 uv0;
    glm::vec2 uv1;
    glm::vec2 uv2;
};

inline float surfaceArea(const Positions& t)
{
    return 0.5f * glm::length(glm::cross(t.v1 - t.v0, t.v2 - t.v0));
}
} // namespace nlrs
