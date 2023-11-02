#include "camera.hpp"

namespace pt
{
Camera createCamera(
    const int       viewportWidth,
    const int       viewportHeight,
    const float     vfov,
    const glm::vec3 origin,
    const glm::vec3 lookAt)
{
    // TODO: pass focusDistance in as a parameter.
    const float focusDistance = 1.0f;
    const float aspectRatio =
        static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    const float theta = glm::radians(vfov);
    const float halfHeight = glm::tan(0.5f * theta);
    const float halfWidth = aspectRatio * halfHeight;

    const glm::vec3 w = glm::normalize(lookAt - origin);
    const glm::vec3 v = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 u = glm::cross(w, v);

    const glm::vec3 lowerLeftCorner = origin - halfWidth * u - halfHeight * v + focusDistance * w;

    return Camera{
        .origin = origin,
        .lowerLeftCorner = lowerLeftCorner,
        .horizontal = 2.0f * halfWidth * u,
        .vertical = 2.0f * halfHeight * v};
}

Ray generateCameraRay(const Camera& camera, const float u, const float v)
{
    const glm::vec3 origin = camera.origin;
    const glm::vec3 direction =
        camera.lowerLeftCorner + camera.horizontal * u + camera.vertical * v - origin;

    return Ray{.origin = origin, .direction = glm::normalize(direction)};
}
} // namespace pt
