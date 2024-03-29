#pragma once

#include "aligned_sky_state.hpp"
#include "gpu_bind_group.hpp"
#include "gpu_buffer.hpp"

#include <common/bvh.hpp>
#include <common/camera.hpp>
#include <common/extent.hpp>
#include <common/texture.hpp>
#include <pt-format/vertex_attributes.hpp>

#include <webgpu/webgpu.h>

#include <array>
#include <cstdint>
#include <deque>
#include <span>

namespace nlrs
{
struct GpuContext;
class Window;

struct SamplingParams
{
    std::uint32_t numSamplesPerPixel = 128;
    std::uint32_t numBounces = 4;

    bool operator==(const SamplingParams&) const noexcept = default;
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

class ReferencePathTracer
{
public:
    ReferencePathTracer(const RendererDescriptor&, const GpuContext&, Scene);

    ReferencePathTracer(const ReferencePathTracer&) = delete;
    ReferencePathTracer& operator=(const ReferencePathTracer&) = delete;

    ReferencePathTracer(ReferencePathTracer&&);
    ReferencePathTracer& operator=(ReferencePathTracer&&);

    ~ReferencePathTracer();

    void setRenderParameters(const RenderParameters&);
    void setPostProcessingParameters(const PostProcessingParameters&);
    void render(const GpuContext&, WGPUTextureView);

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

private:
    GpuBuffer          mVertexBuffer;
    GpuBuffer          mRenderParamsBuffer;
    GpuBuffer          mPostProcessingParamsBuffer;
    GpuBuffer          mSkyStateBuffer;
    GpuBindGroup       mRenderParamsBindGroup;
    GpuBuffer          mBvhNodeBuffer;
    GpuBuffer          mPositionAttributesBuffer;
    GpuBuffer          mVertexAttributesBuffer;
    GpuBuffer          mTextureDescriptorBuffer;
    GpuBuffer          mTextureBuffer;
    GpuBindGroup       mSceneBindGroup;
    GpuBuffer          mImageBuffer;
    GpuBindGroup       mImageBindGroup;
    WGPUQuerySet       mQuerySet;
    GpuBuffer          mQueryBuffer;
    GpuBuffer          mTimestampBuffer;
    WGPURenderPipeline mRenderPipeline;

    RenderParameters         mCurrentRenderParams;
    PostProcessingParameters mCurrentPostProcessingParams;
    std::uint32_t            mFrameCount;
    std::uint32_t            mAccumulatedSampleCount;

    std::deque<std::uint64_t> mRenderPassDurationsNs;
};
} // namespace nlrs
