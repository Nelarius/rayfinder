#pragma once

#include <webgpu/webgpu.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <span>
#include <stdexcept>

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

    template<typename T>
    GpuBuffer(
        WGPUDevice           device,
        const char*          label,
        WGPUBufferUsageFlags usage,
        std::span<const T>   data);

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

template<typename T>
GpuBuffer::GpuBuffer(
    const WGPUDevice           device,
    const char* const          label,
    const WGPUBufferUsageFlags usage,
    const std::span<const T>   data)
    : mBuffer(nullptr),
      mByteSize(0),
      mUsage(usage)
{
    assert(device != nullptr);

    mByteSize = sizeof(T) * data.size();

    const WGPUBufferDescriptor bufferDesc{
        .nextInChain = nullptr,
        .label = label,
        .usage = mUsage,
        .size = mByteSize,
        .mappedAtCreation = true,
    };
    mBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

    if (!mBuffer)
    {
        throw std::runtime_error(std::format("Failed to create buffer: {}", label));
    }

    // It's legal to set mappedAtCreation = true and use the mapped range even if the usage doesn't
    // include MAP_READ or MAP_WRITE.
    // https://www.w3.org/TR/webgpu/#dom-gpubufferdescriptor-mappedatcreation

    void* const mappedData = wgpuBufferGetMappedRange(mBuffer, 0, mByteSize);
    std::memcpy(mappedData, data.data(), mByteSize);
    wgpuBufferUnmap(mBuffer);
}
} // namespace pt
