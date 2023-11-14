#pragma once

#include <common/extent.hpp>

#include <webgpu/webgpu.h>

struct GLFWwindow;

namespace nlrs
{
struct GpuContext
{
    WGPUSurface   surface;
    WGPUDevice    device;
    WGPUQueue     queue;
    WGPUSwapChain swapChain;

    GpuContext(GLFWwindow*, const WGPURequiredLimits&);
    ~GpuContext();

    void resizeFramebuffer(const Extent2i&);

    constexpr static WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
};
} // namespace nlrs
