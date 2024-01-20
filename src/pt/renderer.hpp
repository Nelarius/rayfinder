#pragma once

#include "gpu_buffer.hpp"

#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/extent.hpp>
#include <common/texture.hpp>
#include <webgpu/webgpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>

namespace nlrs
{
struct GpuContext;
class Gui;
class Window;

struct SamplingParams
{
    std::uint32_t numSamplesPerPixel = 128;
    std::uint32_t numBounces = 4;

    bool operator==(const SamplingParams&) const noexcept = default;
};

struct Sky
{
    float                turbidity = 1.0f;
    std::array<float, 3> albedo = {1.0f, 1.0f, 1.0f};
    float                sunZenithDegrees = 30.0f;
    float                sunAzimuthDegrees = 0.0f;

    bool operator==(const Sky&) const noexcept = default;
};

struct RenderParameters
{
    Extent2u       framebufferSize;
    Camera         camera;
    SamplingParams samplingParams;
    Sky            sky;

    bool operator==(const RenderParameters&) const noexcept = default;
};

enum class Tonemapping : std::uint32_t
{
    Linear = 0,
    Filmic,
};

struct PostProcessingParameters
{
    // Exposure is calculated as 1 / (2 ^ stops), stops >= 0. Increasing a stop by one halves the
    // exposure.
    std::uint32_t stops = 0;
    Tonemapping   tonemapping = Tonemapping::Filmic;
};

struct PositionAttribute
{
    glm::vec3 p0;   // offset: 0, size: 12
    float     pad0; // offset: 12, size: 4
    glm::vec3 p1;   // offset: 16, size: 12
    float     pad1; // offset: 28, size: 4
    glm::vec3 p2;   // offset: 32, size: 12
    float     pad2; // offset: 44, size: 4
};

struct VertexAttributes
{
    // Normals
    glm::vec3 n0;   // offset 0, size: 12
    float     pad0; // offset 12, size: 4
    glm::vec3 n1;   // offset 16, size: 12
    float     pad1; // offset 28, size: 4
    glm::vec3 n2;   // offset 32, size: 12
    float     pad2; // offset 44, size: 4

    // Texture coordinates
    glm::vec2 uv0; // offset 48, size: 8
    glm::vec2 uv1; // offset 56, size: 8
    glm::vec2 uv2; // offset 64, size: 8

    // Texture index
    std::uint32_t textureIdx; // offset 72, size: 4
    std::uint32_t pad3;       // offset: 76, size 4
};

struct Scene
{
    std::span<const BvhNode>           bvhNodes;
    std::span<const PositionAttribute> positionAttributes;
    std::span<const VertexAttributes>  vertexAttributes;
    std::span<const Texture>           baseColorTextures;
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
    GpuBuffer          postProcessingParamsBuffer;
    GpuBuffer          skyStateBuffer;
    WGPUBindGroup      renderParamsBindGroup;
    GpuBuffer          bvhNodeBuffer;
    GpuBuffer          positionAttributesBuffer;
    GpuBuffer          vertexAttributesBuffer;
    GpuBuffer          textureDescriptorBuffer;
    GpuBuffer          textureBuffer;
    WGPUBindGroup      sceneBindGroup;
    GpuBuffer          imageBuffer;
    WGPUBindGroup      imageBindGroup;
    WGPUQuerySet       querySet;
    GpuBuffer          queryBuffer;
    GpuBuffer          timestampBuffer;
    WGPURenderPipeline renderPipeline;

    RenderParameters         currentRenderParams;
    PostProcessingParameters currentPostProcessingParams;
    std::uint32_t            frameCount;
    std::uint32_t            accumulatedSampleCount;

    std::deque<std::uint64_t> renderPassDurationsNs;

    Renderer(const RendererDescriptor&, const GpuContext&, Scene);

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    Renderer(Renderer&&);
    Renderer& operator=(Renderer&&);

    ~Renderer();

    void setRenderParameters(const RenderParameters&);
    void setPostProcessingParameters(const PostProcessingParameters&);
    void render(const GpuContext&, const Window&, Gui&);

    float averageRenderpassDurationMs() const;
    float renderProgressPercentage() const;

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
                .maxStorageBuffersPerShaderStage = 6,
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
