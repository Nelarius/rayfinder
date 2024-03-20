#pragma once

#include <common/assert.hpp>

#include <webgpu/webgpu.h>

#include <span>

namespace nlrs
{
class GpuBindGroupLayout
{
public:
    GpuBindGroupLayout() = default;
    ~GpuBindGroupLayout();

    GpuBindGroupLayout(const GpuBindGroupLayout&) = delete;
    GpuBindGroupLayout& operator=(const GpuBindGroupLayout&) = delete;

    GpuBindGroupLayout(GpuBindGroupLayout&&) noexcept;
    GpuBindGroupLayout& operator=(GpuBindGroupLayout&&) noexcept;

    GpuBindGroupLayout(
        WGPUDevice                                device,
        const char*                               label,
        std::span<const WGPUBindGroupLayoutEntry> entries);

    GpuBindGroupLayout(WGPUDevice device, const char* label, const WGPUBindGroupLayoutEntry& entry);

    // Raw access

    inline WGPUBindGroupLayout ptr() const noexcept
    {
        NLRS_ASSERT(mBindGroupLayout != nullptr);
        return mBindGroupLayout;
    }

private:
    WGPUBindGroupLayout mBindGroupLayout = nullptr;
};
} // namespace nlrs
