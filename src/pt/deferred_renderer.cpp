#include "blue_noise.h"
#include "gpu_context.hpp"
#include "gpu_limits.hpp"
#include "gui.hpp"
#include "deferred_renderer.hpp"
#include "shader_source.hpp"
#include "webgpu_utils.hpp"
#include "window.hpp"

#include <common/assert.hpp>
#include <common/r_sequence.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <numeric>

namespace nlrs
{
namespace
{
const WGPUTextureFormat DEPTH_TEXTURE_FORMAT = WGPUTextureFormat_Depth32Float;
const WGPUTextureFormat ALBEDO_TEXTURE_FORMAT = WGPUTextureFormat_BGRA8Unorm;
const WGPUTextureFormat NORMAL_TEXTURE_FORMAT = WGPUTextureFormat_RGBA16Float;
const WGPUTextureFormat VELOCITY_TEXTURE_FORMAT = WGPUTextureFormat_RG16Float;

struct TimestampsLayout
{
    std::uint64_t gbufferPassStart;
    std::uint64_t gbufferPassEnd;
    std::uint64_t lightingPassStart;
    std::uint64_t lightingPassEnd;
    std::uint64_t resolvePassStart;
    std::uint64_t resolvePassEnd;

    static constexpr std::uint32_t QUERY_COUNT = 6;
    static constexpr std::size_t   MEMBER_SIZE = sizeof(std::uint64_t);
};

WGPUTexture createGbufferTexture(
    const WGPUDevice            device,
    const char* const           label,
    const WGPUTextureUsageFlags usage,
    const Extent2u              size,
    const WGPUTextureFormat     format)
{
    const WGPUTextureDescriptor desc{
        .nextInChain = nullptr,
        .label = label,
        .usage = usage,
        .dimension = WGPUTextureDimension_2D,
        .size = {size.x, size.y, 1},
        .format = format,
        .mipLevelCount = 1,
        .sampleCount = 1,
        .viewFormatCount = 1,
        .viewFormats = &format,
    };
    return wgpuDeviceCreateTexture(device, &desc);
}

WGPUTextureView createGbufferTextureView(
    const WGPUTexture       texture,
    const char* const       label,
    const WGPUTextureFormat format)
{
    const WGPUTextureViewDescriptor desc{
        .nextInChain = nullptr,
        .label = label,
        .format = format,
        .dimension = WGPUTextureViewDimension_2D,
        .baseMipLevel = 0,
        .mipLevelCount = 1,
        .baseArrayLayer = 0,
        .arrayLayerCount = 1,
        .aspect = WGPUTextureAspect_All,
    };
    return wgpuTextureCreateView(texture, &desc);
}

// TODO: re-enable jitter
// glm::mat4 jitterMatrix(const Extent2f& framebufferSize, const std::uint32_t frameCount)
// {
//     glm::mat4       jitterMat = glm::mat4(1.0f);
//     const glm::vec2 j = r2Sequence(frameCount, 1 << 20);
//     jitterMat[3][0] = (j.x - 0.5f) / framebufferSize.x;
//     jitterMat[3][1] = (j.y - 0.5f) / framebufferSize.y;
//     return jitterMat;
// }
} // namespace

DeferredRenderer::DeferredRenderer(
    const GpuContext&                 gpuContext,
    const DeferredRendererDescriptor& rendererDesc)
    : mDepthTexture(nullptr),
      mDepthTextureView(nullptr),
      mAlbedoTexture(nullptr),
      mAlbedoTextureView(nullptr),
      mNormalTexture(nullptr),
      mNormalTextureView(nullptr),
      mVelocityTexture(nullptr),
      mVelocityTextureView(nullptr),
      mSampleBuffer(
          gpuContext.device,
          "Deferred renderer :: sample buffer",
          GpuBufferUsages{GpuBufferUsage::Storage},
          3 * sizeof(float) * area(rendererDesc.maxFramebufferSize)),
      mQuerySet(nullptr),
      mQueryBuffer(
          gpuContext.device,
          "Deferred renderer query buffer",
          {GpuBufferUsage::QueryResolve, GpuBufferUsage::CopySrc},
          sizeof(TimestampsLayout)),
      mTimestampsBuffer(
          gpuContext.device,
          "Deferred renderer timestamp buffer",
          {GpuBufferUsage::CopyDst, GpuBufferUsage::MapRead},
          sizeof(TimestampsLayout)),
      mGbufferPass(gpuContext, rendererDesc),
      mDebugPass(),
      mLightingPass(),
      mResolvePass(),
      mGbufferPassDurationsNs(),
      mLightingPassDurationsNs(),
      mFrameCount(0)
{
    {
        const std::array<WGPUTextureFormat, 1> depthFormats{
            DEPTH_TEXTURE_FORMAT,
        };
        const WGPUTextureDescriptor depthTextureDesc{
            .nextInChain = nullptr,
            .label = "Depth texture",
            .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
            .dimension = WGPUTextureDimension_2D,
            .size = {rendererDesc.framebufferSize.x, rendererDesc.framebufferSize.y, 1},
            .format = DEPTH_TEXTURE_FORMAT,
            .mipLevelCount = 1,
            .sampleCount = 1,
            .viewFormatCount = depthFormats.size(),
            .viewFormats = depthFormats.data(),
        };
        mDepthTexture = wgpuDeviceCreateTexture(gpuContext.device, &depthTextureDesc);
        NLRS_ASSERT(mDepthTexture != nullptr);

        const WGPUTextureViewDescriptor depthTextureViewDesc{
            .nextInChain = nullptr,
            .label = "Depth texture view",
            .format = DEPTH_TEXTURE_FORMAT,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_DepthOnly,
        };
        mDepthTextureView = wgpuTextureCreateView(mDepthTexture, &depthTextureViewDesc);
        NLRS_ASSERT(mDepthTextureView != nullptr);
    }

    mAlbedoTexture = createGbufferTexture(
        gpuContext.device,
        "Gbuffer albedo texture",
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
        rendererDesc.framebufferSize,
        ALBEDO_TEXTURE_FORMAT);
    NLRS_ASSERT(mAlbedoTexture != nullptr);

    mAlbedoTextureView = createGbufferTextureView(
        mAlbedoTexture, "Gbuffer albedo texture view", ALBEDO_TEXTURE_FORMAT);
    NLRS_ASSERT(mAlbedoTextureView != nullptr);

    mNormalTexture = createGbufferTexture(
        gpuContext.device,
        "Gbuffer normal texture",
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
        rendererDesc.framebufferSize,
        NORMAL_TEXTURE_FORMAT);
    NLRS_ASSERT(mNormalTexture != nullptr);

    mNormalTextureView = createGbufferTextureView(
        mNormalTexture, "Gbuffer normal texture view", NORMAL_TEXTURE_FORMAT);
    NLRS_ASSERT(mNormalTextureView != nullptr);

    mVelocityTexture = createGbufferTexture(
        gpuContext.device,
        "Gbuffer velocity texture",
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
        rendererDesc.framebufferSize,
        VELOCITY_TEXTURE_FORMAT);
    NLRS_ASSERT(mVelocityTexture != nullptr);

    mVelocityTextureView = createGbufferTextureView(
        mVelocityTexture, "Gbuffer velocity texture view", VELOCITY_TEXTURE_FORMAT);
    NLRS_ASSERT(mVelocityTextureView != nullptr);

    {
        const WGPUQuerySetDescriptor querySetDesc{
            .nextInChain = nullptr,
            .label = "Deferred renderer query set",
            .type = WGPUQueryType_Timestamp,
            .count = TimestampsLayout::QUERY_COUNT};
        mQuerySet = wgpuDeviceCreateQuerySet(gpuContext.device, &querySetDesc);
    }

    mDebugPass = DebugPass{
        gpuContext,
        mAlbedoTextureView,
        mNormalTextureView,
        mDepthTextureView,
        mVelocityTextureView,
        rendererDesc.framebufferSize};
    mLightingPass = LightingPass{
        gpuContext,
        mAlbedoTextureView,
        mNormalTextureView,
        mDepthTextureView,
        mSampleBuffer,
        rendererDesc.sceneBvhNodes,
        rendererDesc.scenePositionAttributes,
        rendererDesc.sceneVertexAttributes,
        rendererDesc.sceneBaseColorTextures};
    mResolvePass = ResolvePass{gpuContext, mSampleBuffer, mVelocityTextureView, rendererDesc};
}

DeferredRenderer::~DeferredRenderer()
{
    querySetSafeRelease(mQuerySet);
    mQuerySet = nullptr;
    textureViewSafeRelease(mVelocityTextureView);
    mVelocityTextureView = nullptr;
    textureSafeRelease(mVelocityTexture);
    mVelocityTexture = nullptr;
    textureViewSafeRelease(mNormalTextureView);
    mNormalTextureView = nullptr;
    textureSafeRelease(mNormalTexture);
    mNormalTexture = nullptr;
    textureViewSafeRelease(mAlbedoTextureView);
    mAlbedoTextureView = nullptr;
    textureSafeRelease(mAlbedoTexture);
    mAlbedoTexture = nullptr;
    textureViewSafeRelease(mDepthTextureView);
    mDepthTextureView = nullptr;
    textureSafeRelease(mDepthTexture);
    mDepthTexture = nullptr;
}

DeferredRenderer::DeferredRenderer(DeferredRenderer&& other)
{
    if (this != &other)
    {
        mDepthTexture = other.mDepthTexture;
        other.mDepthTexture = nullptr;
        mDepthTextureView = other.mDepthTextureView;
        other.mDepthTextureView = nullptr;
        mAlbedoTexture = other.mAlbedoTexture;
        other.mAlbedoTexture = nullptr;
        mAlbedoTextureView = other.mAlbedoTextureView;
        other.mAlbedoTextureView = nullptr;
        mNormalTexture = other.mNormalTexture;
        other.mNormalTexture = nullptr;
        mNormalTextureView = other.mNormalTextureView;
        other.mNormalTextureView = nullptr;
        mVelocityTexture = other.mVelocityTexture;
        other.mVelocityTexture = nullptr;
        mVelocityTextureView = other.mVelocityTextureView;
        other.mVelocityTextureView = nullptr;
        mSampleBuffer = std::move(other.mSampleBuffer);
        mQuerySet = other.mQuerySet;
        other.mQuerySet = nullptr;
        mQueryBuffer = std::move(other.mQueryBuffer);
        mTimestampsBuffer = std::move(other.mTimestampsBuffer);
        mGbufferPass = std::move(other.mGbufferPass);
        mDebugPass = std::move(other.mDebugPass);
        mLightingPass = std::move(other.mLightingPass);
        mResolvePass = std::move(other.mResolvePass);
        mGbufferPassDurationsNs = std::move(other.mGbufferPassDurationsNs);
        mLightingPassDurationsNs = std::move(other.mLightingPassDurationsNs);
        mFrameCount = other.mFrameCount;
    }
}

DeferredRenderer& DeferredRenderer::operator=(DeferredRenderer&& other)
{
    if (this != &other)
    {
        mDepthTexture = other.mDepthTexture;
        other.mDepthTexture = nullptr;
        mDepthTextureView = other.mDepthTextureView;
        other.mDepthTextureView = nullptr;
        mAlbedoTexture = other.mAlbedoTexture;
        other.mAlbedoTexture = nullptr;
        mAlbedoTextureView = other.mAlbedoTextureView;
        other.mAlbedoTextureView = nullptr;
        mNormalTexture = other.mNormalTexture;
        other.mNormalTexture = nullptr;
        mNormalTextureView = other.mNormalTextureView;
        other.mNormalTextureView = nullptr;
        mVelocityTexture = other.mVelocityTexture;
        other.mVelocityTexture = nullptr;
        mVelocityTextureView = other.mVelocityTextureView;
        other.mVelocityTextureView = nullptr;
        mSampleBuffer = std::move(other.mSampleBuffer);
        mQuerySet = other.mQuerySet;
        other.mQuerySet = nullptr;
        mQueryBuffer = std::move(other.mQueryBuffer);
        mTimestampsBuffer = std::move(other.mTimestampsBuffer);
        mGbufferPass = std::move(other.mGbufferPass);
        mDebugPass = std::move(other.mDebugPass);
        mLightingPass = std::move(other.mLightingPass);
        mResolvePass = std::move(other.mResolvePass);
        mGbufferPassDurationsNs = std::move(other.mGbufferPassDurationsNs);
        mLightingPassDurationsNs = std::move(other.mLightingPassDurationsNs);
        mFrameCount = other.mFrameCount;
    }
    return *this;
}

void DeferredRenderer::render(
    const GpuContext&       gpuContext,
    const RenderDescriptor& renderDesc,
    Gui&                    gui)
{
    // Non-standard Dawn way to ensure that Dawn ticks pending async operations.
    do
    {
        wgpuDeviceTick(gpuContext.device);
    } while (wgpuBufferGetMapState(mTimestampsBuffer.ptr()) != WGPUBufferMapState_Unmapped);

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    const Extent2f      framebufferSize = Extent2f(renderDesc.framebufferSize);
    const std::uint32_t frameCount = mFrameCount++;
    // const glm::mat4     jitterMat = jitterMatrix(framebufferSize, frameCount);

    // GBuffer pass

    wgpuCommandEncoderWriteTimestamp(
        encoder,
        mQuerySet,
        offsetof(TimestampsLayout, gbufferPassStart) / TimestampsLayout::MEMBER_SIZE);
    mGbufferPass.render(
        gpuContext,
        encoder,
        mDepthTextureView,
        mAlbedoTextureView,
        mNormalTextureView,
        mVelocityTextureView,
        renderDesc.viewReverseZProjectionMatrix,
        frameCount);
    wgpuCommandEncoderWriteTimestamp(
        encoder,
        mQuerySet,
        offsetof(TimestampsLayout, gbufferPassEnd) / TimestampsLayout::MEMBER_SIZE);

    // Lighting pass

    wgpuCommandEncoderWriteTimestamp(
        encoder,
        mQuerySet,
        offsetof(TimestampsLayout, lightingPassStart) / TimestampsLayout::MEMBER_SIZE);
    {
        const glm::mat4 inverseViewProjectionMat =
            glm::inverse(renderDesc.viewReverseZProjectionMatrix);
        mLightingPass.render(
            gpuContext,
            encoder,
            inverseViewProjectionMat,
            renderDesc.cameraPosition,
            framebufferSize,
            renderDesc.sky,
            frameCount);
    }
    wgpuCommandEncoderWriteTimestamp(
        encoder,
        mQuerySet,
        offsetof(TimestampsLayout, lightingPassEnd) / TimestampsLayout::MEMBER_SIZE);

    // Resolve pass

    wgpuCommandEncoderWriteTimestamp(
        encoder,
        mQuerySet,
        offsetof(TimestampsLayout, resolvePassStart) / TimestampsLayout::MEMBER_SIZE);
    {
        mResolvePass.render(
            gpuContext,
            encoder,
            renderDesc.targetTextureView,
            framebufferSize,
            renderDesc.exposure,
            frameCount,
            gui);
    }
    wgpuCommandEncoderWriteTimestamp(
        encoder,
        mQuerySet,
        offsetof(TimestampsLayout, resolvePassEnd) / TimestampsLayout::MEMBER_SIZE);

    // Resolve timestamp queries

    wgpuCommandEncoderResolveQuerySet(
        encoder, mQuerySet, 0, TimestampsLayout::QUERY_COUNT, mQueryBuffer.ptr(), 0);
    wgpuCommandEncoderCopyBufferToBuffer(
        encoder, mQueryBuffer.ptr(), 0, mTimestampsBuffer.ptr(), 0, sizeof(TimestampsLayout));

    const WGPUCommandBuffer cmdBuffer = [encoder]() {
        const WGPUCommandBufferDescriptor cmdBufferDesc{
            .nextInChain = nullptr,
            .label = "DeferredRenderer command buffer",
        };
        return wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    }();
    wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);

    wgpuBufferMapAsync(
        mTimestampsBuffer.ptr(),
        WGPUMapMode_Read,
        0,
        sizeof(TimestampsLayout),
        [](const WGPUBufferMapAsyncStatus status, void* const userdata) -> void {
            if (status == WGPUBufferMapAsyncStatus_Success)
            {
                NLRS_ASSERT(userdata != nullptr);
                DeferredRenderer& renderer = *static_cast<DeferredRenderer*>(userdata);
                GpuBuffer&        timestampBuffer = renderer.mTimestampsBuffer;
                const void* const bufferData = wgpuBufferGetConstMappedRange(
                    timestampBuffer.ptr(), 0, sizeof(TimestampsLayout));
                NLRS_ASSERT(bufferData != nullptr);

                const TimestampsLayout& timestamps =
                    *static_cast<const TimestampsLayout*>(bufferData);

                auto&      gbufferDurations = renderer.mGbufferPassDurationsNs;
                const auto gbufferDuration =
                    timestamps.gbufferPassEnd - timestamps.gbufferPassStart;
                gbufferDurations.push_back(gbufferDuration);
                if (gbufferDurations.size() > 30)
                {
                    gbufferDurations.pop_front();
                }

                auto&      lightingDurations = renderer.mLightingPassDurationsNs;
                const auto lightingDuration =
                    timestamps.lightingPassEnd - timestamps.lightingPassStart;
                lightingDurations.push_back(lightingDuration);
                if (lightingDurations.size() > 30)
                {
                    lightingDurations.pop_front();
                }

                auto&      resolveDurations = renderer.mResolvePassDurationsNs;
                const auto resolveDuration =
                    timestamps.resolvePassEnd - timestamps.resolvePassStart;
                resolveDurations.push_back(resolveDuration);
                if (resolveDurations.size() > 30)
                {
                    resolveDurations.pop_front();
                }

                wgpuBufferUnmap(timestampBuffer.ptr());
            }
            else
            {
                std::fprintf(stderr, "Failed to map timestamps buffer\n");
            }
        },
        this);
}

void DeferredRenderer::renderDebug(
    const GpuContext&     gpuContext,
    const glm::mat4&      viewProjectionMat,
    const Extent2f&       framebufferSize,
    const WGPUTextureView textureView,
    Gui&                  gui)
{
    wgpuDeviceTick(gpuContext.device);

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    const std::uint32_t frameCount = mFrameCount++;
    // const glm::mat4 jitterMat = jitterMatrix(framebufferSize, mFrameCount);

    mGbufferPass.render(
        gpuContext,
        encoder,
        mDepthTextureView,
        mAlbedoTextureView,
        mNormalTextureView,
        mVelocityTextureView,
        viewProjectionMat,
        frameCount);

    mDebugPass.render(gpuContext, encoder, textureView, framebufferSize, gui);

    const WGPUCommandBuffer cmdBuffer = [encoder]() {
        const WGPUCommandBufferDescriptor cmdBufferDesc{
            .nextInChain = nullptr,
            .label = "DeferredRenderer command buffer",
        };
        return wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    }();
    wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);
}

void DeferredRenderer::resize(const GpuContext& gpuContext, const Extent2u& newSize)
{
    NLRS_ASSERT(newSize.x > 0 && newSize.y > 0);

    textureViewSafeRelease(mNormalTextureView);
    textureSafeRelease(mNormalTexture);
    textureViewSafeRelease(mAlbedoTextureView);
    textureSafeRelease(mAlbedoTexture);
    textureViewSafeRelease(mDepthTextureView);
    textureSafeRelease(mDepthTexture);
    textureViewSafeRelease(mVelocityTextureView);
    textureSafeRelease(mVelocityTexture);

    mNormalTextureView = nullptr;
    mNormalTexture = nullptr;
    mAlbedoTextureView = nullptr;
    mAlbedoTexture = nullptr;
    mDepthTextureView = nullptr;
    mDepthTexture = nullptr;
    mVelocityTextureView = nullptr;
    mVelocityTexture = nullptr;

    {
        const std::array<WGPUTextureFormat, 1> depthFormats{
            DEPTH_TEXTURE_FORMAT,
        };
        const WGPUTextureDescriptor depthTextureDesc{
            .nextInChain = nullptr,
            .label = "Depth texture",
            .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
            .dimension = WGPUTextureDimension_2D,
            .size = {newSize.x, newSize.y, 1},
            .format = DEPTH_TEXTURE_FORMAT,
            .mipLevelCount = 1,
            .sampleCount = 1,
            .viewFormatCount = depthFormats.size(),
            .viewFormats = depthFormats.data(),
        };
        mDepthTexture = wgpuDeviceCreateTexture(gpuContext.device, &depthTextureDesc);
        NLRS_ASSERT(mDepthTexture != nullptr);

        const WGPUTextureViewDescriptor depthTextureViewDesc{
            .nextInChain = nullptr,
            .label = "Depth texture view",
            .format = DEPTH_TEXTURE_FORMAT,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_DepthOnly,
        };
        mDepthTextureView = wgpuTextureCreateView(mDepthTexture, &depthTextureViewDesc);
        NLRS_ASSERT(mDepthTextureView != nullptr);
    }

    mAlbedoTexture = createGbufferTexture(
        gpuContext.device,
        "Gbuffer albedo texture",
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
        newSize,
        ALBEDO_TEXTURE_FORMAT);
    NLRS_ASSERT(mAlbedoTexture != nullptr);
    mAlbedoTextureView = createGbufferTextureView(
        mAlbedoTexture, "Gbuffer albedo texture view", ALBEDO_TEXTURE_FORMAT);
    NLRS_ASSERT(mAlbedoTextureView != nullptr);

    mNormalTexture = createGbufferTexture(
        gpuContext.device,
        "Gbuffer normal texture",
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
        newSize,
        NORMAL_TEXTURE_FORMAT);
    NLRS_ASSERT(mNormalTexture != nullptr);
    mNormalTextureView = createGbufferTextureView(
        mNormalTexture, "Gbuffer normal texture view", NORMAL_TEXTURE_FORMAT);
    NLRS_ASSERT(mNormalTextureView != nullptr);

    mVelocityTexture = createGbufferTexture(
        gpuContext.device,
        "Gbuffer velocity texture",
        WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding,
        newSize,
        VELOCITY_TEXTURE_FORMAT);
    NLRS_ASSERT(mVelocityTexture != nullptr);
    mVelocityTextureView = createGbufferTextureView(
        mVelocityTexture, "Gbuffer velocity texture view", VELOCITY_TEXTURE_FORMAT);
    NLRS_ASSERT(mVelocityTextureView != nullptr);

    mDebugPass.resize(
        gpuContext,
        mAlbedoTextureView,
        mNormalTextureView,
        mDepthTextureView,
        mVelocityTextureView);
    mLightingPass.resize(gpuContext, mAlbedoTextureView, mNormalTextureView, mDepthTextureView);
    mResolvePass.resize(gpuContext, mSampleBuffer, mVelocityTextureView);

    invalidateTemporalAccumulation();
}

DeferredRenderer::GbufferPass::GbufferPass(
    const GpuContext&                 gpuContext,
    const DeferredRendererDescriptor& rendererDesc)
    : mPositionBuffers([&gpuContext, &rendererDesc]() -> std::vector<GpuBuffer> {
          std::vector<GpuBuffer> buffers;
          std::transform(
              rendererDesc.modelPositions.begin(),
              rendererDesc.modelPositions.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const glm::vec4> vertices) -> GpuBuffer {
                  return GpuBuffer(
                      gpuContext.device, "Mesh Vertex Buffer", GpuBufferUsage::Vertex, vertices);
              });
          return buffers;
      }()),
      mNormalBuffers([&gpuContext, &rendererDesc]() -> std::vector<GpuBuffer> {
          std::vector<GpuBuffer> buffers;
          std::transform(
              rendererDesc.modelNormals.begin(),
              rendererDesc.modelNormals.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const glm::vec4> normals) -> GpuBuffer {
                  return GpuBuffer{
                      gpuContext.device, "Mesh normal buffer", GpuBufferUsage::Vertex, normals};
              });
          return buffers;
      }()),
      mTexCoordBuffers([&gpuContext, &rendererDesc]() -> std::vector<GpuBuffer> {
          std::vector<GpuBuffer> buffers;
          std::transform(
              rendererDesc.modelTexCoords.begin(),
              rendererDesc.modelTexCoords.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const glm::vec2> texCoords) {
                  return GpuBuffer(
                      gpuContext.device, "Mesh TexCoord Buffer", GpuBufferUsage::Vertex, texCoords);
              });
          return buffers;
      }()),
      mIndexBuffers([&gpuContext, &rendererDesc]() -> std::vector<IndexBuffer> {
          std::vector<IndexBuffer> buffers;
          std::transform(
              rendererDesc.modelIndices.begin(),
              rendererDesc.modelIndices.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const std::uint32_t> indices) {
                  return IndexBuffer{
                      .buffer = GpuBuffer(
                          gpuContext.device, "Mesh Index Buffer", GpuBufferUsage::Index, indices),
                      .count = static_cast<std::uint32_t>(indices.size()),
                      .format = WGPUIndexFormat_Uint32,
                  };
              });
          return buffers;
      }()),
      mBaseColorTextureIndices([&rendererDesc]() -> std::vector<std::size_t> {
          std::vector<std::size_t> indices;
          std::transform(
              rendererDesc.modelBaseColorTextureIndices.begin(),
              rendererDesc.modelBaseColorTextureIndices.end(),
              std::back_inserter(indices),
              [](const std::uint32_t index) { return static_cast<std::size_t>(index); });
          return indices;
      }()),
      mBaseColorTextures([&gpuContext, &rendererDesc]() -> std::vector<GpuTexture> {
          std::vector<GpuTexture> textures;
          std::transform(
              rendererDesc.sceneBaseColorTextures.begin(),
              rendererDesc.sceneBaseColorTextures.end(),
              std::back_inserter(textures),
              [&gpuContext](const Texture& texture) -> GpuTexture {
                  const auto                  dimensions = texture.dimensions();
                  const WGPUTextureFormat     TEXTURE_FORMAT = WGPUTextureFormat_BGRA8Unorm;
                  const WGPUTextureDescriptor textureDesc{
                      .nextInChain = nullptr,
                      .label = "Mesh texture",
                      .usage = WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding,
                      .dimension = WGPUTextureDimension_2D,
                      .size = {dimensions.width, dimensions.height, 1},
                      .format = TEXTURE_FORMAT,
                      .mipLevelCount = 1,
                      .sampleCount = 1,
                      .viewFormatCount = 1,
                      .viewFormats = &TEXTURE_FORMAT,
                  };
                  const WGPUTexture gpuTexture =
                      wgpuDeviceCreateTexture(gpuContext.device, &textureDesc);

                  const WGPUTextureViewDescriptor viewDesc{
                      .nextInChain = nullptr,
                      .label = "Mesh texture view",
                      .format = TEXTURE_FORMAT,
                      .dimension = WGPUTextureViewDimension_2D,
                      .baseMipLevel = 0,
                      .mipLevelCount = 1,
                      .baseArrayLayer = 0,
                      .arrayLayerCount = 1,
                      .aspect = WGPUTextureAspect_All,
                  };
                  const WGPUTextureView view = wgpuTextureCreateView(gpuTexture, &viewDesc);

                  const WGPUImageCopyTexture imageDestination{
                      .nextInChain = nullptr,
                      .texture = gpuTexture,
                      .mipLevel = 0,
                      .origin = {0, 0, 0},
                      .aspect = WGPUTextureAspect_All,
                  };
                  const WGPUTextureDataLayout sourceDataLayout{
                      .nextInChain = nullptr,
                      .offset = 0,
                      .bytesPerRow =
                          static_cast<std::uint32_t>(dimensions.width * sizeof(Texture::BgraPixel)),
                      .rowsPerImage = dimensions.height,
                  };
                  const WGPUExtent3D writeSize{
                      .width = dimensions.width,
                      .height = dimensions.height,
                      .depthOrArrayLayers = 1};
                  const std::size_t numPixelBytes = sizeof(Texture::BgraPixel);
                  const std::size_t numTextureBytes = texture.pixels().size() * numPixelBytes;
                  wgpuQueueWriteTexture(
                      gpuContext.queue,
                      &imageDestination,
                      texture.pixels().data(),
                      numTextureBytes,
                      &sourceDataLayout,
                      &writeSize);

                  return GpuTexture{
                      .texture = gpuTexture,
                      .view = view,
                  };
              });
          return textures;
      }()),
      mBaseColorTextureBindGroups(),
      mBaseColorSampler(nullptr),
      mUniformBuffer(
          gpuContext.device,
          "Uniform buffer",
          {GpuBufferUsage::Uniform, GpuBufferUsage::CopyDst},
          sizeof(Uniforms)),
      mUniformBindGroup(),
      mSamplerBindGroup(),
      mPipeline(nullptr),
      mPreviousViewReverseZProjectionMat{glm::mat4(1.0f)}
{
    const GpuBindGroupLayout uniformBindGroupLayout{
        gpuContext.device,
        "Uniform bind group layout",
        mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Vertex, sizeof(Uniforms))};

    mUniformBindGroup = GpuBindGroup{
        gpuContext.device,
        "Uniform bind group",
        uniformBindGroupLayout.ptr(),
        mUniformBuffer.bindGroupEntry(0)};

    {
        const WGPUSamplerDescriptor samplerDesc{
            .nextInChain = nullptr,
            .label = "Model texture sampler",
            .addressModeU = WGPUAddressMode_Repeat,
            .addressModeV = WGPUAddressMode_Repeat,
            .addressModeW = WGPUAddressMode_Repeat,
            .magFilter = WGPUFilterMode_Linear,
            .minFilter = WGPUFilterMode_Linear,
            .mipmapFilter = WGPUMipmapFilterMode_Linear,
            .lodMinClamp = 0.f,
            .lodMaxClamp = 32.f,
            .compare = WGPUCompareFunction_Undefined,
            .maxAnisotropy = 1,
        };
        mBaseColorSampler = wgpuDeviceCreateSampler(gpuContext.device, &samplerDesc);
        NLRS_ASSERT(mBaseColorSampler != nullptr);
    }

    const GpuBindGroupLayout samplerBindGroupLayout{
        gpuContext.device,
        "Sampler bind group layout",
        samplerBindGroupLayoutEntry(0, WGPUSamplerBindingType_Filtering)};

    mSamplerBindGroup = GpuBindGroup{
        gpuContext.device,
        "Sampler bind group",
        samplerBindGroupLayout.ptr(),
        samplerBindGroupEntry(0, mBaseColorSampler)};

    const GpuBindGroupLayout textureBindGroupLayout{
        gpuContext.device,
        "Texture bind group layout",
        textureBindGroupLayoutEntry(0, WGPUTextureSampleType_Float, WGPUShaderStage_Fragment)};

    std::transform(
        mBaseColorTextures.begin(),
        mBaseColorTextures.end(),
        std::back_inserter(mBaseColorTextureBindGroups),
        [&gpuContext, &textureBindGroupLayout](const GpuTexture& texture) -> GpuBindGroup {
            return GpuBindGroup{
                gpuContext.device,
                "Texture bind group",
                textureBindGroupLayout.ptr(),
                textureBindGroupEntry(0, texture.view)};
        });

    {
        // Pipeline layout

        const std::array<WGPUBindGroupLayout, 3> bindGroupLayouts{
            uniformBindGroupLayout.ptr(),
            samplerBindGroupLayout.ptr(),
            textureBindGroupLayout.ptr(),
        };

        const WGPUPipelineLayoutDescriptor pipelineLayoutDesc{
            .nextInChain = nullptr,
            .label = "Pipeline layout",
            .bindGroupLayoutCount = bindGroupLayouts.size(),
            .bindGroupLayouts = bindGroupLayouts.data(),
        };

        const WGPUPipelineLayout pipelineLayout =
            wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);

        // Vertex layout

        const WGPUVertexAttribute positionAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = 0,
            .shaderLocation = 0,
        };

        const WGPUVertexBufferLayout positionBufferLayout{
            .arrayStride = sizeof(glm::vec4),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &positionAttribute,
        };

        const WGPUVertexAttribute normalAttribute{
            .format = WGPUVertexFormat_Float32x4,
            .offset = 0,
            .shaderLocation = 1,
        };

        const WGPUVertexBufferLayout normalBufferLayout{
            .arrayStride = sizeof(glm::vec4),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &normalAttribute,
        };

        const WGPUVertexAttribute texCoordAttribute{
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
            .shaderLocation = 2,
        };

        const WGPUVertexBufferLayout texCoordBufferLayout{
            .arrayStride = sizeof(glm::vec2),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &texCoordAttribute,
        };

        const std::array<WGPUVertexBufferLayout, 3> vertexBufferLayouts{
            positionBufferLayout,
            normalBufferLayout,
            texCoordBufferLayout,
        };

        // Depth stencil state

        const WGPUDepthStencilState depthStencilState{
            .nextInChain = nullptr,
            .format = DEPTH_TEXTURE_FORMAT,
            .depthWriteEnabled = true,
            .depthCompare = WGPUCompareFunction_Greater,
            .stencilFront = DEFAULT_STENCIL_FACE_STATE,
            .stencilBack = DEFAULT_STENCIL_FACE_STATE,
            .stencilReadMask = 0, // stencil masks deactivated by setting to zero
            .stencilWriteMask = 0,
            .depthBias = 0,
            .depthBiasSlopeScale = 0,
            .depthBiasClamp = 0,
        };

        // Fragment state

        const WGPUShaderModule shaderModule = [&gpuContext]() -> WGPUShaderModule {
            const WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
                .chain =
                    WGPUChainedStruct{
                        .next = nullptr,
                        .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                    },
                .code = DEFERRED_RENDERER_GBUFFER_PASS_SOURCE,
            };

            const WGPUShaderModuleDescriptor shaderDesc{
                .nextInChain = &shaderCodeDesc.chain,
                .label = "Hybrid renderer gbuffer pass shader",
            };

            return wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);
        }();

        const WGPUBlendState blendState{
            .color =
                WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_One,
                    .dstFactor = WGPUBlendFactor_Zero,
                },
            .alpha =
                WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_One,
                    .dstFactor = WGPUBlendFactor_Zero,
                },
        };

        const WGPUColorTargetState colorTargets[] = {
            {
                .nextInChain = nullptr,
                .format = ALBEDO_TEXTURE_FORMAT,
                .blend = &blendState,
                .writeMask = WGPUColorWriteMask_All,
            },
            {
                .nextInChain = nullptr,
                .format = NORMAL_TEXTURE_FORMAT,
                .blend = &blendState,
                .writeMask = WGPUColorWriteMask_All,
            },
            {
                .nextInChain = nullptr,
                .format = VELOCITY_TEXTURE_FORMAT,
                .blend = &blendState,
                .writeMask = WGPUColorWriteMask_All,
            }};

        const WGPUFragmentState fragmentState{
            .nextInChain = nullptr,
            .module = shaderModule,
            .entryPoint = "fsMain",
            .constantCount = 0,
            .constants = nullptr,
            .targetCount = std::size(colorTargets),
            .targets = colorTargets,
        };

        // Pipeline

        const WGPURenderPipelineDescriptor pipelineDesc{
            .nextInChain = nullptr,
            .label = "Render pipeline",
            .layout = pipelineLayout,
            .vertex =
                WGPUVertexState{
                    .nextInChain = nullptr,
                    .module = shaderModule,
                    .entryPoint = "vsMain",
                    .constantCount = 0,
                    .constants = nullptr,
                    .bufferCount = vertexBufferLayouts.size(),
                    .buffers = vertexBufferLayouts.data(),
                },
            .primitive =
                WGPUPrimitiveState{
                    .nextInChain = nullptr,
                    .topology = WGPUPrimitiveTopology_TriangleList,
                    .stripIndexFormat = WGPUIndexFormat_Undefined,
                    .frontFace = WGPUFrontFace_CCW,
                    .cullMode = WGPUCullMode_Back,
                },
            .depthStencil = &depthStencilState,
            .multisample =
                WGPUMultisampleState{
                    .nextInChain = nullptr,
                    .count = 1,
                    .mask = ~0u,
                    .alphaToCoverageEnabled = false,
                },
            .fragment = &fragmentState,
        };

        mPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

        wgpuPipelineLayoutRelease(pipelineLayout);
    }

    NLRS_ASSERT(mPositionBuffers.size() == mIndexBuffers.size());
    NLRS_ASSERT(mPositionBuffers.size() == mTexCoordBuffers.size());
    NLRS_ASSERT(mPositionBuffers.size() == mBaseColorTextureIndices.size());
}

DeferredRenderer::GbufferPass::~GbufferPass()
{
    renderPipelineSafeRelease(mPipeline);
    mPipeline = nullptr;
    samplerSafeRelease(mBaseColorSampler);
    mBaseColorSampler = nullptr;
    for (const auto& texture : mBaseColorTextures)
    {
        textureSafeRelease(texture.texture);
        textureViewSafeRelease(texture.view);
    }
    mBaseColorTextures.clear();
}

DeferredRenderer::GbufferPass::GbufferPass(GbufferPass&& other) noexcept
{
    if (this != &other)
    {
        mPositionBuffers = std::move(other.mPositionBuffers);
        mNormalBuffers = std::move(other.mNormalBuffers);
        mTexCoordBuffers = std::move(other.mTexCoordBuffers);
        mIndexBuffers = std::move(other.mIndexBuffers);
        mBaseColorTextureIndices = std::move(other.mBaseColorTextureIndices);
        mBaseColorTextures = std::move(other.mBaseColorTextures);
        mBaseColorTextureBindGroups = std::move(other.mBaseColorTextureBindGroups);
        mBaseColorSampler = other.mBaseColorSampler;
        other.mBaseColorSampler = nullptr;
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mSamplerBindGroup = std::move(other.mSamplerBindGroup);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
        mPreviousViewReverseZProjectionMat = other.mPreviousViewReverseZProjectionMat;
    }
}

DeferredRenderer::GbufferPass& DeferredRenderer::GbufferPass::operator=(
    GbufferPass&& other) noexcept
{
    if (this != &other)
    {
        mPositionBuffers = std::move(other.mPositionBuffers);
        mNormalBuffers = std::move(other.mNormalBuffers);
        mTexCoordBuffers = std::move(other.mTexCoordBuffers);
        mIndexBuffers = std::move(other.mIndexBuffers);
        mBaseColorTextureIndices = std::move(other.mBaseColorTextureIndices);
        mBaseColorTextures = std::move(other.mBaseColorTextures);
        mBaseColorTextureBindGroups = std::move(other.mBaseColorTextureBindGroups);
        mBaseColorSampler = other.mBaseColorSampler;
        other.mBaseColorSampler = nullptr;
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mSamplerBindGroup = std::move(other.mSamplerBindGroup);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
        mPreviousViewReverseZProjectionMat = other.mPreviousViewReverseZProjectionMat;
    }
    return *this;
}

void DeferredRenderer::GbufferPass::render(
    const GpuContext&        gpuContext,
    const WGPUCommandEncoder cmdEncoder,
    const WGPUTextureView    depthTextureView,
    const WGPUTextureView    albedoTextureView,
    const WGPUTextureView    normalTextureView,
    const WGPUTextureView    velocityTextureView,
    const glm::mat4&         viewReverseZProjectionMatrix,
    const std::uint32_t      frameCount)
{
    const glm::mat4& previousViewReverseZProjectionMat =
        frameCount == 0 ? viewReverseZProjectionMatrix : mPreviousViewReverseZProjectionMat;
    const Uniforms uniforms{viewReverseZProjectionMatrix, previousViewReverseZProjectionMat};
    wgpuQueueWriteBuffer(gpuContext.queue, mUniformBuffer.ptr(), 0, &uniforms, sizeof(Uniforms));

    mPreviousViewReverseZProjectionMat = viewReverseZProjectionMatrix;

    const WGPURenderPassEncoder renderPassEncoder =
        [cmdEncoder, depthTextureView, albedoTextureView, normalTextureView, velocityTextureView]()
        -> WGPURenderPassEncoder {
        const WGPURenderPassColorAttachment colorAttachments[] = {
            WGPURenderPassColorAttachment{
                .nextInChain = nullptr,
                .view = albedoTextureView,
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .resolveTarget = nullptr,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                .clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0},
            },
            WGPURenderPassColorAttachment{
                .nextInChain = nullptr,
                .view = normalTextureView,
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .resolveTarget = nullptr,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                .clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0}},
            WGPURenderPassColorAttachment{
                .nextInChain = nullptr,
                .view = velocityTextureView,
                .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
                .resolveTarget = nullptr,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                .clearValue = WGPUColor{0.0, 0.0, 0.0, 0.0}}};

        const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
            .view = depthTextureView,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 0.0f,
            .depthReadOnly = false,
            .stencilLoadOp = WGPULoadOp_Undefined,
            .stencilStoreOp = WGPUStoreOp_Undefined,
            .stencilClearValue = 0,
            .stencilReadOnly = true,
        };

        const WGPURenderPassDescriptor renderPassDesc = {
            .nextInChain = nullptr,
            .label = "Render pass encoder",
            .colorAttachmentCount = std::size(colorAttachments),
            .colorAttachments = colorAttachments,
            .depthStencilAttachment = &depthStencilAttachment,
            .occlusionQuerySet = nullptr,
            .timestampWrites = nullptr,
        };

        return wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);
    }();

    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mPipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, mUniformBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, mSamplerBindGroup.ptr(), 0, nullptr);

    std::size_t currentTextureIdx = ~static_cast<std::size_t>(0);
    for (std::size_t idx = 0; idx < mPositionBuffers.size(); ++idx)
    {
        const GpuBuffer& positionBuffer = mPositionBuffers[idx];
        const GpuBuffer& normalBuffer = mNormalBuffers[idx];
        const GpuBuffer& texCoordBuffer = mTexCoordBuffers[idx];
        wgpuRenderPassEncoderSetVertexBuffer(
            renderPassEncoder, 0, positionBuffer.ptr(), 0, positionBuffer.byteSize());
        wgpuRenderPassEncoderSetVertexBuffer(
            renderPassEncoder, 1, normalBuffer.ptr(), 0, normalBuffer.byteSize());
        wgpuRenderPassEncoderSetVertexBuffer(
            renderPassEncoder, 2, texCoordBuffer.ptr(), 0, texCoordBuffer.byteSize());

        const IndexBuffer& indexBuffer = mIndexBuffers[idx];
        wgpuRenderPassEncoderSetIndexBuffer(
            renderPassEncoder,
            indexBuffer.buffer.ptr(),
            indexBuffer.format,
            0,
            indexBuffer.buffer.byteSize());

        const std::size_t textureIdx = mBaseColorTextureIndices[idx];
        if (textureIdx != currentTextureIdx)
        {
            currentTextureIdx = textureIdx;
            const GpuBindGroup& baseColorBindGroup = mBaseColorTextureBindGroups[textureIdx];
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 2, baseColorBindGroup.ptr(), 0, nullptr);
        }

        wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexBuffer.count, 1, 0, 0, 0);
    }

    wgpuRenderPassEncoderEnd(renderPassEncoder);
}

DeferredRenderer::DebugPass::DebugPass(
    const GpuContext&     gpuContext,
    const WGPUTextureView gbufferAlbedoTextureView,
    const WGPUTextureView gbufferNormalTextureView,
    const WGPUTextureView gbufferDepthTextureView,
    const WGPUTextureView gbufferVelocityTextureView,
    const Extent2u&       framebufferSize)
    : mVertexBuffer{gpuContext.device, "Vertex buffer", {GpuBufferUsage::Vertex, GpuBufferUsage::CopyDst}, std::span<const float[2]>(quadVertexData)},
      mUniformBuffer{},
      mUniformBindGroup{},
      mGbufferBindGroupLayout{},
      mGbufferBindGroup{},
      mPipeline(nullptr)
{
    {
        const auto uniformData = Extent2f{framebufferSize};
        mUniformBuffer = GpuBuffer{
            gpuContext.device,
            "Uniform buffer",
            {GpuBufferUsage::Uniform, GpuBufferUsage::CopyDst},
            std::span<const float>(&uniformData.x, sizeof(Extent2f))};
    }

    const GpuBindGroupLayout uniformBindGroupLayout{
        gpuContext.device,
        "Uniform bind group layout",
        mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment, sizeof(Extent2f))};

    mUniformBindGroup = GpuBindGroup{
        gpuContext.device,
        "Uniform bind group",
        uniformBindGroupLayout.ptr(),
        mUniformBuffer.bindGroupEntry(0)};

    mGbufferBindGroupLayout = GpuBindGroupLayout{
        gpuContext.device,
        "Debug pass gbuffer bind group layout",
        std::array<WGPUBindGroupLayoutEntry, 4>{
            textureBindGroupLayoutEntry(
                0, WGPUTextureSampleType_UnfilterableFloat, WGPUShaderStage_Fragment),
            textureBindGroupLayoutEntry(
                1, WGPUTextureSampleType_UnfilterableFloat, WGPUShaderStage_Fragment),
            textureBindGroupLayoutEntry(2, WGPUTextureSampleType_Depth, WGPUShaderStage_Fragment),
            textureBindGroupLayoutEntry(
                3, WGPUTextureSampleType_UnfilterableFloat, WGPUShaderStage_Fragment)}};

    mGbufferBindGroup = GpuBindGroup{
        gpuContext.device,
        "Debug pass gbuffer bind group",
        mGbufferBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 4>{
            textureBindGroupEntry(0, gbufferAlbedoTextureView),
            textureBindGroupEntry(1, gbufferNormalTextureView),
            textureBindGroupEntry(2, gbufferDepthTextureView),
            textureBindGroupEntry(3, gbufferVelocityTextureView)}};

    {
        // Pipeline layout

        const std::array<WGPUBindGroupLayout, 2> bindGroupLayouts{
            uniformBindGroupLayout.ptr(), mGbufferBindGroupLayout.ptr()};

        const WGPUPipelineLayoutDescriptor pipelineLayoutDesc{
            .nextInChain = nullptr,
            .label = "Debug pass pipeline layout",
            .bindGroupLayoutCount = bindGroupLayouts.size(),
            .bindGroupLayouts = bindGroupLayouts.data(),
        };

        const WGPUPipelineLayout pipelineLayout =
            wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);

        // Vertex layout

        const std::array<WGPUVertexAttribute, 1> vertexAttributes{WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
            .shaderLocation = 0,
        }};

        const WGPUVertexBufferLayout vertexBufferLayout{
            .arrayStride = sizeof(float[2]),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = vertexAttributes.size(),
            .attributes = vertexAttributes.data(),
        };

        // Shader module

        const WGPUShaderModule shaderModule = [&gpuContext]() -> WGPUShaderModule {
            const WGPUShaderModuleWGSLDescriptor wgslDesc = {
                .chain =
                    WGPUChainedStruct{
                        .next = nullptr,
                        .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                    },
                .code = DEFERRED_RENDERER_DEBUG_PASS_SOURCE,
            };

            const WGPUShaderModuleDescriptor moduleDesc{
                .nextInChain = &wgslDesc.chain,
                .label = "Debug pass shader",
            };

            return wgpuDeviceCreateShaderModule(gpuContext.device, &moduleDesc);
        }();
        NLRS_ASSERT(shaderModule != nullptr);

        // Fragment state

        const WGPUBlendState blendState{
            .color =
                WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_One,
                    .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                },
            .alpha =
                WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_Zero,
                    .dstFactor = WGPUBlendFactor_One,
                },
        };

        const std::array<WGPUColorTargetState, 1> colorTargets{WGPUColorTargetState{
            .nextInChain = nullptr,
            .format = Window::SWAP_CHAIN_FORMAT,
            .blend = &blendState,
            .writeMask = WGPUColorWriteMask_All}};

        const WGPUFragmentState fragmentState{
            .nextInChain = nullptr,
            .module = shaderModule,
            .entryPoint = "fsMain",
            .constantCount = 0,
            .constants = nullptr,
            .targetCount = colorTargets.size(),
            .targets = colorTargets.data(),
        };

        // Pipeline

        const WGPURenderPipelineDescriptor pipelineDesc{
            .nextInChain = nullptr,
            .label = "Debug pass render pipeline",
            .layout = pipelineLayout,
            .vertex =
                WGPUVertexState{
                    .nextInChain = nullptr,
                    .module = shaderModule,
                    .entryPoint = "vsMain",
                    .constantCount = 0,
                    .constants = nullptr,
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            .primitive =
                WGPUPrimitiveState{
                    .nextInChain = nullptr,
                    .topology = WGPUPrimitiveTopology_TriangleList,
                    .stripIndexFormat = WGPUIndexFormat_Undefined,
                    .frontFace = WGPUFrontFace_CCW,
                    .cullMode = WGPUCullMode_Back,
                },
            .depthStencil = nullptr,
            .multisample =
                WGPUMultisampleState{
                    .nextInChain = nullptr,
                    .count = 1,
                    .mask = ~0u,
                    .alphaToCoverageEnabled = false,
                },
            .fragment = &fragmentState,
        };

        mPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

        wgpuPipelineLayoutRelease(pipelineLayout);
    }
}

DeferredRenderer::DebugPass::~DebugPass()
{
    renderPipelineSafeRelease(mPipeline);
    mPipeline = nullptr;
}

DeferredRenderer::DebugPass::DebugPass(DebugPass&& other) noexcept
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mGbufferBindGroupLayout = std::move(other.mGbufferBindGroupLayout);
        mGbufferBindGroup = std::move(other.mGbufferBindGroup);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
}

DeferredRenderer::DebugPass& DeferredRenderer::DebugPass::operator=(DebugPass&& other) noexcept
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mGbufferBindGroupLayout = std::move(other.mGbufferBindGroupLayout);
        mGbufferBindGroup = std::move(other.mGbufferBindGroup);
        renderPipelineSafeRelease(mPipeline);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
    return *this;
}

void DeferredRenderer::DebugPass::render(
    const GpuContext&        gpuContext,
    const WGPUCommandEncoder cmdEncoder,
    const WGPUTextureView    textureView,
    const Extent2f&          framebufferSize,
    Gui&                     gui)
{
    wgpuQueueWriteBuffer(
        gpuContext.queue, mUniformBuffer.ptr(), 0, &framebufferSize.x, sizeof(Extent2f));

    const WGPURenderPassEncoder renderPass = [cmdEncoder, textureView]() -> WGPURenderPassEncoder {
        const WGPURenderPassColorAttachment colorAttachment{
            .nextInChain = nullptr,
            .view = textureView,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0},
        };

        const WGPURenderPassDescriptor renderPassDesc{
            .nextInChain = nullptr,
            .label = "Debug pass render pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
            .depthStencilAttachment = nullptr,
            .occlusionQuerySet = nullptr,
            .timestampWrites = nullptr,
        };

        return wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);
    }();
    NLRS_ASSERT(renderPass != nullptr);

    wgpuRenderPassEncoderSetPipeline(renderPass, mPipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, mUniformBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 1, mGbufferBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(
        renderPass, 0, mVertexBuffer.ptr(), 0, mVertexBuffer.byteSize());
    wgpuRenderPassEncoderDraw(renderPass, 6, 1, 0, 0);

    gui.render(renderPass);

    wgpuRenderPassEncoderEnd(renderPass);
}

void DeferredRenderer::DebugPass::resize(
    const GpuContext&     gpuContext,
    const WGPUTextureView albedoTextureView,
    const WGPUTextureView normalTextureView,
    const WGPUTextureView depthTextureView,
    const WGPUTextureView velocityTextureView)
{
    mGbufferBindGroup = GpuBindGroup{
        gpuContext.device,
        "Debug pass gbuffer bind group",
        mGbufferBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 4>{
            textureBindGroupEntry(0, albedoTextureView),
            textureBindGroupEntry(1, normalTextureView),
            textureBindGroupEntry(2, depthTextureView),
            textureBindGroupEntry(3, velocityTextureView)}};
}

DeferredRenderer::LightingPass::LightingPass(
    const GpuContext&                  gpuContext,
    const WGPUTextureView              albedoTextureView,
    const WGPUTextureView              normalTextureView,
    const WGPUTextureView              depthTextureView,
    const GpuBuffer&                   sampleBuffer,
    std::span<const BvhNode>           sceneBvhNodes,
    std::span<const PositionAttribute> scenePositionAttributes,
    std::span<const VertexAttributes>  sceneVertexAttributes,
    std::span<const Texture>           sceneBaseColorTextures)
    : mCurrentSky{},
      mSkyStateBuffer{
          gpuContext.device,
          "Sky state buffer",
          {GpuBufferUsage::ReadOnlyStorage, GpuBufferUsage::CopyDst},
          sizeof(AlignedSkyState)},
      mUniformBuffer{
          gpuContext.device,
          "Sky uniform buffer",
          {GpuBufferUsage::Uniform, GpuBufferUsage::CopyDst},
          sizeof(Uniforms)},
      mUniformBindGroup{},
      mGbufferBindGroupLayout{},
      mGbufferBindGroup{},
      mBvhNodeBuffer{
          gpuContext.device,
          "BVH node buffer",
          {GpuBufferUsage::ReadOnlyStorage, GpuBufferUsage::CopyDst},
          std::span<const BvhNode>(sceneBvhNodes)},
      mPositionAttributesBuffer{
          gpuContext.device,
          "Position attribute buffer",
          {GpuBufferUsage::ReadOnlyStorage, GpuBufferUsage::CopyDst},
          std::span<const PositionAttribute>(scenePositionAttributes)},
      mVertexAttributesBuffer{
          gpuContext.device,
          "Vertex attribute buffer",
          {GpuBufferUsage::ReadOnlyStorage, GpuBufferUsage::CopyDst},
          std::span<const VertexAttributes>(sceneVertexAttributes)},
      mTextureDescriptorBuffer{},
      mTextureBuffer{},
      mBlueNoiseBuffer{[&gpuContext]() -> GpuBuffer {
          std::span<const std::uint8_t> blueNoise(blueNoiseValues, sizeof(blueNoiseValues));
          std::vector<std::uint32_t>    bufferData;
          bufferData.reserve(2 + blueNoise.size()); // size + array of values
          bufferData.push_back(static_cast<std::uint32_t>(blueNoiseWidth));
          bufferData.push_back(static_cast<std::uint32_t>(blueNoiseHeight));
          for (const std::uint8_t value : blueNoise)
          {
              const float f = static_cast<float>(value) / 255.0f;
              bufferData.push_back(std::bit_cast<std::uint32_t>(f));
          }
          return GpuBuffer{
              gpuContext.device,
              "blue noise buffer",
              {GpuBufferUsage::ReadOnlyStorage, GpuBufferUsage::CopyDst},
              std::span<const std::uint32_t>(bufferData)};
      }()},
      mBvhBindGroup{},
      mSampleBindGroup{},
      mPipeline(nullptr)
{
    {
        const AlignedSkyState skyState{mCurrentSky};
        wgpuQueueWriteBuffer(
            gpuContext.queue, mSkyStateBuffer.ptr(), 0, &skyState, sizeof(AlignedSkyState));
    }

    const GpuBindGroupLayout uniformBindGroupLayout{
        gpuContext.device,
        "Lighting passs uniform bind group layout",
        mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Compute, sizeof(Uniforms))};

    mUniformBindGroup = GpuBindGroup{
        gpuContext.device,
        "Lighting pass uniform bind group",
        uniformBindGroupLayout.ptr(),
        mUniformBuffer.bindGroupEntry(0)};

    mGbufferBindGroupLayout = GpuBindGroupLayout{
        gpuContext.device,
        "lighting pass gbuffer bind group layout",
        std::array<WGPUBindGroupLayoutEntry, 3>{
            textureBindGroupLayoutEntry(
                0, WGPUTextureSampleType_UnfilterableFloat, WGPUShaderStage_Compute),
            textureBindGroupLayoutEntry(
                1, WGPUTextureSampleType_UnfilterableFloat, WGPUShaderStage_Compute),
            textureBindGroupLayoutEntry(2, WGPUTextureSampleType_Depth, WGPUShaderStage_Compute)}};

    mGbufferBindGroup = GpuBindGroup{
        gpuContext.device,
        "Lighting pass gbuffer bind group",
        mGbufferBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 3>{
            textureBindGroupEntry(0, albedoTextureView),
            textureBindGroupEntry(1, normalTextureView),
            textureBindGroupEntry(2, depthTextureView)}};

    {
        struct TextureDescriptor
        {
            std::uint32_t width, height, offset;
        };
        // Ensure matches layout of `TextureDescriptor` definition in shader.
        std::vector<TextureDescriptor> textureDescriptors;
        textureDescriptors.reserve(sceneBaseColorTextures.size());

        std::vector<Texture::BgraPixel> textureData;
        textureData.reserve((2 << 28) / sizeof(Texture::BgraPixel));

        // Texture descriptors and texture data need to appended in the order of
        // sceneBaseColorTextures. The vertex attribute's `textureIdx` indexes into that array,
        // and we want to use the same indices to index into the texture descriptor array.

        for (const Texture& baseColorTexture : sceneBaseColorTextures)
        {
            const auto dimensions = baseColorTexture.dimensions();
            const auto pixels = baseColorTexture.pixels();

            const std::uint32_t width = dimensions.width;
            const std::uint32_t height = dimensions.height;
            const std::uint32_t offset = static_cast<std::uint32_t>(textureData.size());

            textureData.resize(textureData.size() + pixels.size());
            std::memcpy(
                textureData.data() + offset,
                pixels.data(),
                pixels.size() * sizeof(Texture::BgraPixel));

            textureDescriptors.push_back({width, height, offset});
        }

        mTextureDescriptorBuffer = GpuBuffer(
            gpuContext.device,
            "texture descriptor buffer",
            {GpuBufferUsage::ReadOnlyStorage, GpuBufferUsage::CopyDst},
            std::span<const TextureDescriptor>(textureDescriptors));

        const std::size_t textureDataNumBytes = textureData.size() * sizeof(Texture::BgraPixel);
        const std::size_t maxStorageBufferBindingSize =
            static_cast<std::size_t>(REQUIRED_LIMITS.maxStorageBufferBindingSize);
        if (textureDataNumBytes > maxStorageBufferBindingSize)
        {
            throw std::runtime_error(fmt::format(
                "Texture buffer size ({}) exceeds "
                "maxStorageBufferBindingSize ({}).",
                textureDataNumBytes,
                maxStorageBufferBindingSize));
        }

        mTextureBuffer = GpuBuffer(
            gpuContext.device,
            "texture buffer",
            {GpuBufferUsage::ReadOnlyStorage, GpuBufferUsage::CopyDst},
            std::span<const Texture::BgraPixel>(textureData));
    }

    const GpuBindGroupLayout bvhBindGroupLayout{
        gpuContext.device,
        "Scene bind group layout",
        std::array<WGPUBindGroupLayoutEntry, 7>{
            mSkyStateBuffer.bindGroupLayoutEntry(
                0, WGPUShaderStage_Compute, sizeof(AlignedSkyState)),
            mBvhNodeBuffer.bindGroupLayoutEntry(1, WGPUShaderStage_Compute),
            mPositionAttributesBuffer.bindGroupLayoutEntry(2, WGPUShaderStage_Compute),
            mVertexAttributesBuffer.bindGroupLayoutEntry(3, WGPUShaderStage_Compute),
            mTextureDescriptorBuffer.bindGroupLayoutEntry(4, WGPUShaderStage_Compute),
            mTextureBuffer.bindGroupLayoutEntry(5, WGPUShaderStage_Compute),
            mBlueNoiseBuffer.bindGroupLayoutEntry(6, WGPUShaderStage_Compute),
        }};

    mBvhBindGroup = GpuBindGroup{
        gpuContext.device,
        "Lighting pass BVH bind group",
        bvhBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 7>{
            mSkyStateBuffer.bindGroupEntry(0),
            mBvhNodeBuffer.bindGroupEntry(1),
            mPositionAttributesBuffer.bindGroupEntry(2),
            mVertexAttributesBuffer.bindGroupEntry(3),
            mTextureDescriptorBuffer.bindGroupEntry(4),
            mTextureBuffer.bindGroupEntry(5),
            mBlueNoiseBuffer.bindGroupEntry(6),
        }};

    const GpuBindGroupLayout sampleBindGroupLayout{
        gpuContext.device,
        "Lighting pass accumulation bind group layout",
        std::array<WGPUBindGroupLayoutEntry, 1>{
            sampleBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Compute)}};

    mSampleBindGroup = GpuBindGroup{
        gpuContext.device,
        "Lighting pass accumulation bind group",
        sampleBindGroupLayout.ptr(),
        sampleBuffer.bindGroupEntry(0)};

    {
        // Pipeline layout

        const WGPUShaderModule shaderModule = [&gpuContext]() -> WGPUShaderModule {
            const WGPUShaderModuleWGSLDescriptor wgslDesc = {
                .chain =
                    WGPUChainedStruct{
                        .next = nullptr,
                        .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                    },
                .code = DEFERRED_RENDERER_LIGHTING_PASS_SOURCE,
            };

            const WGPUShaderModuleDescriptor moduleDesc{
                .nextInChain = &wgslDesc.chain,
                .label = "Lighting pass shader",
            };

            return wgpuDeviceCreateShaderModule(gpuContext.device, &moduleDesc);
        }();
        NLRS_ASSERT(shaderModule != nullptr);

        const WGPUBindGroupLayout bindGroupLayouts[] = {
            sampleBindGroupLayout.ptr(),
            uniformBindGroupLayout.ptr(),
            mGbufferBindGroupLayout.ptr(),
            bvhBindGroupLayout.ptr()};

        const WGPUPipelineLayoutDescriptor pipelineLayoutDesc{
            .nextInChain = nullptr,
            .label = "Lighting pass pipeline layout",
            .bindGroupLayoutCount = std::size(bindGroupLayouts),
            .bindGroupLayouts = bindGroupLayouts,
        };

        const WGPUPipelineLayout pipelineLayout =
            wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);

        const WGPUProgrammableStageDescriptor computeStageDesc{
            .nextInChain = nullptr,
            .module = shaderModule,
            .entryPoint = "main",
            .constantCount = 0,
            .constants = nullptr,
        };

        const WGPUComputePipelineDescriptor pipelineDesc{
            .nextInChain = nullptr,
            .label = "Lighting pass compute pipeline",
            .layout = pipelineLayout,
            .compute = computeStageDesc,
        };

        mPipeline = wgpuDeviceCreateComputePipeline(gpuContext.device, &pipelineDesc);
        wgpuPipelineLayoutRelease(pipelineLayout);
    }
}

DeferredRenderer::LightingPass::~LightingPass()
{
    computePipelineSafeRelease(mPipeline);
    mPipeline = nullptr;
}

DeferredRenderer::LightingPass::LightingPass(LightingPass&& other) noexcept
{
    if (this != &other)
    {
        mCurrentSky = other.mCurrentSky;
        mSkyStateBuffer = std::move(other.mSkyStateBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mGbufferBindGroupLayout = std::move(other.mGbufferBindGroupLayout);
        mGbufferBindGroup = std::move(other.mGbufferBindGroup);
        mBvhNodeBuffer = std::move(other.mBvhNodeBuffer);
        mPositionAttributesBuffer = std::move(other.mPositionAttributesBuffer);
        mVertexAttributesBuffer = std::move(other.mVertexAttributesBuffer);
        mTextureDescriptorBuffer = std::move(other.mTextureDescriptorBuffer);
        mTextureBuffer = std::move(other.mTextureBuffer);
        mBlueNoiseBuffer = std::move(other.mBlueNoiseBuffer);
        mBvhBindGroup = std::move(other.mBvhBindGroup);
        mSampleBindGroup = std::move(other.mSampleBindGroup);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
}

DeferredRenderer::LightingPass& DeferredRenderer::LightingPass::operator=(
    LightingPass&& other) noexcept
{
    if (this != &other)
    {
        mCurrentSky = other.mCurrentSky;
        mSkyStateBuffer = std::move(other.mSkyStateBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mGbufferBindGroupLayout = std::move(other.mGbufferBindGroupLayout);
        mGbufferBindGroup = std::move(other.mGbufferBindGroup);
        mBvhNodeBuffer = std::move(other.mBvhNodeBuffer);
        mPositionAttributesBuffer = std::move(other.mPositionAttributesBuffer);
        mVertexAttributesBuffer = std::move(other.mVertexAttributesBuffer);
        mTextureDescriptorBuffer = std::move(other.mTextureDescriptorBuffer);
        mTextureBuffer = std::move(other.mTextureBuffer);
        mBlueNoiseBuffer = std::move(other.mBlueNoiseBuffer);
        mBvhBindGroup = std::move(other.mBvhBindGroup);
        mSampleBindGroup = std::move(other.mSampleBindGroup);
        computePipelineSafeRelease(mPipeline);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
    return *this;
}

void DeferredRenderer::LightingPass::render(
    const GpuContext&        gpuContext,
    const WGPUCommandEncoder cmdEncoder,
    const glm::mat4&         inverseViewReverseZProjectionMatrix,
    const glm::vec3&         cameraPosition,
    const Extent2f&          fbsize,
    const Sky&               sky,
    const std::uint32_t      frameCount)
{
    if (mCurrentSky != sky)
    {
        mCurrentSky = sky;
        const AlignedSkyState skyState{sky};
        wgpuQueueWriteBuffer(
            gpuContext.queue, mSkyStateBuffer.ptr(), 0, &skyState, sizeof(AlignedSkyState));
    }

    {
        const Uniforms uniforms{
            inverseViewReverseZProjectionMatrix,
            glm::vec4(cameraPosition, 1.f),
            glm::vec2(fbsize.x, fbsize.y),
            frameCount,
            0};
        wgpuQueueWriteBuffer(
            gpuContext.queue, mUniformBuffer.ptr(), 0, &uniforms, sizeof(Uniforms));
    }

    const WGPUComputePassEncoder computePass = [cmdEncoder]() -> WGPUComputePassEncoder {
        const WGPUComputePassDescriptor computePassDesc{
            .nextInChain = nullptr,
            .label = "Lighting pass compute pass descriptor",
            .timestampWrites = nullptr,
        };
        return wgpuCommandEncoderBeginComputePass(cmdEncoder, &computePassDesc);
    }();
    NLRS_ASSERT(computePass != nullptr);

    wgpuComputePassEncoderSetPipeline(computePass, mPipeline);
    wgpuComputePassEncoderSetBindGroup(computePass, 0, mSampleBindGroup.ptr(), 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePass, 1, mUniformBindGroup.ptr(), 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePass, 2, mGbufferBindGroup.ptr(), 0, nullptr);
    wgpuComputePassEncoderSetBindGroup(computePass, 3, mBvhBindGroup.ptr(), 0, nullptr);

    const std::uint32_t workgroupCountX = static_cast<std::uint32_t>(0.5f + fbsize.x / 8.f);
    const std::uint32_t workgroupCountY = static_cast<std::uint32_t>(0.5f + fbsize.y / 8.f);
    wgpuComputePassEncoderDispatchWorkgroups(computePass, workgroupCountX, workgroupCountY, 1);

    wgpuComputePassEncoderEnd(computePass);
}

void DeferredRenderer::LightingPass::resize(
    const GpuContext& gpuContext,
    WGPUTextureView   albedoTextureView,
    WGPUTextureView   normalTextureView,
    WGPUTextureView   depthTextureView)
{
    mGbufferBindGroup = GpuBindGroup{
        gpuContext.device,
        "Lighting pass gbuffer bind group",
        mGbufferBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 3>{
            textureBindGroupEntry(0, albedoTextureView),
            textureBindGroupEntry(1, normalTextureView),
            textureBindGroupEntry(2, depthTextureView)}};
}

DeferredRenderer::ResolvePass::ResolvePass(
    const GpuContext&                 gpuContext,
    const GpuBuffer&                  sampleBuffer,
    const WGPUTextureView             velocityTextureView,
    const DeferredRendererDescriptor& rendererDesc)
    : mVertexBuffer{gpuContext.device, "Resolve pass vertex buffer", {GpuBufferUsage::Vertex, GpuBufferUsage::CopyDst}, std::span<const float[2]>(quadVertexData)},
      mUniformBuffer{
          gpuContext.device,
          "Sky uniform buffer",
          {GpuBufferUsage::Uniform, GpuBufferUsage::CopyDst},
          sizeof(Uniforms)},
      mUniformBindGroup{},
      mAccumulationBuffer{
          gpuContext.device,
          "Deferred renderer :: accumulation buffer",
          GpuBufferUsages{GpuBufferUsage::Storage},
          3 * sizeof(float) * area(rendererDesc.maxFramebufferSize)},
      mTaaBindGroupLayout{},
      mTaaBindGroup{},
      mPipeline(nullptr)
{
    const GpuBindGroupLayout uniformBindGroupLayout{
        gpuContext.device,
        "TAA uniform bind group layout",
        mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment, sizeof(Uniforms))};

    mUniformBindGroup = GpuBindGroup{
        gpuContext.device,
        "TAA uniform bind group",
        uniformBindGroupLayout.ptr(),
        mUniformBuffer.bindGroupEntry(0)};

    mTaaBindGroupLayout = GpuBindGroupLayout{
        gpuContext.device,
        "TAA bind group layout",
        std::array<WGPUBindGroupLayoutEntry, 3>{
            sampleBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment),
            mAccumulationBuffer.bindGroupLayoutEntry(1, WGPUShaderStage_Fragment),
            textureBindGroupLayoutEntry(
                2, WGPUTextureSampleType_UnfilterableFloat, WGPUShaderStage_Fragment)}};

    mTaaBindGroup = GpuBindGroup{
        gpuContext.device,
        "TAA bind group",
        mTaaBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 3>{
            sampleBuffer.bindGroupEntry(0),
            mAccumulationBuffer.bindGroupEntry(1),
            textureBindGroupEntry(2, velocityTextureView)}};

    {
        // Pipeline layout

        const WGPUBindGroupLayout bindGroupLayouts[] = {
            uniformBindGroupLayout.ptr(), mTaaBindGroupLayout.ptr()};

        const WGPUPipelineLayoutDescriptor pipelineLayoutDesc{
            .nextInChain = nullptr,
            .label = "Resolve pass pipeline layout",
            .bindGroupLayoutCount = std::size(bindGroupLayouts),
            .bindGroupLayouts = bindGroupLayouts,
        };

        const WGPUPipelineLayout pipelineLayout =
            wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);

        // Vertex layout

        const WGPUVertexAttribute vertexAttributes[] = {WGPUVertexAttribute{
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
            .shaderLocation = 0,
        }};

        const WGPUVertexBufferLayout vertexBufferLayout{
            .arrayStride = sizeof(float[2]),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = std::size(vertexAttributes),
            .attributes = vertexAttributes,
        };

        // Shader module

        const WGPUShaderModule shaderModule = [&gpuContext]() -> WGPUShaderModule {
            const WGPUShaderModuleWGSLDescriptor wgslDesc = {
                .chain =
                    WGPUChainedStruct{
                        .next = nullptr,
                        .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                    },
                .code = DEFERRED_RENDERER_RESOLVE_PASS_SOURCE,
            };

            const WGPUShaderModuleDescriptor moduleDesc{
                .nextInChain = &wgslDesc.chain,
                .label = "Resolve pass shader",
            };

            return wgpuDeviceCreateShaderModule(gpuContext.device, &moduleDesc);
        }();
        NLRS_ASSERT(shaderModule != nullptr);

        // Fragment state

        const WGPUBlendState blendState{
            .color =
                WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_One,
                    .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                },
            .alpha =
                WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_Zero,
                    .dstFactor = WGPUBlendFactor_One,
                },
        };

        const WGPUColorTargetState colorTargets[] = {WGPUColorTargetState{
            .nextInChain = nullptr,
            .format = Window::SWAP_CHAIN_FORMAT,
            .blend = &blendState,
            .writeMask = WGPUColorWriteMask_All}};

        const WGPUFragmentState fragmentState{
            .nextInChain = nullptr,
            .module = shaderModule,
            .entryPoint = "fsMain",
            .constantCount = 0,
            .constants = nullptr,
            .targetCount = std::size(colorTargets),
            .targets = colorTargets,
        };

        // Pipeline

        const WGPURenderPipelineDescriptor pipelineDesc{
            .nextInChain = nullptr,
            .label = "resolve pass render pipeline",
            .layout = pipelineLayout,
            .vertex =
                WGPUVertexState{
                    .nextInChain = nullptr,
                    .module = shaderModule,
                    .entryPoint = "vsMain",
                    .constantCount = 0,
                    .constants = nullptr,
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            .primitive =
                WGPUPrimitiveState{
                    .nextInChain = nullptr,
                    .topology = WGPUPrimitiveTopology_TriangleList,
                    .stripIndexFormat = WGPUIndexFormat_Undefined,
                    .frontFace = WGPUFrontFace_CCW,
                    .cullMode = WGPUCullMode_Back,
                },
            .depthStencil = nullptr,
            .multisample =
                WGPUMultisampleState{
                    .nextInChain = nullptr,
                    .count = 1,
                    .mask = ~0u,
                    .alphaToCoverageEnabled = false,
                },
            .fragment = &fragmentState,
        };

        mPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);
        wgpuPipelineLayoutRelease(pipelineLayout);
    }
}

DeferredRenderer::ResolvePass::~ResolvePass()
{
    renderPipelineSafeRelease(mPipeline);
    mPipeline = nullptr;
}

DeferredRenderer::ResolvePass::ResolvePass(ResolvePass&& other) noexcept
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mAccumulationBuffer = std::move(other.mAccumulationBuffer);
        mTaaBindGroupLayout = std::move(other.mTaaBindGroupLayout);
        mTaaBindGroup = std::move(other.mTaaBindGroup);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
}

DeferredRenderer::ResolvePass& DeferredRenderer::ResolvePass::operator=(
    ResolvePass&& other) noexcept
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mAccumulationBuffer = std::move(other.mAccumulationBuffer);
        mTaaBindGroupLayout = std::move(other.mTaaBindGroupLayout);
        mTaaBindGroup = std::move(other.mTaaBindGroup);
        renderPipelineSafeRelease(mPipeline);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
    return *this;
}

void DeferredRenderer::ResolvePass::render(
    const GpuContext&        gpuContext,
    const WGPUCommandEncoder cmdEncoder,
    WGPUTextureView          targetTextureView,
    const Extent2f&          fbsize,
    const float              exposure,
    const std::uint32_t      frameCount,
    Gui&                     gui)
{
    {
        const Uniforms uniforms{glm::vec2(fbsize.x, fbsize.y), exposure, frameCount};
        wgpuQueueWriteBuffer(
            gpuContext.queue, mUniformBuffer.ptr(), 0, &uniforms, sizeof(Uniforms));
    }

    const WGPURenderPassEncoder renderPass = [cmdEncoder,
                                              targetTextureView]() -> WGPURenderPassEncoder {
        const WGPURenderPassColorAttachment colorAttachment{
            .nextInChain = nullptr,
            .view = targetTextureView,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED,
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0},
        };

        const WGPURenderPassDescriptor renderPassDesc{
            .nextInChain = nullptr,
            .label = "Lighting pass render pass",
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
            .depthStencilAttachment = nullptr,
            .occlusionQuerySet = nullptr,
            .timestampWrites = nullptr,
        };

        return wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);
    }();
    NLRS_ASSERT(renderPass != nullptr);

    wgpuRenderPassEncoderSetPipeline(renderPass, mPipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 0, mUniformBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPass, 1, mTaaBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(
        renderPass, 0, mVertexBuffer.ptr(), 0, mVertexBuffer.byteSize());
    wgpuRenderPassEncoderDraw(renderPass, 6, 1, 0, 0);

    gui.render(renderPass);

    wgpuRenderPassEncoderEnd(renderPass);
}

void DeferredRenderer::ResolvePass::resize(
    const GpuContext&     gpuContext,
    const GpuBuffer&      sampleBuffer,
    const WGPUTextureView velocityTextureView)
{
    mTaaBindGroup = GpuBindGroup{
        gpuContext.device,
        "TAA bind group",
        mTaaBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 3>{
            sampleBuffer.bindGroupEntry(0),
            mAccumulationBuffer.bindGroupEntry(1),
            textureBindGroupEntry(2, velocityTextureView)}};
}

DeferredRenderer::PerfStats DeferredRenderer::getPerfStats() const
{
    NLRS_ASSERT(mGbufferPassDurationsNs.size() == mLightingPassDurationsNs.size());

    if (mGbufferPassDurationsNs.empty())
    {
        return {};
    }

    return {
        0.000001f *
            static_cast<float>(std::accumulate(
                mGbufferPassDurationsNs.begin(), mGbufferPassDurationsNs.end(), 0ll)) /
            mGbufferPassDurationsNs.size(),
        0.000001f *
            static_cast<float>(std::accumulate(
                mLightingPassDurationsNs.begin(), mLightingPassDurationsNs.end(), 0ll)) /
            mLightingPassDurationsNs.size(),
        0.000001f *
            static_cast<float>(std::accumulate(
                mResolvePassDurationsNs.begin(), mResolvePassDurationsNs.end(), 0ll)) /
            mResolvePassDurationsNs.size()};
}

void DeferredRenderer::invalidateTemporalAccumulation()
{
    // In the first frame of the accumulation sequence, we are forced to write the lighting pass
    // sample straight to the accumulation buffer. This effectively resets the accumulation.
    mFrameCount = 0;
}
} // namespace nlrs
