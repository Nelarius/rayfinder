#pragma once

#include "gpu_buffer.hpp"

#include <common/extent.hpp>

#include <cstddef>
#include <cstdint>
#include <webgpu/webgpu.h>

namespace pt
{
struct GpuContext;

struct RendererDescriptor
{
    Extent2i currentFramebufferSize;
    Extent2i maxFramebufferSize;
};

struct Renderer
{
    GpuBuffer           frameDataBuffer;
    GpuBuffer           pixelBuffer;
    WGPUBindGroup       computePixelsBindGroup;
    WGPUComputePipeline computePipeline;

    GpuBuffer          vertexBuffer;
    GpuBuffer          uniformsBuffer;
    WGPUBindGroup      uniformsBindGroup;
    WGPUBindGroup      renderPixelsBindGroup;
    WGPURenderPipeline renderPipeline;

    Extent2i      currentFramebufferSize;
    std::uint32_t frameCount;

    Renderer(const RendererDescriptor&, const GpuContext&);
    ~Renderer();

    void render(const GpuContext&);
    void resizeFramebuffer(const Extent2i&);

    static constexpr WGPURequiredLimits wgpuRequiredLimits{
        .nextInChain = nullptr,
        .limits =
            WGPULimits{
                .maxTextureDimension1D = 0,
                .maxTextureDimension2D = 0,
                .maxTextureDimension3D = 0,
                .maxTextureArrayLayers = 0,
                .maxBindGroups = 1,
                .maxBindGroupsPlusVertexBuffers = 0,
                .maxBindingsPerBindGroup = 1,
                .maxDynamicUniformBuffersPerPipelineLayout = 0,
                .maxDynamicStorageBuffersPerPipelineLayout = 0,
                .maxSampledTexturesPerShaderStage = 0,
                .maxSamplersPerShaderStage = 0,
                .maxStorageBuffersPerShaderStage = 0,
                .maxStorageTexturesPerShaderStage = 0,
                .maxUniformBuffersPerShaderStage = 0,
                .maxUniformBufferBindingSize = 0,
                .maxStorageBufferBindingSize = 0,
                .minUniformBufferOffsetAlignment = 256,
                .minStorageBufferOffsetAlignment = 256,
                .maxVertexBuffers = 1,
                .maxBufferSize = 132710400, // 4K resolution RGBA32F buffer
                .maxVertexAttributes = 2,
                .maxVertexBufferArrayStride = 2 * sizeof(float),
                .maxInterStageShaderComponents = 0,
                .maxInterStageShaderVariables = 0,
                .maxColorAttachments = 0,
                .maxColorAttachmentBytesPerSample = 0,
                .maxComputeWorkgroupStorageSize = 0,
                .maxComputeInvocationsPerWorkgroup = 0,
                .maxComputeWorkgroupSizeX = 0,
                .maxComputeWorkgroupSizeY = 0,
                .maxComputeWorkgroupSizeZ = 0,
                .maxComputeWorkgroupsPerDimension = 0,
            },
    };
};
} // namespace pt
