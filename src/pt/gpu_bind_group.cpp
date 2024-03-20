#include "gpu_bind_group.hpp"
#include "webgpu_utils.hpp"

namespace nlrs
{
GpuBindGroup::GpuBindGroup(
    const WGPUDevice                          device,
    const char* const                         label,
    const WGPUBindGroupLayout                 layout,
    const std::span<const WGPUBindGroupEntry> entries)
    : mBindGroup(nullptr)
{
    const WGPUBindGroupDescriptor desc{
        .nextInChain = nullptr,
        .label = label,
        .layout = layout,
        .entryCount = entries.size(),
        .entries = entries.data(),
    };
    mBindGroup = wgpuDeviceCreateBindGroup(device, &desc);
    NLRS_ASSERT(mBindGroup != nullptr);
}

GpuBindGroup::GpuBindGroup(
    const WGPUDevice          device,
    const char* const         label,
    const WGPUBindGroupLayout layout,
    const WGPUBindGroupEntry& entry)
    : mBindGroup(nullptr)
{
    const WGPUBindGroupDescriptor desc{
        .nextInChain = nullptr,
        .label = label,
        .layout = layout,
        .entryCount = 1,
        .entries = &entry,
    };
    mBindGroup = wgpuDeviceCreateBindGroup(device, &desc);
    NLRS_ASSERT(mBindGroup != nullptr);
}

GpuBindGroup::GpuBindGroup(GpuBindGroup&& other) noexcept
    : mBindGroup(nullptr)
{
    if (this != &other)
    {
        mBindGroup = other.mBindGroup;
        other.mBindGroup = nullptr;
    }
}

GpuBindGroup& GpuBindGroup::operator=(GpuBindGroup&& other) noexcept
{
    if (this != &other)
    {
        bindGroupSafeRelease(mBindGroup);
        mBindGroup = other.mBindGroup;
        other.mBindGroup = nullptr;
    }
    return *this;
}

GpuBindGroup::~GpuBindGroup()
{
    bindGroupSafeRelease(mBindGroup);
    mBindGroup = nullptr;
}
} // namespace nlrs
