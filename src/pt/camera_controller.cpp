#include "camera_controller.hpp"

#include <cassert>
#include <cmath>

namespace pt
{
Camera FlyCameraController::getCamera() const
{
    const auto orientation = cameraOrientation();
    return createCamera(
        position,
        position + focusDistance * orientation.forward,
        aperture,
        focusDistance,
        vfov,
        aspectRatio(windowSize));
}

void FlyCameraController::lookAt(const glm::vec3& p)
{
    const glm::vec3 d = p - position;
    const float     r = glm::length(d);
    const float     x = std::atan2(d.z, d.x);
    const float     y = std::asin(d.y / r);
    yaw = Angle::radians(x);
    pitch = Angle::radians(y);
}

void FlyCameraController::update(const float dt, const MousePos& mousePos)
{
    if (mouseLookPressed)
    {
        if (lastMousePos)
        {
            const auto      orientation = cameraOrientation();
            const glm::vec3 c1 = orientation.right;
            const glm::vec3 c2 = orientation.forward;
            const glm::vec3 c3 = glm::normalize(glm::cross(c1, c2));
            const glm::mat3 fromLocal(c1, c2, c3);
            const glm::mat3 toLocal = glm::inverse(fromLocal);

            // Perform cartesian to spherical coordinate conversion in camera-local space,
            // where z points straight into the screen. That way there is no need to worry
            // about which quadrant of the sphere we are in for the conversion.
            assert(lastMousePos.has_value());
            const glm::vec3 currentDir = toLocal * generateCameraRayDir(orientation, mousePos);
            const glm::vec3 previousDir =
                toLocal * generateCameraRayDir(orientation, *lastMousePos);

            const float x1 = currentDir.x;
            const float y1 = currentDir.y;
            const float z1 = currentDir.z;

            const float x2 = previousDir.x;
            const float y2 = previousDir.y;
            const float z2 = previousDir.z;

            const float p1 = std::acos(z1);
            const float p2 = std::acos(z2);

            const float a1 = std::copysign(1.0f, y1) * std::acos(x1 / std::sqrt(x1 * x1 + y1 * y1));
            const float a2 = std::copysign(1.0f, y2) * std::acos(x2 / std::sqrt(x2 * x2 + y2 * y2));

            yaw = yaw + Angle::radians(a1 - a2);
            pitch = Angle::radians(glm::clamp(
                pitch.as_radians() + (p1 - p2), glm::radians(-89.0f), glm::radians(89.0f)));
        }
    }

    {
        const glm::vec3 translation = glm::vec3(
            (rightPressed - leftPressed) * speed * dt,
            (upPressed - downPressed) * speed * dt,
            (forwardPressed - backwardPressed) * speed * dt);
        const auto orientation = cameraOrientation();
        position += orientation.right * translation.x + orientation.up * translation.y +
                    orientation.forward * translation.z;
    }

    lastMousePos = mousePos;
}

FlyCameraController::Orientation FlyCameraController::cameraOrientation() const
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

glm::vec3 FlyCameraController::generateCameraRayDir(
    const Orientation& orientation,
    const MousePos&    pos) const
{
    const float aspect = aspectRatio(windowSize);
    const float halfHeight = std::tan(0.5f * vfov.as_radians());
    const float halfWidth = aspect * halfHeight;

    // UV coordinates in [0, 1] range with (0, 0) in the top-left corner.
    const float u = static_cast<float>(pos.x / windowSize.x);
    const float v = static_cast<float>(pos.y / windowSize.y);

    // Map coordinates to [-1, 1] range with (-1, -1) in the bottom-left corner.
    const float x = 2.0f * u - 1.0f;
    const float y = 1.0f - 2.0f * v;

    const glm::vec3 pointOnPlane = position + focusDistance * orientation.forward +
                                   x * halfWidth * orientation.right +
                                   y * halfHeight * orientation.up;

    return glm::normalize(pointOnPlane - position);
}
} // namespace pt
