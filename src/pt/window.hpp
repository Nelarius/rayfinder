#pragma once

#include <common/extent.hpp>

#include <webgpu/webgpu.h>

#include <functional>
#include <string_view>

struct GLFWwindow;

namespace nlrs
{
struct GpuContext;

struct WindowDescriptor
{
    Extent2i         windowSize;
    std::string_view title;
};

using ScreenSize = Extent2i;
using FramebufferSize = Extent2i;

using NewFrameCallback = std::function<void()>;
using UpdateCallback = std::function<void(GLFWwindow*, float)>;
using RenderCallback = std::function<void(GLFWwindow*, WGPUSwapChain)>;
using ResizeCallback = std::function<void(FramebufferSize)>;

class Window
{
public:
    explicit Window(const WindowDescriptor&, const GpuContext&);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&&);
    Window& operator=(Window&&);

    // Size accessors

    // Returns the size of the window in screen coordinates.
    ScreenSize size() const;
    // Returns the size of the window in pixels.
    FramebufferSize resolution() const;
    // Returns the largest monitor, in pixels, by pixel count.

    // Run loop

    void run(
        const GpuContext&,
        NewFrameCallback&&,
        UpdateCallback&&,
        RenderCallback&&,
        ResizeCallback&&);

    // Raw access

    GLFWwindow* ptr() const { return mWindow; }

    constexpr static WGPUTextureFormat SWAP_CHAIN_FORMAT = WGPUTextureFormat_BGRA8Unorm;

private:
    GLFWwindow*   mWindow;
    WGPUSurface   mSurface;
    WGPUSwapChain mSwapChain;

    static int glfwRefCount;
};
} // namespace nlrs
