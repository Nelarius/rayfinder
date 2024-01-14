#pragma once

#include "ray.hpp"
#include "units/angle.hpp"

#include <glm/glm.hpp>

namespace nlrs
{
struct Camera
{
    glm::vec3 origin;
    glm::vec3 lowerLeftCorner;
    glm::vec3 horizontal;
    glm::vec3 vertical;
    glm::vec3 up;
    glm::vec3 right;
    float     lensRadius;

    bool operator==(const Camera& rhs) const noexcept = default;
};

// `aspectRatio` is defined as (width / height).
Camera createCamera(
    glm::vec3 origin,
    glm::vec3 lookAt,
    float     aperture,
    float     focusDistance,
    Angle     vfov,
    float     aspectRatio);

// (u, v) are in [0, 1] range, where (0, 0) is the lower left corner and (1, 1) is the upper right
// corner.
Ray generateCameraRay(const Camera& camera, float u, float v);
} // namespace nlrs
