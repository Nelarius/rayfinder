#pragma once

#include <glm/glm.hpp>

namespace nlrs
{
struct PositionAttribute
{
    glm::vec3 p0;   // offset: 0, size: 12
    float     pad0; // offset: 12, size: 4
    glm::vec3 p1;   // offset: 16, size: 12
    float     pad1; // offset: 28, size: 4
    glm::vec3 p2;   // offset: 32, size: 12
    float     pad2; // offset: 44, size: 4
};

struct VertexAttributes
{
    // Normals
    glm::vec3 n0;   // offset 0, size: 12
    float     pad0; // offset 12, size: 4
    glm::vec3 n1;   // offset 16, size: 12
    float     pad1; // offset 28, size: 4
    glm::vec3 n2;   // offset 32, size: 12
    float     pad2; // offset 44, size: 4

    // Texture coordinates
    glm::vec2 uv0; // offset 48, size: 8
    glm::vec2 uv1; // offset 56, size: 8
    glm::vec2 uv2; // offset 64, size: 8

    // Texture index
    std::uint32_t textureIdx; // offset 72, size: 4
    std::uint32_t pad3;       // offset: 76, size 4
};
} // namespace nlrs