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
class Gui;
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
    float          exposure;

    bool operator==(const RenderParameters&) const noexcept = default;
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
    void render(const GpuContext&, WGPUTextureView, Gui&);

    float averageRenderpassDurationMs() const;
    float renderProgressPercentage() const;

private:
    GpuBuffer          mVertexBuffer;
    GpuBuffer          mRenderParamsBuffer;
    GpuBuffer          mSkyStateBuffer;
    GpuBindGroup       mRenderParamsBindGroup;
    GpuBuffer          mBvhNodeBuffer;
    GpuBuffer          mPositionAttributesBuffer;
    GpuBuffer          mVertexAttributesBuffer;
    GpuBuffer          mTextureDescriptorBuffer;
    GpuBuffer          mTextureBuffer;
    GpuBuffer          mBlueNoiseBuffer;
    GpuBindGroup       mSceneBindGroup;
    GpuBuffer          mImageBuffer;
    GpuBindGroup       mImageBindGroup;
    WGPUQuerySet       mQuerySet;
    GpuBuffer          mQueryBuffer;
    GpuBuffer          mTimestampBuffer;
    WGPURenderPipeline mRenderPipeline;

    RenderParameters mCurrentRenderParams;
    std::uint32_t    mFrameCount;
    std::uint32_t    mAccumulatedSampleCount;

    std::deque<std::uint64_t> mRenderPassDurationsNs;
};
} // namespace nlrs
