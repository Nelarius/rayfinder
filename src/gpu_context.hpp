#pragma once

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

    void resizeFramebuffer(const FramebufferSize&);
};
} // namespace pt
