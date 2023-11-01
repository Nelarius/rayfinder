#pragma once

#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>

namespace pt
{
// A wrapper around WGPUBuffer with unique ownership semantics.
class GpuBuffer
{
public:
    GpuBuffer() = default;

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    GpuBuffer(GpuBuffer&&);
    GpuBuffer& operator=(GpuBuffer&&);

    GpuBuffer(
        WGPUDevice           device,
        const char*          label,
        std::size_t          byteSize,
        WGPUBufferUsageFlags usage);
    ~GpuBuffer();

    // Raw access

    inline WGPUBuffer  handle() const { return mBuffer; }
    inline std::size_t byteSize() const { return mByteSize; }

    // Bind group and layout

    WGPUBindGroupLayoutEntry bindGroupLayoutEntry(
        std::uint32_t        binding,
        WGPUShaderStageFlags visibility) const;
    WGPUBindGroupEntry bindGroupEntry(std::uint32_t binding) const;

private:
    WGPUBuffer           mBuffer = nullptr;
    std::size_t          mByteSize = 0;
    WGPUBufferUsageFlags mUsage = WGPUBufferUsage_None;
};
} // namespace pt
