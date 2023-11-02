#include "gpu_buffer.hpp"

namespace pt
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

GpuBuffer::GpuBuffer(GpuBuffer&& other)
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

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& other)
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
        throw std::runtime_error(std::format("Failed to create buffer: {}", label));
    }
}

GpuBuffer::~GpuBuffer()
{
    bufferSafeRelease(mBuffer);
    mBuffer = nullptr;
}

WGPUBindGroupLayoutEntry GpuBuffer::bindGroupLayoutEntry(
    const std::uint32_t        binding,
    const WGPUShaderStageFlags visibility) const
{
    assert(mBuffer != nullptr);

    const WGPUBufferBindingType bindingType = bufferUsageToBufferBindingType(mUsage);

    return WGPUBindGroupLayoutEntry{
        .nextInChain = nullptr,
        .binding = binding,
        .visibility = visibility,
        .buffer =
            WGPUBufferBindingLayout{
                .nextInChain = nullptr,
                .type = bindingType,
                .hasDynamicOffset = false,
                .minBindingSize = mByteSize},
        .sampler =
            WGPUSamplerBindingLayout{
                .nextInChain = nullptr,
                .type = WGPUSamplerBindingType_Undefined,
            },
        .texture =
            WGPUTextureBindingLayout{
                .nextInChain = nullptr,
                .sampleType = WGPUTextureSampleType_Undefined,
                .viewDimension = WGPUTextureViewDimension_Undefined,
                .multisampled = false,
            },
        .storageTexture =
            WGPUStorageTextureBindingLayout{
                .nextInChain = nullptr,
                .access = WGPUStorageTextureAccess_Undefined,
                .format = WGPUTextureFormat_Undefined,
                .viewDimension = WGPUTextureViewDimension_Undefined,
            },
    };
}

WGPUBindGroupEntry GpuBuffer::bindGroupEntry(const std::uint32_t binding) const
{
    assert(mBuffer != nullptr);
    return WGPUBindGroupEntry{
        .nextInChain = nullptr,
        .binding = binding,
        .buffer = mBuffer,
        .offset = 0,
        .size = mByteSize,
        .sampler = nullptr,
        .textureView = nullptr,
    };
}
} // namespace pt
