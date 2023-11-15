#pragma once

#include "gpu_buffer.hpp"

#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/extent.hpp>
#include <common/geometry.hpp>
#include <common/texture.hpp>
#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>

namespace nlrs
{
struct GpuContext;

struct SamplingParams
{
    std::uint32_t numSamplesPerPixel = 128;
    std::uint32_t numBounces = 4;

    bool operator==(const SamplingParams& rhs) const noexcept = default;
};

struct RenderParameters
{
    Extent2u       framebufferSize;
    Camera         camera;
    SamplingParams samplingParams;

    bool operator==(const RenderParameters& rhs) const noexcept = default;
};

struct Scene
{
    const Bvh&                     bvh;
    std::span<const Positions>     positions;
    std::span<const Normals>       normals;
    std::span<const TexCoords>     texCoords;
    std::span<const std::uint32_t> textureIndices;
    std::span<const Texture>       baseColorTextures;
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
    GpuBuffer          normalsBuffer;
    GpuBuffer          texCoordsBuffer;
    GpuBuffer          textureDescriptorIndicesBuffer;
    GpuBuffer          textureDescriptorBuffer;
    GpuBuffer          textureBuffer;
    WGPUBindGroup      sceneBindGroup;
    GpuBuffer          imageBuffer;
    WGPUBindGroup      imageBindGroup;
    WGPUQuerySet       querySet;
    GpuBuffer          queryBuffer;
    GpuBuffer          timestampBuffer;
    WGPURenderPipeline renderPipeline;

    RenderParameters currentRenderParams;
    std::uint32_t    frameCount;
    std::uint32_t    accumulatedSampleCount;

    std::deque<std::uint64_t> drawDurationsNs;
    std::deque<std::uint64_t> renderPassDurationsNs;

    struct TimestampBufferMapContext
    {
        GpuBuffer*                 timestampBuffer = nullptr;
        std::deque<std::uint64_t>* drawDurationsNs = nullptr;
        std::deque<std::uint64_t>* renderPassDurationsNs = nullptr;
    };

    TimestampBufferMapContext timestampBufferMapContext;

    Renderer(const RendererDescriptor&, const GpuContext&, Scene);
    ~Renderer();

    void setRenderParameters(const RenderParameters&);
    void beginFrame();
    void render(const GpuContext&);

    float averageDrawDurationMs() const;
    float averageRenderpassDurationMs() const;

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
                .maxBindingsPerBindGroup = 7,
                .maxDynamicUniformBuffersPerPipelineLayout = 0,
                .maxDynamicStorageBuffersPerPipelineLayout = 0,
                .maxSampledTexturesPerShaderStage = 0,
                .maxSamplersPerShaderStage = 0,
                .maxStorageBuffersPerShaderStage = 7,
                .maxStorageTexturesPerShaderStage = 0,
                .maxUniformBuffersPerShaderStage = 1,
                .maxUniformBufferBindingSize = 80,
                .maxStorageBufferBindingSize = 1 << 28, // 256 MiB
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
} // namespace nlrs
