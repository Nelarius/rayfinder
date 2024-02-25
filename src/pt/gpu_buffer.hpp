#pragma once

#include "webgpu_utils.hpp"

#include <fmt/core.h>
#include <webgpu/webgpu.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace nlrs
{
// A wrapper around WGPUBuffer with unique ownership semantics.
class GpuBuffer
{
public:
    GpuBuffer() = default;

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    GpuBuffer(GpuBuffer&&) noexcept;
    GpuBuffer& operator=(GpuBuffer&&) noexcept;

    GpuBuffer(
        WGPUDevice           device,
        const char*          label,
        WGPUBufferUsageFlags usage,
        std::size_t          byteSize);

    template<typename T>
    GpuBuffer(
        WGPUDevice           device,
        const char*          label,
        WGPUBufferUsageFlags usage,
        std::span<const T>   data);

    ~GpuBuffer();

    // Raw access

    inline WGPUBuffer handle() const noexcept
    {
        assert(mBuffer != nullptr);
        return mBuffer;
    }
    inline std::size_t byteSize() const noexcept
    {
        assert(mByteSize > 0);
        return mByteSize;
    }

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
      mByteSize(sizeof(T) * data.size()),
      mUsage(usage)
{
    assert(device != nullptr);

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
        throw std::runtime_error(fmt::format("Failed to create buffer: {}.", label));
    }

    // It's legal to set mappedAtCreation = true and use the mapped range even if the usage doesn't
    // include MAP_READ or MAP_WRITE.
    // https://www.w3.org/TR/webgpu/#dom-gpubufferdescriptor-mappedatcreation

    void* const mappedData = wgpuBufferGetMappedRange(mBuffer, 0, mByteSize);
    assert(mappedData);
    std::memcpy(mappedData, data.data(), mByteSize);
    wgpuBufferUnmap(mBuffer);
}
} // namespace nlrs
