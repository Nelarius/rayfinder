#pragma once

#include <common/camera.hpp>
#include <common/extent.hpp>
#include <common/units/angle.hpp>

#include <glm/glm.hpp>

namespace pt
{
struct CameraController
{
public:
    CameraController() = default;

    Camera getCamera(const Extent2i& windowSize) const;

    void update(float dt);

    float speed = 0.8f;
    bool  leftPressed = false;
    bool  rightPressed = false;
    bool  forwardPressed = false;
    bool  backwardPressed = false;
    bool  upPressed = false;
    bool  downPressed = false;

private:
    glm::vec3 position = glm::vec3(-19.0f, 2.0f, -8.0f);
    Angle     yaw = Angle::degrees(25.0f);
    Angle     pitch = Angle::degrees(-5.0f);
    Angle     vfov = Angle::degrees(80.0f);
    float     aperture = 0.8f;
    float     focusDistance = 10.0f;

    struct Orientation
    {
        glm::vec3 forward;
        glm::vec3 right;
        glm::vec3 up;
    };

    Orientation cameraOrientation() const;
};
} // namespace pt
