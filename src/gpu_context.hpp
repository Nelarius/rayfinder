#pragma once

#include "common/framebuffer_size.hpp"

#include <webgpu/webgpu.h>

struct GLFWwindow;

namespace pt
{
struct WindowDescriptor
{
    WGPUSurfaceDescriptor surfaceDescriptor;
    FramebufferSize       framebufferSize;
};

struct GpuContext
{
    WGPUInstance  instance;
    WGPUSurface   surface;
    WGPUAdapter   adapter;
    WGPUDevice    device;
    WGPUQueue     queue;
    WGPUSwapChain swapChain;

    GpuContext(WindowDescriptor, const WGPURequiredLimits&);
    ~GpuContext();

    void resizeFramebuffer(const FramebufferSize&);
};
} // namespace pt
