#include "camera.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace nlrs
{
Camera createCamera(
    const glm::vec3 origin,
    const glm::vec3 lookAt,
    const float     aperture,
    const float     focusDistance,
    const Angle     vfov,
    const float     aspectRatio)
{
    const float theta = vfov.asRadians();
    const float halfHeight = focusDistance * glm::tan(0.5f * theta);
    const float halfWidth = aspectRatio * halfHeight;

    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    const glm::vec3 forward = glm::normalize(lookAt - origin);
    const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    const glm::vec3 up = glm::cross(right, forward);

    const glm::vec3 lowerLeftCorner =
        origin - halfWidth * right - halfHeight * up + focusDistance * forward;

    const glm::vec3 horizontal = 2.0f * halfWidth * right;
    const glm::vec3 vertical = 2.0f * halfHeight * up;

    const float lensRadius = 0.5f * aperture;

    return Camera{
        .origin = origin,
        .lowerLeftCorner = lowerLeftCorner,
        .horizontal = horizontal,
        .vertical = vertical,
        .up = up,
        .right = right,
        .lensRadius = lensRadius,
    };
}

Ray generateCameraRay(const Camera& camera, const float u, const float v)
{
    const glm::vec3 origin = camera.origin;
    const glm::vec3 direction =
        camera.lowerLeftCorner + camera.horizontal * u + camera.vertical * v - origin;

    return Ray{.origin = origin, .direction = glm::normalize(direction)};
}
} // namespace nlrs
