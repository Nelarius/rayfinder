#include "gpu_bind_group_layout.hpp"
#include "webgpu_utils.hpp"

namespace nlrs
{
GpuBindGroupLayout::GpuBindGroupLayout(
    const WGPUDevice                                device,
    const char* const                               label,
    const std::span<const WGPUBindGroupLayoutEntry> entries)
    : mBindGroupLayout(nullptr)
{
    const WGPUBindGroupLayoutDescriptor desc{
        .nextInChain = nullptr,
        .label = label,
        .entryCount = entries.size(),
        .entries = entries.data(),
    };
    mBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &desc);
    NLRS_ASSERT(mBindGroupLayout != nullptr);
}

GpuBindGroupLayout::GpuBindGroupLayout(
    const WGPUDevice                device,
    const char* const               label,
    const WGPUBindGroupLayoutEntry& entry)
    : mBindGroupLayout(nullptr)
{
    const WGPUBindGroupLayoutDescriptor desc{
        .nextInChain = nullptr,
        .label = label,
        .entryCount = 1,
        .entries = &entry,
    };
    mBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &desc);
    NLRS_ASSERT(mBindGroupLayout != nullptr);
}

GpuBindGroupLayout::GpuBindGroupLayout(GpuBindGroupLayout&& other) noexcept
    : mBindGroupLayout(nullptr)
{
    if (this != &other)
    {
        mBindGroupLayout = other.mBindGroupLayout;
        other.mBindGroupLayout = nullptr;
    }
}

GpuBindGroupLayout& GpuBindGroupLayout::operator=(GpuBindGroupLayout&& other) noexcept
{
    if (this != &other)
    {
        bindGroupLayoutSafeRelease(mBindGroupLayout);
        mBindGroupLayout = other.mBindGroupLayout;
        other.mBindGroupLayout = nullptr;
    }
    return *this;
}

GpuBindGroupLayout::~GpuBindGroupLayout()
{
    bindGroupLayoutSafeRelease(mBindGroupLayout);
    mBindGroupLayout = nullptr;
}
} // namespace nlrs
