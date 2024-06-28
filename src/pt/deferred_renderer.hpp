#pragma once

#include "aligned_sky_state.hpp"
#include "gpu_bind_group.hpp"
#include "gpu_bind_group_layout.hpp"
#include "gpu_buffer.hpp"

#include <common/bvh.hpp>
#include <common/extent.hpp>
#include <common/texture.hpp>
#include <pt-format/vertex_attributes.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <span>
#include <vector>

namespace nlrs
{
struct GpuContext;
class Gui;

struct DeferredRendererDescriptor
{
    Extent2u                                  framebufferSize;
    Extent2u                                  maxFramebufferSize;
    std::span<std::span<const glm::vec4>>     modelPositions;
    std::span<std::span<const glm::vec4>>     modelNormals;
    std::span<std::span<const glm::vec2>>     modelTexCoords;
    std::span<std::span<const std::uint32_t>> modelIndices;
    std::span<std::uint32_t>                  modelBaseColorTextureIndices;
    std::span<Texture>                        sceneBaseColorTextures;

    std::span<const BvhNode>           sceneBvhNodes;
    std::span<const PositionAttribute> scenePositionAttributes;
    std::span<const VertexAttributes>  sceneVertexAttributes;
};

struct RenderDescriptor
{
    glm::mat4       viewReverseZProjectionMatrix;
    glm::vec3       cameraPosition;
    Sky             sky;
    Extent2u        framebufferSize;
    float           exposure;
    WGPUTextureView targetTextureView;
};

class DeferredRenderer
{
public:
    struct PerfStats
    {
        float averageGbufferPassDurationsMs = 0.0f;
        float averageLightingPassDurationsMs = 0.0f;
        float averageResolvePassDurationsMs = 0.0f;
    };

    DeferredRenderer(const GpuContext&, const DeferredRendererDescriptor&);
    ~DeferredRenderer();

    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    DeferredRenderer(DeferredRenderer&&);
    DeferredRenderer& operator=(DeferredRenderer&&);

    void render(const GpuContext&, const RenderDescriptor&, Gui&);
    void renderDebug(const GpuContext&, const glm::mat4&, const Extent2f&, WGPUTextureView, Gui&);
    void resize(const GpuContext&, const Extent2u&);

    PerfStats getPerfStats() const;

private:
    struct IndexBuffer
    {
        GpuBuffer       buffer;
        std::uint32_t   count;
        WGPUIndexFormat format;
    };

    struct GpuTexture
    {
        WGPUTexture     texture;
        WGPUTextureView view;
    };

    struct GbufferPass
    {
    private:
        std::vector<GpuBuffer>    mPositionBuffers{};
        std::vector<GpuBuffer>    mNormalBuffers{};
        std::vector<GpuBuffer>    mTexCoordBuffers{};
        std::vector<IndexBuffer>  mIndexBuffers{};
        std::vector<std::size_t>  mBaseColorTextureIndices{};
        std::vector<GpuTexture>   mBaseColorTextures{};
        std::vector<GpuBindGroup> mBaseColorTextureBindGroups{};
        WGPUSampler               mBaseColorSampler = nullptr;
        GpuBuffer                 mUniformBuffer{};
        GpuBindGroup              mUniformBindGroup{};
        GpuBindGroup              mSamplerBindGroup{};
        WGPURenderPipeline        mPipeline = nullptr;

        struct Uniforms
        {
            glm::mat4 viewProjectionMat;
            glm::mat4 jitterMat;
        };

    public:
        GbufferPass() = default;
        GbufferPass(const GpuContext&, const DeferredRendererDescriptor&);
        ~GbufferPass();

        GbufferPass(const GbufferPass&) = delete;
        GbufferPass& operator=(const GbufferPass&) = delete;

        GbufferPass(GbufferPass&&) noexcept;
        GbufferPass& operator=(GbufferPass&&) noexcept;

        void render(
            const GpuContext&  gpuContext,
            const glm::mat4&   viewProjectionMat,
            const glm::mat4&   jitterMat,
            WGPUCommandEncoder cmdEncoder,
            WGPUTextureView    depthTextureView,
            WGPUTextureView    albedoTextureView,
            WGPUTextureView    normalTextureView);
    };

    struct DebugPass
    {
    private:
        GpuBuffer          mVertexBuffer = GpuBuffer{};
        GpuBuffer          mUniformBuffer = GpuBuffer{};
        GpuBindGroup       mUniformBindGroup = GpuBindGroup{};
        GpuBindGroupLayout mGbufferBindGroupLayout = GpuBindGroupLayout{};
        GpuBindGroup       mGbufferBindGroup = GpuBindGroup{};
        WGPURenderPipeline mPipeline = nullptr;

    public:
        DebugPass() = default;
        DebugPass(
            const GpuContext& gpuContext,
            WGPUTextureView   albedoTextureView,
            WGPUTextureView   normalTextureView,
            WGPUTextureView   depthTextureView,
            const Extent2u&   framebufferSize);
        ~DebugPass();

        DebugPass(const DebugPass&) = delete;
        DebugPass& operator=(const DebugPass&) = delete;

        DebugPass(DebugPass&&) noexcept;
        DebugPass& operator=(DebugPass&&) noexcept;

        void render(
            const GpuContext&  gpuContext,
            WGPUCommandEncoder encoder,
            WGPUTextureView    textureView,
            const Extent2f&    framebufferSize,
            Gui&               gui);
        void resize(
            const GpuContext&,
            WGPUTextureView albedoTextureView,
            WGPUTextureView normalTextureView,
            WGPUTextureView depthTextureView);
    };

    struct LightingPass
    {
    private:
        Sky                 mCurrentSky = Sky{};
        GpuBuffer           mSkyStateBuffer = GpuBuffer{};
        GpuBuffer           mUniformBuffer = GpuBuffer{};
        GpuBindGroup        mUniformBindGroup = GpuBindGroup{};
        GpuBindGroupLayout  mGbufferBindGroupLayout = GpuBindGroupLayout{};
        GpuBindGroup        mGbufferBindGroup = GpuBindGroup{};
        GpuBuffer           mBvhNodeBuffer = GpuBuffer{};
        GpuBuffer           mPositionAttributesBuffer = GpuBuffer{};
        GpuBuffer           mVertexAttributesBuffer = GpuBuffer{};
        GpuBuffer           mTextureDescriptorBuffer = GpuBuffer{};
        GpuBuffer           mTextureBuffer = GpuBuffer{};
        GpuBuffer           mBlueNoiseBuffer = GpuBuffer{};
        GpuBindGroup        mBvhBindGroup = GpuBindGroup{};
        GpuBindGroup        mSampleBindGroup = GpuBindGroup{};
        WGPUComputePipeline mPipeline = nullptr;

        struct Uniforms
        {
            glm::mat4     inverseViewReverseZProjectionMat;
            glm::vec4     cameraPosition;
            glm::vec2     framebufferSize;
            std::uint32_t frameCount;
            std::uint32_t _padding;
        };

    public:
        LightingPass() = default;
        LightingPass(
            const GpuContext&                  gpuContext,
            WGPUTextureView                    albedoTextureView,
            WGPUTextureView                    normalTextureView,
            WGPUTextureView                    depthTextureView,
            const GpuBuffer&                   accumulationBuffer,
            std::span<const BvhNode>           bvhNodes,
            std::span<const PositionAttribute> positionAttributes,
            std::span<const VertexAttributes>  vertexAttributes,
            std::span<const Texture>           baseColorTextures);
        ~LightingPass();

        LightingPass(const LightingPass&) = delete;
        LightingPass& operator=(const LightingPass&) = delete;

        LightingPass(LightingPass&&) noexcept;
        LightingPass& operator=(LightingPass&&) noexcept;

        void render(
            const GpuContext&  gpuContext,
            WGPUCommandEncoder cmdEncoder,
            const glm::mat4&   inverseViewProjection,
            const glm::vec3&   cameraPosition,
            const Extent2f&    framebufferSize,
            const Sky&         sky,
            std::uint32_t      frameCount);
        void resize(
            const GpuContext&,
            WGPUTextureView albedoTextureView,
            WGPUTextureView normalTextureView,
            WGPUTextureView depthTextureView);
    };

    struct ResolvePass
    {
    private:
        GpuBuffer          mVertexBuffer = GpuBuffer{};
        GpuBuffer          mUniformBuffer = GpuBuffer{};
        GpuBindGroup       mUniformBindGroup = GpuBindGroup{};
        GpuBuffer          mAccumulationBuffer = GpuBuffer{};
        GpuBindGroup       mTaaBindGroup = GpuBindGroup{};
        WGPURenderPipeline mPipeline = nullptr;

        struct Uniforms
        {
            glm::vec2     framebufferSize;
            float         exposure;
            std::uint32_t frameCount;
        };

    public:
        ResolvePass() = default;
        ResolvePass(
            const GpuContext&                 gpuContext,
            const GpuBuffer&                  sampleBuffer,
            const DeferredRendererDescriptor& desc);
        ~ResolvePass();

        ResolvePass(const ResolvePass&) = delete;
        ResolvePass& operator=(const ResolvePass&) = delete;

        ResolvePass(ResolvePass&&) noexcept;
        ResolvePass& operator=(ResolvePass&&) noexcept;

        void render(
            const GpuContext&  gpuContext,
            WGPUCommandEncoder cmdEncoder,
            WGPUTextureView    targetTextureView,
            const Extent2f&    framebufferSize,
            float              exposure,
            std::uint32_t      frameCount,
            Gui&               gui);
    };

    void invalidateTemporalAccumulation();

    WGPUTexture               mDepthTexture;
    WGPUTextureView           mDepthTextureView;
    WGPUTexture               mAlbedoTexture;
    WGPUTextureView           mAlbedoTextureView;
    WGPUTexture               mNormalTexture;
    WGPUTextureView           mNormalTextureView;
    GpuBuffer                 mSampleBuffer;
    WGPUQuerySet              mQuerySet;
    GpuBuffer                 mQueryBuffer;
    GpuBuffer                 mTimestampsBuffer;
    GbufferPass               mGbufferPass;
    DebugPass                 mDebugPass;
    LightingPass              mLightingPass;
    ResolvePass               mResolvePass;
    std::deque<std::uint64_t> mGbufferPassDurationsNs;
    std::deque<std::uint64_t> mLightingPassDurationsNs;
    std::deque<std::uint64_t> mResolvePassDurationsNs;
    std::uint32_t             mFrameCount;
};
} // namespace nlrs
