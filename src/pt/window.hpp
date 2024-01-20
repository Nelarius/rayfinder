#pragma once

#include <common/extent.hpp>

#include <webgpu/webgpu.h>

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
    Extent2i size() const;
    // Returns the size of the window in pixels.
    Extent2i resolution() const;
    // Returns the largest monitor, in pixels, by pixel count.
    Extent2i largestMonitorResolution() const;

    // Update

    void resizeFramebuffer(const Extent2i&, const GpuContext&);
    void present();

    // Raw access

    GLFWwindow* ptr() const { return mWindow; }

    WGPUSwapChain swapChain() const { return mSwapChain; }

    constexpr static WGPUTextureFormat SWAP_CHAIN_FORMAT = WGPUTextureFormat_BGRA8Unorm;

private:
    GLFWwindow*   mWindow;
    WGPUSurface   mSurface;
    WGPUSwapChain mSwapChain;

    static int glfwRefCount;
};
} // namespace nlrs
