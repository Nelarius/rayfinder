#pragma once

#include <common/extent.hpp>

#include <webgpu/webgpu.h>

struct GLFWwindow;

namespace nlrs
{
struct GpuContext
{
    WGPUInstance instance;
    WGPUDevice   device;
    WGPUQueue    queue;

    GpuContext(const WGPURequiredLimits&);
    ~GpuContext();
};
} // namespace nlrs
