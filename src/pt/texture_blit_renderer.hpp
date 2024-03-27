#pragma once

#include "gpu_bind_group.hpp"
#include "gpu_bind_group_layout.hpp"
#include "gpu_buffer.hpp"

#include <common/extent.hpp>

#include <webgpu/webgpu.h>

namespace nlrs
{
struct GpuContext;
class Gui;

struct TextureBlitRendererDescriptor
{
    Extent2u framebufferSize;
};

class TextureBlitRenderer
{
public:
    TextureBlitRenderer(const GpuContext& context, const TextureBlitRendererDescriptor& descriptor);
    ~TextureBlitRenderer();

    TextureBlitRenderer(const TextureBlitRenderer&) = delete;
    TextureBlitRenderer& operator=(const TextureBlitRenderer&) = delete;

    TextureBlitRenderer(TextureBlitRenderer&&) = delete;
    TextureBlitRenderer& operator=(TextureBlitRenderer&&) = delete;

    // Rendering

    void render(const GpuContext&, Gui&, WGPUSwapChain);
    void resize(const GpuContext&, const Extent2u&);

    // Accessors

    inline WGPUTextureView textureView() const noexcept { return mTextureView; }

private:
    GpuBuffer          mVertexBuffer;
    WGPUTexture        mTexture;
    WGPUTextureView    mTextureView;
    WGPUSampler        mSampler;
    GpuBindGroupLayout mTextureBindGroupLayout;
    GpuBindGroup       mTextureBindGroup;
    WGPURenderPipeline mPipeline;
};
} // namespace nlrs
