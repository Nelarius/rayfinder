#pragma once

#include <common/assert.hpp>

#include <webgpu/webgpu.h>

#include <span>

namespace nlrs
{
class GpuBindGroup
{
public:
    GpuBindGroup() = default;
    ~GpuBindGroup();

    GpuBindGroup(const GpuBindGroup&) = delete;
    GpuBindGroup& operator=(const GpuBindGroup&) = delete;

    GpuBindGroup(GpuBindGroup&&) noexcept;
    GpuBindGroup& operator=(GpuBindGroup&&) noexcept;

    GpuBindGroup(
        WGPUDevice                          device,
        const char*                         label,
        WGPUBindGroupLayout                 layout,
        std::span<const WGPUBindGroupEntry> entries);

    GpuBindGroup(
        WGPUDevice                device,
        const char*               label,
        WGPUBindGroupLayout       layout,
        const WGPUBindGroupEntry& entry);

    // Raw access

    inline WGPUBindGroup ptr() const noexcept
    {
        NLRS_ASSERT(mBindGroup != nullptr);
        return mBindGroup;
    }

private:
    WGPUBindGroup mBindGroup = nullptr;
};
} // namespace nlrs
