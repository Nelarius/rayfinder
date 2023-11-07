#pragma once

#include <common/camera.hpp>
#include <common/extent.hpp>
#include <common/units/angle.hpp>

#include <glm/glm.hpp>

#include <optional>

namespace pt
{
struct MousePos
{
    // Mouse position coordinates are given in screen coordinates not pixel coordinates.
    double x = 0.0;
    double y = 0.0;
};

struct FlyCameraController
{
public:
    FlyCameraController() = default;

    Camera getCamera() const;

    void update(float dt, const MousePos&);

    float speed = 0.8f;
    bool  leftPressed = false;
    bool  rightPressed = false;
    bool  forwardPressed = false;
    bool  backwardPressed = false;
    bool  upPressed = false;
    bool  downPressed = false;
    bool  mouseLookPressed = false;
    // TODO: discriminate between window size and framebuffer size to avoid accidents with mouse
    // cursor uv-coordinates
    Extent2i                windowSize = Extent2i(0, 0);
    std::optional<MousePos> lastMousePos = std::nullopt;

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
    glm::vec3   generateCameraRayDir(const Orientation&, const MousePos&) const;
};
} // namespace pt
