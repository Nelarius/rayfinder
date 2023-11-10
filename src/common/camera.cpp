#include "camera.hpp"

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
    const float theta = vfov.as_radians();
    const float halfHeight = focusDistance * glm::tan(0.5f * theta);
    const float halfWidth = aspectRatio * halfHeight;

    const glm::vec3 w = glm::normalize(lookAt - origin);
    const glm::vec3 v = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 u = glm::cross(w, v);

    const glm::vec3 lowerLeftCorner = origin - halfWidth * u - halfHeight * v + focusDistance * w;

    return Camera{
        .origin = origin,
        .lowerLeftCorner = lowerLeftCorner,
        .horizontal = 2.0f * halfWidth * u,
        .vertical = 2.0f * halfHeight * v,
        .lensRadius = 0.5f * aperture,
    };
}

Ray generateCameraRay(const Camera& camera, const float u, const float v)
{
    // TODO: this does not implement lens offsets using the lens radius. It is not needed for tests
    // at the moment. CPU-raytracing would need it for depth of field effects. Here is a quick
    // sketch of what it could look like (WGSL code):
    // ````
    // let randomPointInLens = camera.lensRadius * rngNextVec3InUnitDisk(rngState);
    // let lensOffset = randomPointInLens.x * camera.u + randomPointInLens.y * camera.v;
    // let origin = camera.origin + lensOffset;
    // ```
    const glm::vec3 origin = camera.origin;
    const glm::vec3 direction =
        camera.lowerLeftCorner + camera.horizontal * u + camera.vertical * v - origin;

    return Ray{.origin = origin, .direction = glm::normalize(direction)};
}
} // namespace nlrs
