#pragma once

#include "aligned_sky_state.hpp"
#include "gpu_bind_group.hpp"
#include "gpu_bind_group_layout.hpp"
#include "gpu_buffer.hpp"

#include <common/extent.hpp>
#include <common/texture.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <webgpu/webgpu.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace nlrs
{
struct GpuContext;

struct HybridRendererDescriptor
{
    Extent2u                                  framebufferSize;
    std::span<std::span<const glm::vec4>>     modelPositions;
    std::span<std::span<const glm::vec4>>     modelNormals;
    std::span<std::span<const glm::vec2>>     modelTexCoords;
    std::span<std::span<const std::uint32_t>> modelIndices;
    std::span<std::size_t>                    baseColorTextureIndices;
    std::span<Texture>                        baseColorTextures;
};

class HybridRenderer
{
public:
    HybridRenderer(const GpuContext&, const HybridRendererDescriptor&);
    ~HybridRenderer();

    HybridRenderer(const HybridRenderer&) = delete;
    HybridRenderer& operator=(const HybridRenderer&) = delete;

    HybridRenderer(HybridRenderer&&) = delete;
    HybridRenderer& operator=(HybridRenderer&&) = delete;

    void render(
        const GpuContext& gpuContext,
        const glm::mat4&  viewProjectionMatrix,
        const glm::vec3&  cameraPosition,
        const Sky&        sky,
        WGPUTextureView   targetTextureView);
    void renderDebug(const GpuContext&, const glm::mat4&, WGPUTextureView);
    void resize(const GpuContext&, const Extent2u&);

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
        GbufferPass(const GpuContext&, const HybridRendererDescriptor&);
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
            const GpuBindGroup& gbufferBindGroup,
            WGPUCommandEncoder  encoder,
            WGPUTextureView     textureView);
        void resize(const GpuContext&, const Extent2u&);
    };

    struct SkyPass
    {
    private:
        Sky                mCurrentSky;
        GpuBuffer          mVertexBuffer;
        GpuBuffer          mSkyStateBuffer;
        GpuBindGroup       mSkyStateBindGroup;
        GpuBuffer          mUniformBuffer;
        GpuBindGroup       mUniformBindGroup;
        WGPURenderPipeline mPipeline;

        struct Uniforms
        {
            glm::mat4 inverseViewProjection;
            glm::vec4 cameraPosition;
        };

    public:
        SkyPass(const GpuContext&);
        ~SkyPass();

        SkyPass(const SkyPass&) = delete;
        SkyPass& operator=(const SkyPass&) = delete;

        SkyPass(SkyPass&&) = delete;
        SkyPass& operator=(SkyPass&&) = delete;

        void render(
            const GpuContext&  gpuContext,
            const glm::mat4&   viewProjectionMatrix,
            const glm::vec3&   cameraPosition,
            const Sky&         sky,
            WGPUCommandEncoder cmdEncoder,
            WGPUTextureView    textureView);
    };

    WGPUTexture        mDepthTexture;
    WGPUTextureView    mDepthTextureView;
    WGPUTexture        mAlbedoTexture;
    WGPUTextureView    mAlbedoTextureView;
    WGPUTexture        mNormalTexture;
    WGPUTextureView    mNormalTextureView;
    GpuBindGroupLayout mGbufferBindGroupLayout;
    GpuBindGroup       mGbufferBindGroup;
    GbufferPass        mGbufferPass;
    DebugPass          mDebugPass;
    SkyPass            mSkyPass;
};
} // namespace nlrs
