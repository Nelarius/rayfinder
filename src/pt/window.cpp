#include "window.hpp"

#include <GLFW/glfw3.h>

#include <cassert>
#include <format>
#include <stdexcept>

namespace pt
{
int Window::glfwRefCount = 0;

Window::Window(const WindowDescriptor& windowDesc)
    : mWindow(nullptr)
{
    if (glfwRefCount++ == 0)
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW.");
        }
        // NOTE: with this hint in place, GLFW assumes that we will manage the API and we can skip
        // calling glfwSwapBuffers.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    mWindow = glfwCreateWindow(
        windowDesc.windowSize.x,
        windowDesc.windowSize.y,
        windowDesc.title.data(),
        nullptr,
        nullptr);

    if (!mWindow)
    {
        throw std::runtime_error("Failed to create GLFW window.");
    }
}

Window::~Window()
{
    if (mWindow)
    {
        glfwDestroyWindow(mWindow);
        mWindow = nullptr;
    }

    if (--glfwRefCount == 0)
    {
        glfwTerminate();
    }
}

Extent2i Window::size() const
{
    Extent2i result;
    glfwGetWindowSize(mWindow, &result.x, &result.y);
    return result;
}

Extent2i Window::resolution() const
{
    Extent2i result;
    glfwGetFramebufferSize(mWindow, &result.x, &result.y);
    return result;
}

Extent2i Window::largestMonitorResolution() const
{
    int           monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    assert(monitorCount > 0);

    int      maxArea = 0;
    Extent2i maxResolution;

    for (int i = 0; i < monitorCount; ++i)
    {
        GLFWmonitor* const monitor = monitors[i];

        float xscale, yscale;
        glfwGetMonitorContentScale(monitor, &xscale, &yscale);

        const GLFWvidmode* const mode = glfwGetVideoMode(monitor);

        const int xpixels = static_cast<int>(xscale * mode->width + 0.5f);
        const int ypixels = static_cast<int>(yscale * mode->height + 0.5f);
        const int area = xpixels * ypixels;

        if (area > maxArea)
        {
            maxArea = area;
            maxResolution = Extent2i(xpixels, ypixels);
        }
    }

    return maxResolution;
}
} // namespace pt
