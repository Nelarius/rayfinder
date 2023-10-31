#pragma once

#include "common/extent.hpp"

#include <string_view>

struct GLFWwindow;

namespace pt
{
struct WindowDescriptor
{
    Extent2i         windowSize;
    std::string_view title;
};

class Window
{
public:
    explicit Window(const WindowDescriptor&);
    ~Window();

    Extent2i    framebufferSize() const;
    GLFWwindow* ptr() const { return mWindow; }

private:
    GLFWwindow* mWindow;

    static int glfwRefCount;
};
} // namespace pt
