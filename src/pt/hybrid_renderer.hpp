#pragma once

#include "gpu_buffer.hpp"

#include <common/extent.hpp>

#include <glm/glm.hpp>
#include <webgpu/webgpu.h>

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
};

class HybridRenderer
{
public:
    HybridRenderer(const GpuContext&, HybridRendererDescriptor);
    ~HybridRenderer();

    HybridRenderer(const HybridRenderer&) = delete;
    HybridRenderer& operator=(const HybridRenderer&) = delete;

    HybridRenderer(HybridRenderer&&);
    HybridRenderer& operator=(HybridRenderer&&);

    void render(const GpuContext&, WGPUTextureView, const glm::mat4& viewProjectionMatrix);
    void resize(const GpuContext&, const Extent2u&);

private:
    struct IndexBuffer
    {
        GpuBuffer       buffer;
        std::uint32_t   count;
        WGPUIndexFormat format;
    };

    std::vector<GpuBuffer>   mPositionBuffers;
    std::vector<GpuBuffer>   mTexCoordBuffers;
    std::vector<IndexBuffer> mIndexBuffers;
    GpuBuffer                mUniformBuffer;
    WGPUBindGroup            mUniformBindGroup;
    WGPUTexture              mDepthTexture;
    WGPUTextureView          mDepthTextureView;
    WGPURenderPipeline       mPipeline;
};
} // namespace nlrs
