#include "window.hpp"

#include <GLFW/glfw3.h>

#include <format>
#include <stdexcept>

namespace pt
{
int Window::glfwRefCount = 0;

Window::Window(const WindowDescriptor& windowDesc) : mWindow(nullptr)
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

Extent2i Window::framebufferSize() const
{
    Extent2i result;
    glfwGetFramebufferSize(mWindow, &result.x, &result.y);
    return result;
}
} // namespace pt
