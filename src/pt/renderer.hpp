#pragma once

#include "gpu_buffer.hpp"

#include <common/camera.hpp>
#include <common/extent.hpp>

#include <cstddef>
#include <cstdint>
#include <webgpu/webgpu.h>

namespace pt
{
struct Bvh;
struct GpuContext;

struct RenderParameters
{
    Extent2u framebufferSize;
    Camera   camera;
};

struct RendererDescriptor
{
    RenderParameters renderParams;
    Extent2i         maxFramebufferSize;
};

struct Renderer
{
    GpuBuffer          vertexBuffer;
    GpuBuffer          uniformsBuffer;
    WGPUBindGroup      uniformsBindGroup;
    GpuBuffer          renderParamsBuffer;
    WGPUBindGroup      renderParamsBindGroup;
    GpuBuffer          bvhNodeBuffer;
    GpuBuffer          triangleBuffer;
    WGPUBindGroup      sceneBindGroup;
    WGPURenderPipeline renderPipeline;

    RenderParameters currentRenderParams;
    std::uint32_t    frameCount;

    Renderer(const RendererDescriptor&, const GpuContext&, const Bvh& bvh);
    ~Renderer();

    void setRenderParameters(const RenderParameters&);
    void beginFrame();
    void render(const GpuContext&);

    static constexpr WGPURequiredLimits wgpuRequiredLimits{
        .nextInChain = nullptr,
        .limits =
            WGPULimits{
                .maxTextureDimension1D = 0,
                .maxTextureDimension2D = 0,
                .maxTextureDimension3D = 0,
                .maxTextureArrayLayers = 0,
                .maxBindGroups = 2,
                .maxBindGroupsPlusVertexBuffers = 0,
                .maxBindingsPerBindGroup = 1,
                .maxDynamicUniformBuffersPerPipelineLayout = 0,
                .maxDynamicStorageBuffersPerPipelineLayout = 0,
                .maxSampledTexturesPerShaderStage = 0,
                .maxSamplersPerShaderStage = 0,
                .maxStorageBuffersPerShaderStage = 0,
                .maxStorageTexturesPerShaderStage = 0,
                .maxUniformBuffersPerShaderStage = 1,
                .maxUniformBufferBindingSize = 80,
                .maxStorageBufferBindingSize = 132710400, // 4K resolution RGBA32F buffer,
                .minUniformBufferOffsetAlignment = 256,
                .minStorageBufferOffsetAlignment = 256,
                .maxVertexBuffers = 1,
                .maxBufferSize = 0,
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
