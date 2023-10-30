#pragma once

#include <cstddef>
#include <webgpu/webgpu.h>

namespace pt
{
struct GpuContext;

struct Renderer
{
    WGPUBuffer         vertexBuffer;
    std::size_t        vertexBufferByteSize;
    WGPUBuffer         uniformsBuffer;
    WGPUBindGroup      uniformsBindGroup;
    WGPURenderPipeline renderPipeline;

    explicit Renderer(const GpuContext&);
    ~Renderer();

    void render(const GpuContext&);

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
                .minUniformBufferOffsetAlignment = 64,
                .minStorageBufferOffsetAlignment = 16,
                .maxVertexBuffers = 1,
                .maxBufferSize = 24 * sizeof(float),
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
