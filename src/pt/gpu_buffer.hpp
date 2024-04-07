#pragma once

#include "webgpu_utils.hpp"

#include <common/assert.hpp>

#include <fmt/core.h>
#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>

namespace nlrs
{
enum class GpuBufferUsage : uint32_t
{
    None = 0,
    CopySrc = 1 << 0,
    CopyDst = 1 << 1,
    MapRead = 1 << 2,
    MapWrite = 1 << 3,
    Index = 1 << 4,
    Vertex = 1 << 5,
    Uniform = 1 << 6,
    Storage = 1 << 7,
    ReadOnlyStorage = 1 << 8,
    Indirect = 1 << 9,
    QueryResolve = 1 << 10,
};

// A wrapper around WGPUBuffer with unique ownership semantics.
class GpuBuffer
{
public:
    GpuBuffer() = default;

    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    GpuBuffer(GpuBuffer&&) noexcept;
    GpuBuffer& operator=(GpuBuffer&&) noexcept;

    GpuBuffer(WGPUDevice device, const char* label, GpuBufferUsage usage, std::size_t byteSize);

    template<typename T>
    GpuBuffer(WGPUDevice device, const char* label, GpuBufferUsage usage, std::span<const T> data);

    ~GpuBuffer();

    // Raw access

    inline WGPUBuffer ptr() const noexcept
    {
        NLRS_ASSERT(mBuffer != nullptr);
        return mBuffer;
    }
    inline std::size_t byteSize() const noexcept
    {
        NLRS_ASSERT(mByteSize > 0);
        return mByteSize;
    }

    // Bind group and layout

    WGPUBindGroupLayoutEntry bindGroupLayoutEntry(
        std::uint32_t        bindingIndex,
        WGPUShaderStageFlags visibility,
        std::size_t          minBindingSize = 0) const;
    WGPUBindGroupEntry bindGroupEntry(std::uint32_t bindingIndex) const;

private:
    WGPUBuffer     mBuffer = nullptr;
    std::size_t    mByteSize = 0;
    GpuBufferUsage mUsage = GpuBufferUsage::None;
};

inline GpuBufferUsage operator|(GpuBufferUsage lhs, GpuBufferUsage rhs) noexcept
{
    return static_cast<GpuBufferUsage>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

inline GpuBufferUsage operator|=(GpuBufferUsage& lhs, GpuBufferUsage rhs) noexcept
{
    return lhs = lhs | rhs;
}

inline GpuBufferUsage operator&(GpuBufferUsage lhs, GpuBufferUsage rhs) noexcept
{
    return static_cast<GpuBufferUsage>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

inline WGPUBufferUsageFlags gpuBufferUsageToWGPUBufferUsage(const GpuBufferUsage usage) noexcept
{
    WGPUBufferUsageFlags wgpuUsage = WGPUBufferUsage_None;

    if ((usage & GpuBufferUsage::CopySrc) == GpuBufferUsage::CopySrc)
    {
        wgpuUsage |= WGPUBufferUsage_CopySrc;
    }
    if ((usage & GpuBufferUsage::CopyDst) == GpuBufferUsage::CopyDst)
    {
        wgpuUsage |= WGPUBufferUsage_CopyDst;
    }
    if ((usage & GpuBufferUsage::MapRead) == GpuBufferUsage::MapRead)
    {
        wgpuUsage |= WGPUBufferUsage_MapRead;
    }
    if ((usage & GpuBufferUsage::MapWrite) == GpuBufferUsage::MapWrite)
    {
        wgpuUsage |= WGPUBufferUsage_MapWrite;
    }
    if ((usage & GpuBufferUsage::Index) == GpuBufferUsage::Index)
    {
        wgpuUsage |= WGPUBufferUsage_Index;
    }
    if ((usage & GpuBufferUsage::Vertex) == GpuBufferUsage::Vertex)
    {
        wgpuUsage |= WGPUBufferUsage_Vertex;
    }
    if ((usage & GpuBufferUsage::Uniform) == GpuBufferUsage::Uniform)
    {
        wgpuUsage |= WGPUBufferUsage_Uniform;
    }
    if ((usage & GpuBufferUsage::Storage) == GpuBufferUsage::Storage)
    {
        wgpuUsage |= WGPUBufferUsage_Storage;
    }
    if ((usage & GpuBufferUsage::ReadOnlyStorage) == GpuBufferUsage::ReadOnlyStorage)
    {
        wgpuUsage |= WGPUBufferUsage_Storage;
    }
    if ((usage & GpuBufferUsage::Indirect) == GpuBufferUsage::Indirect)
    {
        wgpuUsage |= WGPUBufferUsage_Indirect;
    }
    if ((usage & GpuBufferUsage::QueryResolve) == GpuBufferUsage::QueryResolve)
    {
        wgpuUsage |= WGPUBufferUsage_QueryResolve;
    }

    return wgpuUsage;
}

template<typename T>
GpuBuffer::GpuBuffer(
    const WGPUDevice         device,
    const char* const        label,
    const GpuBufferUsage     usage,
    const std::span<const T> data)
    : mBuffer(nullptr),
      mByteSize(sizeof(T) * data.size()),
      mUsage(usage)
{
    NLRS_ASSERT(device != nullptr);
    NLRS_ASSERT((mUsage & GpuBufferUsage::Uniform) == GpuBufferUsage::None || mByteSize % 16 == 0);

    const WGPUBufferDescriptor bufferDesc{
        .nextInChain = nullptr,
        .label = label,
        .usage = gpuBufferUsageToWGPUBufferUsage(usage),
        .size = mByteSize,
        .mappedAtCreation = true,
    };
    mBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

    if (!mBuffer)
    {
        throw std::runtime_error(fmt::format("Failed to create buffer \"{}\".", label));
    }

    // It's legal to set mappedAtCreation = true and use the mapped range even if the usage doesn't
    // include MAP_READ or MAP_WRITE.
    // https://www.w3.org/TR/webgpu/#dom-gpubufferdescriptor-mappedatcreation

    void* const mappedData = wgpuBufferGetMappedRange(mBuffer, 0, mByteSize);
    if (!mappedData)
    {
        throw std::runtime_error(
            fmt::format("Failed to map buffer \"{}\", bytesize: {}.", label, mByteSize));
    }
    std::memcpy(mappedData, data.data(), mByteSize);
    wgpuBufferUnmap(mBuffer);
}
} // namespace nlrs
