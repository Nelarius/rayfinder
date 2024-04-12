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

// TODO: the naming could be considered inconsistent here. What about modelBvhNodes, etc. and meshPositions, meshNormals, etc.?
struct DeferredRendererDescriptor
{
    Extent2u                                  framebufferSize;
    std::span<std::span<const glm::vec4>>     modelPositions;
    std::span<std::span<const glm::vec4>>     modelNormals;
    std::span<std::span<const glm::vec2>>     modelTexCoords;
    std::span<std::span<const std::uint32_t>> modelIndices;
    std::span<std::size_t>                    modelBaseColorTextureIndices;
    std::span<Texture>                        sceneBaseColorTextures;

    std::span<const BvhNode>           sceneBvhNodes;
    std::span<const PositionAttribute> scenePositionAttributes;
    std::span<const VertexAttributes>  sceneVertexAttributes;
};

struct RenderDescriptor
{
    glm::mat4       viewProjectionMatrix;
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
    };

    DeferredRenderer(const GpuContext&, const DeferredRendererDescriptor&);
    ~DeferredRenderer();

    DeferredRenderer(const DeferredRenderer&) = delete;
    DeferredRenderer& operator=(const DeferredRenderer&) = delete;

    DeferredRenderer(DeferredRenderer&&) = delete;
    DeferredRenderer& operator=(DeferredRenderer&&) = delete;

    void render(const GpuContext& gpuContext, const RenderDescriptor& renderDescriptor);
    void renderDebug(const GpuContext&, const glm::mat4&, const Extent2f&, WGPUTextureView);
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
        std::vector<GpuBuffer>    mPositionBuffers;
        std::vector<GpuBuffer>    mNormalBuffers;
        std::vector<GpuBuffer>    mTexCoordBuffers;
        std::vector<IndexBuffer>  mIndexBuffers;
        std::vector<std::size_t>  mBaseColorTextureIndices;
        std::vector<GpuTexture>   mBaseColorTextures;
        std::vector<GpuBindGroup> mBaseColorTextureBindGroups;
        WGPUSampler               mBaseColorSampler;
        GpuBuffer                 mUniformBuffer;
        GpuBindGroup              mUniformBindGroup;
        GpuBindGroup              mSamplerBindGroup;
        WGPURenderPipeline        mPipeline;

    public:
        GbufferPass(const GpuContext&, const DeferredRendererDescriptor&);
        ~GbufferPass();

        GbufferPass(const GbufferPass&) = delete;
        GbufferPass& operator=(const GbufferPass&) = delete;

        GbufferPass(GbufferPass&&) = delete;
        GbufferPass& operator=(GbufferPass&&) = delete;

        void render(
            const GpuContext&  gpuContext,
            const glm::mat4&   viewProjectionMat,
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
        WGPURenderPipeline mPipeline = nullptr;

    public:
        DebugPass() = default;
        DebugPass(
            const GpuContext&         gpuContext,
            const GpuBindGroupLayout& gbufferBindGroupLayout,
            const Extent2u&           framebufferSize);
        ~DebugPass();

        DebugPass(const DebugPass&) = delete;
        DebugPass& operator=(const DebugPass&) = delete;

        DebugPass(DebugPass&&) noexcept;
        DebugPass& operator=(DebugPass&&) noexcept;

        void render(
            const GpuContext&   gpuContext,
            const GpuBindGroup& gbufferBindGroup,
            WGPUCommandEncoder  encoder,
            WGPUTextureView     textureView,
            const Extent2f&     framebufferSize);
    };

    struct LightingPass
    {
    private:
        Sky                mCurrentSky = Sky{};
        GpuBuffer          mVertexBuffer = GpuBuffer{};
        GpuBuffer          mSkyStateBuffer = GpuBuffer{};
        GpuBindGroup       mSkyStateBindGroup = GpuBindGroup{};
        GpuBuffer          mUniformBuffer = GpuBuffer{};
        GpuBindGroup       mUniformBindGroup = GpuBindGroup{};
        GpuBuffer          mBvhNodeBuffer;
        GpuBuffer          mPositionAttributesBuffer;
        GpuBuffer          mVertexAttributesBuffer;
        GpuBuffer          mTextureDescriptorBuffer;
        GpuBuffer          mTextureBuffer;
        GpuBindGroup       mBvhBindGroup;
        WGPURenderPipeline mPipeline = nullptr;

        struct Uniforms
        {
            glm::mat4 inverseViewProjection;
            glm::vec4 cameraPosition;
            glm::vec2 framebufferSize;
            float     exposure;
            float     padding;
        };

    public:
        LightingPass() = default;
        LightingPass(
            const GpuContext&                  gpuContext,
            const GpuBindGroupLayout&          gbufferBindGroupLayout,
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
            const GpuContext&   gpuContext,
            const GpuBindGroup& gbufferBindGroup,
            WGPUCommandEncoder  cmdEncoder,
            WGPUTextureView     targetTextureView,
            const glm::mat4&    inverseViewProjection,
            const glm::vec3&    cameraPosition,
            const Extent2f&     framebufferSize,
            const Sky&          sky,
            float               exposure);
    };

    WGPUTexture               mDepthTexture;
    WGPUTextureView           mDepthTextureView;
    WGPUTexture               mAlbedoTexture;
    WGPUTextureView           mAlbedoTextureView;
    WGPUTexture               mNormalTexture;
    WGPUTextureView           mNormalTextureView;
    GpuBindGroupLayout        mGbufferBindGroupLayout;
    GpuBindGroup              mGbufferBindGroup;
    WGPUQuerySet              mQuerySet;
    GpuBuffer                 mQueryBuffer;
    GpuBuffer                 mTimestampsBuffer;
    GbufferPass               mGbufferPass;
    DebugPass                 mDebugPass;
    LightingPass              mLightingPass;
    std::deque<std::uint64_t> mGbufferPassDurationsNs;
    std::deque<std::uint64_t> mLightingPassDurationsNs;
};
} // namespace nlrs
