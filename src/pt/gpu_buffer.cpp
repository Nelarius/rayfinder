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

inline WGPUBufferBindingType gpuBufferUsageToWGPUBufferBindingType(const GpuBufferUsages usages)
{
    if (usages.has(GpuBufferUsage::Storage))
    {
        return WGPUBufferBindingType_Storage;
    }
    else if (usages.has(GpuBufferUsage::ReadOnlyStorage))
    {
        return WGPUBufferBindingType_ReadOnlyStorage;
    }
    else if (usages.has(GpuBufferUsage::Uniform))
    {
        return WGPUBufferBindingType_Uniform;
    }

    NLRS_ASSERT("Invalid buffer usage for binding type!");

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
        other.mUsage = GpuBufferUsage::None;
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
        other.mUsage = GpuBufferUsage::None;
    }
    return *this;
}

GpuBuffer::GpuBuffer(
    const WGPUDevice      device,
    const char* const     label,
    const GpuBufferUsages usages,
    const std::size_t     byteSize)
    : mBuffer(nullptr),
      mByteSize(byteSize),
      mUsage(usages)
{
    NLRS_ASSERT(device != nullptr);
    NLRS_ASSERT(mByteSize % 16 == 0 || !mUsage.has(GpuBufferUsage::Uniform));

    const WGPUBufferDescriptor bufferDesc{
        .nextInChain = nullptr,
        .label = label,
        .usage = gpuBufferUsageToWGPUBufferUsage(usages),
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
    const WGPUShaderStageFlags visibility,
    const std::size_t          minBindingSize) const
{
    NLRS_ASSERT(mBuffer != nullptr);
    const WGPUBufferBindingType bindingType = gpuBufferUsageToWGPUBufferBindingType(mUsage);
    return bufferBindGroupLayoutEntry(bindingIdx, visibility, bindingType, minBindingSize);
}

WGPUBindGroupEntry GpuBuffer::bindGroupEntry(const std::uint32_t bindingIdx) const
{
    NLRS_ASSERT(mBuffer != nullptr);
    return bufferBindGroupEntry(bindingIdx, mBuffer, mByteSize);
}
} // namespace nlrs
