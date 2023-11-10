#pragma once

#include <common/extent.hpp>

#include <string_view>

struct GLFWwindow;

namespace nlrs
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

    // Size accessors

    // Returns the size of the window in screen coordinates.
    Extent2i size() const;
    // Returns the size of the window in pixels.
    Extent2i resolution() const;
    // Returns the largest monitor, in pixels, by pixel count.
    Extent2i largestMonitorResolution() const;

    // Update loop

    void beginFrame();

    // Raw access

    GLFWwindow* ptr() const { return mWindow; }

private:
    GLFWwindow* mWindow;

    static int glfwRefCount;
};
} // namespace nlrs
