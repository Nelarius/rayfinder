#pragma once

#include "gpu_bind_group.hpp"
#include "gpu_bind_group_layout.hpp"
#include "gpu_buffer.hpp"

#include <common/extent.hpp>
#include <common/texture.hpp>

#include <glm/glm.hpp>
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

    void render(const GpuContext&, WGPUTextureView, const glm::mat4& viewProjectionMatrix);
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
        std::vector<GpuBuffer>    mPositionBuffers;
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
            WGPUTextureView    albedoTextureView);
    };

    struct DebugPass
    {
        GpuBuffer          mVertexBuffer = GpuBuffer{};
        GpuBuffer          mUniformBuffer = GpuBuffer{}; // TODO: replace with WGPUConstantEntry
        GpuBindGroup       mUniformBindGroup = GpuBindGroup{};
        WGPURenderPipeline mPipeline = nullptr;

        DebugPass() = default;
        DebugPass(const GpuContext& gpuContext, const GpuBindGroupLayout& gbufferBindGroupLayout);
        ~DebugPass();

        DebugPass(const DebugPass&) = delete;
        DebugPass& operator=(const DebugPass&) = delete;

        DebugPass(DebugPass&&) noexcept;
        DebugPass& operator=(DebugPass&&) noexcept;

        void render(const GpuBindGroup&, WGPUCommandEncoder, WGPUTextureView);
    };

    WGPUTexture        mDepthTexture;
    WGPUTextureView    mDepthTextureView;
    WGPUTexture        mAlbedoTexture;
    WGPUTextureView    mAlbedoTextureView;
    WGPUSampler        mGbufferSampler;
    GpuBindGroupLayout mGbufferBindGroupLayout;
    GpuBindGroup       mGbufferBindGroup;
    GbufferPass        mGbufferPass;
    DebugPass          mDebugPass;
};
} // namespace nlrs
