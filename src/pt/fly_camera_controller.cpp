#include "fly_camera_controller.hpp"

#include <GLFW/glfw3.h>

#include <cassert>
#include <cmath>

namespace nlrs
{
Camera FlyCameraController::getCamera() const
{
    const auto orientation = cameraOrientation();
    return createCamera(
        mPosition,
        mPosition + mFocusDistance * orientation.forward,
        mAperture,
        mFocusDistance,
        mVfov,
        aspectRatio(mWindowSize));
}

void FlyCameraController::lookAt(const glm::vec3& p)
{
    const glm::vec3 d = p - mPosition;
    const float     l = glm::length(d);
    const float     x = std::atan2(d.z, d.x);
    const float     y = std::asin(d.y / l);
    mYaw = Angle::radians(x);
    mPitch = Angle::radians(y);
}

void FlyCameraController::update(GLFWwindow* const window, const float dt)
{
    MousePos mousePos;
    glfwGetCursorPos(window, &mousePos.x, &mousePos.y);

    // Update input state

    {
        glfwGetWindowSize(window, &mWindowSize.x, &mWindowSize.y);
        mLeftPressed = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
        mRightPressed = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
        mForwardPressed = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
        mBackwardPressed = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
        mUpPressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
        mDownPressed = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;

        mMouseLookPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    }

    // Update orientation

    if (mMouseLookPressed)
    {
        if (mLastMousePos)
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
            assert(mLastMousePos.has_value());
            const glm::vec3 currentDir = toLocal * generateCameraRayDir(orientation, mousePos);
            const glm::vec3 previousDir =
                toLocal * generateCameraRayDir(orientation, *mLastMousePos);

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

            mYaw = mYaw + Angle::radians(a1 - a2);
            mPitch = Angle::radians(glm::clamp(
                mPitch.asRadians() + (p1 - p2), glm::radians(-89.0f), glm::radians(89.0f)));
        }
    }

    // Update translation

    {
        const glm::vec3 translation = glm::vec3(
            (mRightPressed - mLeftPressed) * mSpeed * dt,
            (mUpPressed - mDownPressed) * mSpeed * dt,
            (mForwardPressed - mBackwardPressed) * mSpeed * dt);
        const auto orientation = cameraOrientation();
        mPosition += orientation.right * translation.x + orientation.up * translation.y +
                     orientation.forward * translation.z;
    }

    mLastMousePos = mousePos;
}

FlyCameraController::Orientation FlyCameraController::cameraOrientation() const
{
    const glm::vec3 forward = glm::normalize(glm::vec3(
        std::cos(mYaw.asRadians()) * std::cos(mPitch.asRadians()),
        std::sin(mPitch.asRadians()),
        std::sin(mYaw.asRadians()) * std::cos(mPitch.asRadians())));
    const glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 right = glm::cross(forward, worldUp);
    const glm::vec3 up = glm::cross(right, forward);
    return Orientation{forward, right, up};
}

glm::vec3 FlyCameraController::generateCameraRayDir(
    const Orientation& orientation,
    const MousePos&    pos) const
{
    const float aspect = aspectRatio(mWindowSize);
    const float halfHeight = mFocusDistance * std::tan(0.5f * mVfov.asRadians());
    const float halfWidth = aspect * halfHeight;

    // UV coordinates in [0, 1] range with (0, 0) in the top-left corner.
    const float u = static_cast<float>(pos.x / mWindowSize.x);
    const float v = static_cast<float>(pos.y / mWindowSize.y);

    // Map coordinates to [-1, 1] range with (-1, -1) in the bottom-left corner.
    const float x = 2.0f * u - 1.0f;
    const float y = 1.0f - 2.0f * v;

    const glm::vec3 pointOnPlane = mPosition + mFocusDistance * orientation.forward +
                                   x * halfWidth * orientation.right +
                                   y * halfHeight * orientation.up;

    return glm::normalize(pointOnPlane - mPosition);
}
} // namespace nlrs
