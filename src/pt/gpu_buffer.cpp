#include "gpu_buffer.hpp"

namespace nlrs
{
namespace
{
void bufferSafeRelease(WGPUBuffer buffer)
{
    if (buffer)
    {
        wgpuBufferDestroy(buffer);
        wgpuBufferRelease(buffer);
    }
}

WGPUBufferBindingType bufferUsageToBufferBindingType(const WGPUBufferUsageFlags usage)
{
    assert(usage != WGPUBufferUsage_None);

    // BufferUsage flags contains redundant binding type information that we can reuse, so that the
    // user doesn't have to provide the additional flag.

    if (usage & WGPUBufferUsage_Uniform)
    {
        return WGPUBufferBindingType_Uniform;
    }
    else if (usage & WGPUBufferUsage_Storage)
    {
        return WGPUBufferBindingType_Storage;
    }

    assert(!"No matching WGPUBufferBindingType for WGPUBufferUsage.");

    return WGPUBufferBindingType_Undefined;
}
} // namespace

GpuBuffer::GpuBuffer(GpuBuffer&& other) noexcept
{
    if (this != &other)
    {
        mBuffer = other.mBuffer;
        mByteSize = other.mByteSize;
        mUsage = other.mUsage;

        other.mBuffer = nullptr;
        other.mByteSize = 0;
        other.mUsage = WGPUBufferUsage_None;
    }
}

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other) noexcept
{
    if (this != &other)
    {
        bufferSafeRelease(mBuffer);

        mBuffer = other.mBuffer;
        mByteSize = other.mByteSize;
        mUsage = other.mUsage;

        other.mBuffer = nullptr;
        other.mByteSize = 0;
        other.mUsage = WGPUBufferUsage_None;
    }
    return *this;
}

GpuBuffer::GpuBuffer(
    const WGPUDevice           device,
    const char* const          label,
    const WGPUBufferUsageFlags usage,
    const std::size_t          byteSize)
    : mBuffer(nullptr),
      mByteSize(byteSize),
      mUsage(usage)
{
    assert(device != nullptr);

    const WGPUBufferDescriptor bufferDesc{
        .nextInChain = nullptr,
        .label = label,
        .usage = mUsage,
        .size = mByteSize,
        .mappedAtCreation = false,
    };
    mBuffer = wgpuDeviceCreateBuffer(device, &bufferDesc);

    if (!mBuffer)
    {
        throw std::runtime_error(fmt::format("Failed to create buffer: {}.", label));
    }
}

GpuBuffer::~GpuBuffer()
{
    bufferSafeRelease(mBuffer);
    mBuffer = nullptr;
}

WGPUBindGroupLayoutEntry GpuBuffer::bindGroupLayoutEntry(
    const std::uint32_t        bindingIdx,
    const WGPUShaderStageFlags visibility) const
{
    assert(mBuffer != nullptr);
    const WGPUBufferBindingType bindingType = bufferUsageToBufferBindingType(mUsage);
    return bufferBindGroupLayoutEntry(bindingIdx, visibility, bindingType, mByteSize);
}

WGPUBindGroupEntry GpuBuffer::bindGroupEntry(const std::uint32_t bindingIdx) const
{
    assert(mBuffer != nullptr);
    return bufferBindGroupEntry(bindingIdx, mBuffer, mByteSize);
}
} // namespace nlrs
