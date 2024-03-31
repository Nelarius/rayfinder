#include "gpu_bind_group_layout.hpp"
#include "gpu_context.hpp"
#include "reference_path_tracer.hpp"
#include "shader_source.hpp"
#include "webgpu_utils.hpp"
#include "window.hpp"

#include <common/bvh.hpp>
#include <common/gltf_model.hpp>

#include <fmt/core.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <deque>
#include <numbers>
#include <numeric>
#include <span>
#include <stdexcept>
#include <utility>

namespace nlrs
{
namespace
{
struct FrameDataLayout
{
    Extent2u      dimensions;
    std::uint32_t frameCount;
    std::uint32_t padding;

    FrameDataLayout(const Extent2u& dimensions, const std::uint32_t frameCount)
        : dimensions(dimensions),
          frameCount(frameCount),
          padding(0)
    {
    }
};

struct CameraLayout
{
    glm::vec3 origin;
    float     padding0;
    glm::vec3 lowerLeftCorner;
    float     padding1;
    glm::vec3 horizontal;
    float     padding2;
    glm::vec3 vertical;
    float     padding3;
    glm::vec3 up;
    float     padding4;
    glm::vec3 right;
    float     lensRadius;

    CameraLayout(const Camera& c)
        : origin(c.origin),
          padding0(0.0f),
          lowerLeftCorner(c.lowerLeftCorner),
          padding1(0.0f),
          horizontal(c.horizontal),
          padding2(0.0f),
          vertical(c.vertical),
          padding3(0.0f),
          up(c.up),
          padding4(0.0f),
          right(c.right),
          lensRadius(c.lensRadius)
    {
    }
};

struct SamplingStateLayout
{
    std::uint32_t numSamplesPerPixel;
    std::uint32_t numBounces;
    std::uint32_t accumulatedSampleCount;
    std::uint32_t padding;

    SamplingStateLayout(
        const SamplingParams& samplingParams,
        const std::uint32_t   accumulatedSampleCount)
        : numSamplesPerPixel(samplingParams.numSamplesPerPixel),
          numBounces(samplingParams.numBounces),
          accumulatedSampleCount(accumulatedSampleCount),
          padding(0)
    {
    }
};

struct RenderParamsLayout
{
    FrameDataLayout     frameData;
    CameraLayout        camera;
    SamplingStateLayout samplingState;

    RenderParamsLayout(
        const Extent2u&         dimensions,
        const std::uint32_t     frameCount,
        const RenderParameters& renderParams,
        const std::uint32_t     accumulatedSampleCount)
        : frameData(dimensions, frameCount),
          camera(renderParams.camera),
          samplingState(renderParams.samplingParams, accumulatedSampleCount)
    {
    }
};

struct TimestampsLayout
{
    std::uint64_t renderPassBegin;
    std::uint64_t renderPassEnd;

    static constexpr std::uint32_t QUERY_COUNT = 2;
};
} // namespace

ReferencePathTracer::ReferencePathTracer(
    const RendererDescriptor& rendererDesc,
    const GpuContext&         gpuContext,
    const Scene               scene)
    : mVertexBuffer(
          gpuContext.device,
          "Vertex buffer",
          GpuBufferUsage::Vertex | GpuBufferUsage::CopyDst,
          std::span<const float[2]>(quadVertexData)),
      mRenderParamsBuffer(
          gpuContext.device,
          "render params buffer",
          GpuBufferUsage::Uniform | GpuBufferUsage::CopyDst,
          sizeof(RenderParamsLayout)),
      mPostProcessingParamsBuffer(
          gpuContext.device,
          "post processing params buffer",
          GpuBufferUsage::Uniform | GpuBufferUsage::CopyDst,
          sizeof(PostProcessingParameters)),
      mSkyStateBuffer(
          gpuContext.device,
          "sky state buffer",
          GpuBufferUsage::ReadOnlyStorage | GpuBufferUsage::CopyDst,
          sizeof(AlignedSkyState)),
      mRenderParamsBindGroup(),
      mBvhNodeBuffer(
          gpuContext.device,
          "bvh nodes buffer",
          GpuBufferUsage::ReadOnlyStorage | GpuBufferUsage::CopyDst,
          std::span<const BvhNode>(scene.bvhNodes)),
      mPositionAttributesBuffer(
          gpuContext.device,
          "position attributes buffer",
          GpuBufferUsage::ReadOnlyStorage | GpuBufferUsage::CopyDst,
          std::span<const PositionAttribute>(scene.positionAttributes)),
      mVertexAttributesBuffer(
          gpuContext.device,
          "vertex attributes buffer",
          GpuBufferUsage::ReadOnlyStorage | GpuBufferUsage::CopyDst,
          std::span<const VertexAttributes>(scene.vertexAttributes)),
      mTextureDescriptorBuffer(),
      mTextureBuffer(),
      mSceneBindGroup(),
      mImageBuffer(
          gpuContext.device,
          "image buffer",
          GpuBufferUsage::Storage,
          sizeof(float[4]) * rendererDesc.maxFramebufferSize.x * rendererDesc.maxFramebufferSize.y),
      mImageBindGroup(),
      mQuerySet(nullptr),
      mQueryBuffer(
          gpuContext.device,
          "render pass query buffer",
          GpuBufferUsage::QueryResolve | GpuBufferUsage::CopySrc,
          sizeof(TimestampsLayout)),
      mTimestampBuffer(
          gpuContext.device,
          "render pass timestamp buffer",
          GpuBufferUsage::CopyDst | GpuBufferUsage::MapRead,
          sizeof(TimestampsLayout)),
      mRenderPipeline(nullptr),
      mCurrentRenderParams(rendererDesc.renderParams),
      mCurrentPostProcessingParams(),
      mFrameCount(0),
      mAccumulatedSampleCount(0),
      mRenderPassDurationsNs()
{
    {
        struct TextureDescriptor
        {
            std::uint32_t width, height, offset;
        };
        // Ensure matches layout of `TextureDescriptor` definition in shader.
        std::vector<TextureDescriptor> textureDescriptors;
        textureDescriptors.reserve(scene.baseColorTextures.size());

        std::vector<Texture::BgraPixel> textureData;
        textureData.reserve(67108864);

        // Texture descriptors and texture data need to appended in the order of the model's
        // baseColorTextures. The model's baseColorTextureIndices index into that array, and we want
        // to use the same indices to index into the texture descriptor array.
        //
        // Summary:
        // baseColorTextureIndices -> baseColorTextures becomes
        // textureDescriptorIndices -> textureDescriptor -> textureData lookup

        for (const Texture& baseColorTexture : scene.baseColorTextures)
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
            GpuBufferUsage::ReadOnlyStorage | GpuBufferUsage::CopyDst,
            std::span<const TextureDescriptor>(textureDescriptors));

        const std::size_t textureDataNumBytes = textureData.size() * sizeof(Texture::BgraPixel);
        const std::size_t maxStorageBufferBindingSize =
            static_cast<std::size_t>(wgpuRequiredLimits.limits.maxStorageBufferBindingSize);
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
            GpuBufferUsage::ReadOnlyStorage | GpuBufferUsage::CopyDst,
            std::span<const Texture::BgraPixel>(textureData));
    }

    {
        // Blend state for color target

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

        // Color target information for fragment state

        const WGPUColorTargetState colorTarget{
            .nextInChain = nullptr,
            .format = Window::SWAP_CHAIN_FORMAT,
            .blend = &blendState,
            // We could write to only some of the color channels.
            .writeMask = WGPUColorWriteMask_All,
        };

        // Shader modules

        const WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
            .chain =
                WGPUChainedStruct{
                    .next = nullptr,
                    .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                },
            .code = REFERENCE_PATH_TRACER_SOURCE,
        };

        const WGPUShaderModuleDescriptor shaderDesc{
            .nextInChain = &shaderCodeDesc.chain,
            .label = "Shader module",
        };

        const WGPUShaderModule shaderModule =
            wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);

        const WGPUFragmentState fragmentState{
            .nextInChain = nullptr,
            .module = shaderModule,
            .entryPoint = "fsMain",
            .constantCount = 0,
            .constants = nullptr,
            .targetCount = 1,
            .targets = &colorTarget,
        };

        // Vertex layout

        const std::array<WGPUVertexAttribute, 1> vertexAttributes{WGPUVertexAttribute{
            // position
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

        // renderParams group layout

        const std::array<WGPUBindGroupLayoutEntry, 3> renderParamsBindGroupLayoutEntries{
            mRenderParamsBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment),
            mPostProcessingParamsBuffer.bindGroupLayoutEntry(1, WGPUShaderStage_Fragment),
            mSkyStateBuffer.bindGroupLayoutEntry(2, WGPUShaderStage_Fragment),
        };
        const GpuBindGroupLayout renderParamsBindGroupLayout{
            gpuContext.device,
            "RenderParams bind group layout",
            renderParamsBindGroupLayoutEntries};

        // scene bind group layout

        const std::array<WGPUBindGroupLayoutEntry, 5> sceneBindGroupLayoutEntries{
            mBvhNodeBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment),
            mPositionAttributesBuffer.bindGroupLayoutEntry(1, WGPUShaderStage_Fragment),
            mVertexAttributesBuffer.bindGroupLayoutEntry(2, WGPUShaderStage_Fragment),
            mTextureDescriptorBuffer.bindGroupLayoutEntry(3, WGPUShaderStage_Fragment),
            mTextureBuffer.bindGroupLayoutEntry(4, WGPUShaderStage_Fragment),
        };
        const GpuBindGroupLayout sceneBindGroupLayout{
            gpuContext.device, "Scene bind group layout", sceneBindGroupLayoutEntries};

        // image bind group layout

        const GpuBindGroupLayout imageBindGroupLayout{
            gpuContext.device,
            "Image bind group layout",
            mImageBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment)};

        // pipeline layout

        const std::array<WGPUBindGroupLayout, 3> bindGroupLayouts{
            renderParamsBindGroupLayout.ptr(),
            sceneBindGroupLayout.ptr(),
            imageBindGroupLayout.ptr(),
        };

        const WGPUPipelineLayoutDescriptor pipelineLayoutDesc{
            .nextInChain = nullptr,
            .label = "Pipeline layout",
            .bindGroupLayoutCount = bindGroupLayouts.size(),
            .bindGroupLayouts = bindGroupLayouts.data(),
        };
        const WGPUPipelineLayout pipelineLayout =
            wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);

        // renderParams bind group

        const std::array<WGPUBindGroupEntry, 3> renderParamsBindGroupEntries{
            mRenderParamsBuffer.bindGroupEntry(0),
            mPostProcessingParamsBuffer.bindGroupEntry(1),
            mSkyStateBuffer.bindGroupEntry(2),
        };
        mRenderParamsBindGroup = GpuBindGroup{
            gpuContext.device,
            "RenderParams bind group",
            renderParamsBindGroupLayout.ptr(),
            renderParamsBindGroupEntries};

        // scene bind group

        const std::array<WGPUBindGroupEntry, 5> sceneBindGroupEntries{
            mBvhNodeBuffer.bindGroupEntry(0),
            mPositionAttributesBuffer.bindGroupEntry(1),
            mVertexAttributesBuffer.bindGroupEntry(2),
            mTextureDescriptorBuffer.bindGroupEntry(3),
            mTextureBuffer.bindGroupEntry(4),
        };
        mSceneBindGroup = GpuBindGroup{
            gpuContext.device,
            "Scene bind group",
            sceneBindGroupLayout.ptr(),
            sceneBindGroupEntries};

        // image bind group

        mImageBindGroup = GpuBindGroup{
            gpuContext.device,
            "Image bind group",
            imageBindGroupLayout.ptr(),
            mImageBuffer.bindGroupEntry(0)};

        // pipeline

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
                    .bufferCount = 1,
                    .buffers = &vertexBufferLayout,
                },
            // NOTE: the primitive assembly config, defines how the primitive assembly and
            // rasterization stages are configured.
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
            // NOTE: the fragment state is a potentially null value.
            .fragment = &fragmentState,
        };

        mRenderPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

        wgpuPipelineLayoutRelease(pipelineLayout);
    }

    // Timestamp query sets
    {
        const WGPUQuerySetDescriptor querySetDesc{
            .nextInChain = nullptr,
            .label = "renderpass timestamp query set",
            .type = WGPUQueryType_Timestamp,
            .count = TimestampsLayout::QUERY_COUNT};
        mQuerySet = wgpuDeviceCreateQuerySet(gpuContext.device, &querySetDesc);
    }
}

ReferencePathTracer::ReferencePathTracer(ReferencePathTracer&& other)
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mRenderParamsBuffer = std::move(other.mRenderParamsBuffer);
        mPostProcessingParamsBuffer = std::move(other.mPostProcessingParamsBuffer);
        mSkyStateBuffer = std::move(other.mSkyStateBuffer);
        mRenderParamsBindGroup = std::move(other.mRenderParamsBindGroup);
        mBvhNodeBuffer = std::move(other.mBvhNodeBuffer);
        mPositionAttributesBuffer = std::move(other.mPositionAttributesBuffer);
        mVertexAttributesBuffer = std::move(other.mVertexAttributesBuffer);
        mTextureDescriptorBuffer = std::move(other.mTextureDescriptorBuffer);
        mTextureBuffer = std::move(other.mTextureBuffer);
        mSceneBindGroup = std::move(other.mSceneBindGroup);
        mImageBuffer = std::move(other.mImageBuffer);
        mImageBindGroup = std::move(other.mImageBindGroup);
        mQuerySet = other.mQuerySet;
        other.mQuerySet = nullptr;
        mQueryBuffer = std::move(other.mQueryBuffer);
        mTimestampBuffer = std::move(other.mTimestampBuffer);
        mRenderPipeline = other.mRenderPipeline;
        other.mRenderPipeline = nullptr;

        mCurrentRenderParams = other.mCurrentRenderParams;
        mCurrentPostProcessingParams = other.mCurrentPostProcessingParams;
        mFrameCount = other.mFrameCount;
        mAccumulatedSampleCount = other.mAccumulatedSampleCount;

        mRenderPassDurationsNs = std::move(other.mRenderPassDurationsNs);
    }
}

ReferencePathTracer& ReferencePathTracer::operator=(ReferencePathTracer&& other)
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mRenderParamsBuffer = std::move(other.mRenderParamsBuffer);
        mPostProcessingParamsBuffer = std::move(other.mPostProcessingParamsBuffer);
        mSkyStateBuffer = std::move(other.mSkyStateBuffer);
        mRenderParamsBindGroup = std::move(other.mRenderParamsBindGroup);
        mBvhNodeBuffer = std::move(other.mBvhNodeBuffer);
        mPositionAttributesBuffer = std::move(other.mPositionAttributesBuffer);
        mVertexAttributesBuffer = std::move(other.mVertexAttributesBuffer);
        mTextureDescriptorBuffer = std::move(other.mTextureDescriptorBuffer);
        mTextureBuffer = std::move(other.mTextureBuffer);
        mSceneBindGroup = std::move(other.mSceneBindGroup);
        mImageBuffer = std::move(other.mImageBuffer);
        mImageBindGroup = std::move(other.mImageBindGroup);
        mQuerySet = other.mQuerySet;
        other.mQuerySet = nullptr;
        mQueryBuffer = std::move(other.mQueryBuffer);
        mTimestampBuffer = std::move(other.mTimestampBuffer);
        mRenderPipeline = other.mRenderPipeline;
        other.mRenderPipeline = nullptr;

        mCurrentRenderParams = other.mCurrentRenderParams;
        mCurrentPostProcessingParams = other.mCurrentPostProcessingParams;
        mFrameCount = other.mFrameCount;
        mAccumulatedSampleCount = other.mAccumulatedSampleCount;

        mRenderPassDurationsNs = std::move(other.mRenderPassDurationsNs);
    }
    return *this;
}

ReferencePathTracer::~ReferencePathTracer()
{
    renderPipelineSafeRelease(mRenderPipeline);
    mRenderPipeline = nullptr;
    querySetSafeRelease(mQuerySet);
    mQuerySet = nullptr;
}

void ReferencePathTracer::setRenderParameters(const RenderParameters& renderParams)
{
    if (mCurrentRenderParams != renderParams)
    {
        mCurrentRenderParams = renderParams;
        mAccumulatedSampleCount = 0; // reset the temporal accumulation
    }
}

void ReferencePathTracer::setPostProcessingParameters(
    const PostProcessingParameters& postProcessingParameters)
{
    mCurrentPostProcessingParams = postProcessingParameters;
}

void ReferencePathTracer::render(const GpuContext& gpuContext, WGPUTextureView textureView)
{
    // Non-standard Dawn way to ensure that Dawn ticks pending async operations.
    do
    {
        wgpuDeviceTick(gpuContext.device);
    } while (wgpuBufferGetMapState(mTimestampBuffer.ptr()) != WGPUBufferMapState_Unmapped);

    {
        assert(mAccumulatedSampleCount <= mCurrentRenderParams.samplingParams.numSamplesPerPixel);
        const RenderParamsLayout renderParamsLayout{
            mCurrentRenderParams.framebufferSize,
            mFrameCount++,
            mCurrentRenderParams,
            mAccumulatedSampleCount};
        wgpuQueueWriteBuffer(
            gpuContext.queue,
            mRenderParamsBuffer.ptr(),
            0,
            &renderParamsLayout,
            sizeof(RenderParamsLayout));
        mAccumulatedSampleCount = std::min(
            mAccumulatedSampleCount + 1, mCurrentRenderParams.samplingParams.numSamplesPerPixel);
        wgpuQueueWriteBuffer(
            gpuContext.queue,
            mPostProcessingParamsBuffer.ptr(),
            0,
            &mCurrentPostProcessingParams,
            sizeof(PostProcessingParameters));
        const AlignedSkyState skyState{mCurrentRenderParams.sky};
        wgpuQueueWriteBuffer(
            gpuContext.queue, mSkyStateBuffer.ptr(), 0, &skyState, sizeof(AlignedSkyState));
    }

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    wgpuCommandEncoderWriteTimestamp(encoder, mQuerySet, 0);
    {
        const WGPURenderPassEncoder renderPassEncoder = [encoder,
                                                         textureView]() -> WGPURenderPassEncoder {
            const WGPURenderPassColorAttachment renderPassColorAttachment{
                .nextInChain = nullptr,
                .view = textureView,
                .depthSlice =
                    WGPU_DEPTH_SLICE_UNDEFINED, // depthSlice must be initialized with 'undefined'
                                                // value for 2d color attachments.
                .resolveTarget = nullptr,
                .loadOp = WGPULoadOp_Clear,
                .storeOp = WGPUStoreOp_Store,
                .clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0},
            };

            const WGPURenderPassDescriptor renderPassDesc = {
                .nextInChain = nullptr,
                .label = "Render pass encoder",
                .colorAttachmentCount = 1,
                .colorAttachments = &renderPassColorAttachment,
                .depthStencilAttachment = nullptr,
                .occlusionQuerySet = nullptr,
                .timestampWrites = nullptr,
            };

            return wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
        }();

        {
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mRenderPipeline);
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 0, mRenderParamsBindGroup.ptr(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 1, mSceneBindGroup.ptr(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 2, mImageBindGroup.ptr(), 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(
                renderPassEncoder, 0, mVertexBuffer.ptr(), 0, mVertexBuffer.byteSize());
            wgpuRenderPassEncoderDraw(renderPassEncoder, 6, 1, 0, 0);
        }

        wgpuRenderPassEncoderEnd(renderPassEncoder);
    }
    wgpuCommandEncoderWriteTimestamp(encoder, mQuerySet, 1);

    wgpuCommandEncoderResolveQuerySet(
        encoder, mQuerySet, 0, TimestampsLayout::QUERY_COUNT, mQueryBuffer.ptr(), 0);
    wgpuCommandEncoderCopyBufferToBuffer(
        encoder, mQueryBuffer.ptr(), 0, mTimestampBuffer.ptr(), 0, sizeof(TimestampsLayout));

    const WGPUCommandBuffer cmdBuffer = [encoder]() {
        const WGPUCommandBufferDescriptor cmdBufferDesc{
            .nextInChain = nullptr,
            .label = "ReferencePathTracer command buffer",
        };
        return wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    }();
    wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);

    // Map query timers
    wgpuBufferMapAsync(
        mTimestampBuffer.ptr(),
        WGPUMapMode_Read,
        0,
        sizeof(TimestampsLayout),
        [](const WGPUBufferMapAsyncStatus status, void* const userdata) -> void {
            if (status == WGPUBufferMapAsyncStatus_Success)
            {
                assert(userdata);
                ReferencePathTracer& renderer = *static_cast<ReferencePathTracer*>(userdata);
                GpuBuffer&           timestampBuffer = renderer.mTimestampBuffer;
                const void*          bufferData = wgpuBufferGetConstMappedRange(
                    timestampBuffer.ptr(), 0, sizeof(TimestampsLayout));
                assert(bufferData);

                const TimestampsLayout* const timestamps =
                    reinterpret_cast<const TimestampsLayout*>(bufferData);

                std::deque<std::uint64_t>& renderPassDurations = renderer.mRenderPassDurationsNs;
                const std::uint64_t        renderPassDelta =
                    timestamps->renderPassEnd - timestamps->renderPassBegin;

                renderPassDurations.push_back(renderPassDelta);
                if (renderPassDurations.size() > 30)
                {
                    renderPassDurations.pop_front();
                }

                wgpuBufferUnmap(timestampBuffer.ptr());
            }
            else
            {
                std::fprintf(stderr, "Failed to map query buffer\n");
            }
        },
        this);
}

float ReferencePathTracer::averageRenderpassDurationMs() const
{
    if (mRenderPassDurationsNs.empty())
    {
        return 0.0f;
    }

    const std::uint64_t sum = std::accumulate(
        mRenderPassDurationsNs.begin(), mRenderPassDurationsNs.end(), std::uint64_t(0));
    return 0.000001f * static_cast<float>(sum) / mRenderPassDurationsNs.size();
}

float ReferencePathTracer::renderProgressPercentage() const
{
    return 100.0f * static_cast<float>(mAccumulatedSampleCount) /
           static_cast<float>(mCurrentRenderParams.samplingParams.numSamplesPerPixel);
}
} // namespace nlrs
