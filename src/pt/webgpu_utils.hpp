#pragma once

#include <fmt/core.h>
#include <webgpu/webgpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace nlrs
{
inline void bindGroupSafeRelease(const WGPUBindGroup bindGroup) noexcept
{
    if (bindGroup)
    {
        wgpuBindGroupRelease(bindGroup);
    }
}

inline void bindGroupLayoutSafeRelease(const WGPUBindGroupLayout bindGroupLayout) noexcept
{
    if (bindGroupLayout)
    {
        wgpuBindGroupLayoutRelease(bindGroupLayout);
    }
}

inline void querySetSafeRelease(const WGPUQuerySet querySet) noexcept
{
    if (querySet)
    {
        wgpuQuerySetDestroy(querySet);
        wgpuQuerySetRelease(querySet);
    }
}

inline void renderPipelineSafeRelease(const WGPURenderPipeline pipeline) noexcept
{
    if (pipeline)
    {
        wgpuRenderPipelineRelease(pipeline);
    }
}

inline void samplerSafeRelease(const WGPUSampler sampler) noexcept
{
    if (sampler)
    {
        wgpuSamplerRelease(sampler);
    }
}

inline void textureSafeRelease(const WGPUTexture texture) noexcept
{
    if (texture)
    {
        wgpuTextureDestroy(texture);
        wgpuTextureRelease(texture);
    }
}

inline void textureViewSafeRelease(const WGPUTextureView textureView) noexcept
{
    if (textureView)
    {
        wgpuTextureViewRelease(textureView);
    }
}

constexpr WGPUStencilFaceState DEFAULT_STENCIL_FACE_STATE{
    .compare = WGPUCompareFunction_Always,
    .failOp = WGPUStencilOperation_Keep,
    .depthFailOp = WGPUStencilOperation_Keep,
    .passOp = WGPUStencilOperation_Keep,
};

constexpr WGPUBufferBindingLayout DEFAULT_BUFFER_BINDING_LAYOUT{
    .nextInChain = nullptr,
    .type = WGPUBufferBindingType_Undefined,
    .hasDynamicOffset = false,
    .minBindingSize = 0,
};

constexpr WGPUSamplerBindingLayout DEFAULT_SAMPLER_BINDING_LAYOUT{
    .nextInChain = nullptr,
    .type = WGPUSamplerBindingType_Undefined,
};

constexpr WGPUTextureBindingLayout DEFAULT_TEXTURE_BINDING_LAYOUT{
    .nextInChain = nullptr,
    .sampleType = WGPUTextureSampleType_Undefined,
    .viewDimension = WGPUTextureViewDimension_Undefined,
    .multisampled = false,
};

constexpr WGPUStorageTextureBindingLayout DEFAULT_STORAGE_TEXTURE_BINDING_LAYOUT{
    .nextInChain = nullptr,
    .access = WGPUStorageTextureAccess_Undefined,
    .format = WGPUTextureFormat_Undefined,
    .viewDimension = WGPUTextureViewDimension_Undefined,
};

inline WGPUBindGroupLayoutEntry bufferBindGroupLayoutEntry(
    const std::uint32_t         bindingIdx,
    const WGPUShaderStageFlags  visibility,
    const WGPUBufferBindingType bindingType,
    const std::size_t           minBindingSize)
{
    return WGPUBindGroupLayoutEntry{
        .nextInChain = nullptr,
        .binding = bindingIdx,
        .visibility = visibility,
        .buffer =
            WGPUBufferBindingLayout{
                .nextInChain = nullptr,
                .type = bindingType,
                .hasDynamicOffset = false,
                .minBindingSize = static_cast<std::uint64_t>(minBindingSize)},
        .sampler = DEFAULT_SAMPLER_BINDING_LAYOUT,
        .texture = DEFAULT_TEXTURE_BINDING_LAYOUT,
        .storageTexture = DEFAULT_STORAGE_TEXTURE_BINDING_LAYOUT,
    };
}

inline WGPUBindGroupEntry bufferBindGroupEntry(
    const std::uint32_t bindingIdx,
    const WGPUBuffer    buffer,
    const std::size_t   byteSize)
{
    return WGPUBindGroupEntry{
        .nextInChain = nullptr,
        .binding = bindingIdx,
        .buffer = buffer,
        .offset = 0,
        .size = byteSize,
        .sampler = nullptr,
        .textureView = nullptr,
    };
}

inline WGPUBindGroupLayoutEntry textureBindGroupLayoutEntry(
    const std::uint32_t         bindingIdx,
    const WGPUTextureSampleType sampleType)
{
    return WGPUBindGroupLayoutEntry{
        .nextInChain = nullptr,
        .binding = bindingIdx,
        .visibility = WGPUShaderStage_Fragment,
        .buffer = DEFAULT_BUFFER_BINDING_LAYOUT,
        .sampler = DEFAULT_SAMPLER_BINDING_LAYOUT,
        .texture =
            WGPUTextureBindingLayout{
                .nextInChain = nullptr,
                .sampleType = sampleType,
                .viewDimension = WGPUTextureViewDimension_2D,
                .multisampled = false,
            },
        .storageTexture = DEFAULT_STORAGE_TEXTURE_BINDING_LAYOUT,
    };
}

inline WGPUBindGroupEntry textureBindGroupEntry(
    const std::uint32_t   bindingIdx,
    const WGPUTextureView textureView)
{
    return WGPUBindGroupEntry{
        .nextInChain = nullptr,
        .binding = bindingIdx,
        .buffer = nullptr,
        .offset = 0,
        .size = 0,
        .sampler = nullptr,
        .textureView = textureView,
    };
}

inline WGPUBindGroupLayoutEntry samplerBindGroupLayoutEntry(
    const std::uint32_t          bindingIdx,
    const WGPUSamplerBindingType samplerType)
{
    return WGPUBindGroupLayoutEntry{
        .nextInChain = nullptr,
        .binding = bindingIdx,
        .visibility = WGPUShaderStage_Fragment,
        .buffer = DEFAULT_BUFFER_BINDING_LAYOUT,
        .sampler =
            WGPUSamplerBindingLayout{
                .nextInChain = nullptr,
                .type = samplerType,
            },
        .texture = DEFAULT_TEXTURE_BINDING_LAYOUT,
        .storageTexture = DEFAULT_STORAGE_TEXTURE_BINDING_LAYOUT,
    };
}

inline WGPUBindGroupEntry samplerBindGroupEntry(
    const std::uint32_t bindingIdx,
    const WGPUSampler   sampler)
{
    return WGPUBindGroupEntry{
        .nextInChain = nullptr,
        .binding = bindingIdx,
        .buffer = nullptr,
        .offset = 0,
        .size = 0,
        .sampler = sampler,
        .textureView = nullptr,
    };
}

constexpr std::array<float[2], 6> quadVertexData{{
    // clang-format off
    {-1.0, -1.0,},
    {1.0, -1.0,},
    {1.0, 1.0,},
    {1.0, 1.0,},
    {-1.0, 1.0,},
    {-1.0, -1.0,},
    // clang-format on
}};
} // namespace nlrs
