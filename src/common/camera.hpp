#pragma once

#include "geometry.hpp"

#include <glm/glm.hpp>

namespace pt
{
struct Camera
{
    glm::vec3 origin;
    glm::vec3 lowerLeftCorner;
    glm::vec3 horizontal;
    glm::vec3 vertical;
};

Camera createCamera(
    const int       viewportWidth,
    const int       viewportHeight,
    const float     vfov,
    const glm::vec3 origin,
    const glm::vec3 lookAt);

// (u, v) are in [0, 1] range, where (0, 0) is the lower left corner and (1, 1) is the upper right
// corner.
Ray generateCameraRay(const Camera& camera, const float u, const float v);
} // namespace pt
