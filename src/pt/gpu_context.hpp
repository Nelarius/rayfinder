#pragma once

#include <common/extent.hpp>

#include <webgpu/webgpu.h>

struct GLFWwindow;

namespace pt
{
struct GpuContext
{
    WGPUInstance  instance;
    WGPUSurface   surface;
    WGPUAdapter   adapter;
    WGPUDevice    device;
    WGPUQueue     queue;
    WGPUSwapChain swapChain;

    GpuContext(GLFWwindow*, const WGPURequiredLimits&);
    ~GpuContext();

    void resizeFramebuffer(const Extent2i&);

    constexpr static WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
};
} // namespace pt
