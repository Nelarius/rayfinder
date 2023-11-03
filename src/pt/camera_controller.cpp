#include "camera_controller.hpp"

#include <cmath>

namespace pt
{
CameraController::Orientation CameraController::cameraOrientation() const
{
    const glm::vec3 forward = glm::normalize(glm::vec3(
        std::cos(yaw.as_radians()) * std::cos(pitch.as_radians()),
        std::sin(pitch.as_radians()),
        std::sin(yaw.as_radians()) * std::cos(pitch.as_radians())));
    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::cross(forward, worldUp);
    const glm::vec3 up = glm::cross(right, forward);
    return Orientation{forward, right, up};
}

Camera CameraController::getCamera(const Extent2i& windowSize) const
{
    const Orientation orientation = cameraOrientation();
    return createCamera(
        position,
        position + focusDistance * orientation.forward,
        aperture,
        focusDistance,
        vfov,
        windowSize.x,
        windowSize.y);
}

void CameraController::update(const float dt)
{
    const Orientation orientation = cameraOrientation();
    const glm::vec3   translation = glm::vec3(
        (rightPressed - leftPressed) * speed * dt,
        (upPressed - downPressed) * speed * dt,
        (backwardPressed - forwardPressed) * speed * dt);
    position += orientation.right * translation.x + orientation.up * translation.y +
                orientation.forward * translation.z;
}
} // namespace pt
