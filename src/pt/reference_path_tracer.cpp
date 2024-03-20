#include "gpu_bind_group_layout.hpp"
#include "gpu_context.hpp"
#include "reference_path_tracer.hpp"
#include "webgpu_utils.hpp"
#include "window.hpp"

#include <common/bvh.hpp>
#include <common/gltf_model.hpp>
#include <common/platform.hpp>
#include <hw-skymodel/hw_skymodel.h>

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
inline constexpr float PI = std::numbers::pi_v<float>;
inline constexpr float DEGREES_TO_RADIANS = PI / 180.0f;

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

struct SkyStateLayout
{
    float     params[27];        // offset: 0
    float     skyRadiances[3];   // offset: 27
    float     solarRadiances[3]; // offset: 30
    float     padding1[3];       // offset: 33
    glm::vec3 sunDirection;      // offset: 36
    float     padding2;          // offset: 39

    SkyStateLayout(const Sky& sky)
        : params{0},
          skyRadiances{0},
          solarRadiances{0},
          padding1{0.f, 0.f, 0.f},
          sunDirection(0.f),
          padding2(0.0f)
    {
        const float sunZenith = sky.sunZenithDegrees * DEGREES_TO_RADIANS;
        const float sunAzimuth = sky.sunAzimuthDegrees * DEGREES_TO_RADIANS;

        sunDirection = glm::normalize(glm::vec3(
            std::sin(sunZenith) * std::cos(sunAzimuth),
            std::cos(sunZenith),
            -std::sin(sunZenith) * std::sin(sunAzimuth)));

        const sky_params skyParams{
            .elevation = 0.5f * PI - sunZenith,
            .turbidity = sky.turbidity,
            .albedo = {sky.albedo[0], sky.albedo[1], sky.albedo[2]}};

        sky_state                   skyState;
        [[maybe_unused]] const auto r = sky_state_new(&skyParams, &skyState);
        // TODO: exceptional error handling
        assert(r == sky_state_result_success);

        std::memcpy(params, skyState.params, sizeof(skyState.params));
        std::memcpy(skyRadiances, skyState.sky_radiances, sizeof(skyState.sky_radiances));
        std::memcpy(solarRadiances, skyState.solar_radiances, sizeof(skyState.solar_radiances));
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
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
          std::span<const float[2]>(quadVertexData)),
      mUniformsBuffer(),
      mUniformsBindGroup(),
      mRenderParamsBuffer(
          gpuContext.device,
          "render params buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
          sizeof(RenderParamsLayout)),
      mPostProcessingParamsBuffer(
          gpuContext.device,
          "post processing params buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
          sizeof(PostProcessingParameters)),
      mSkyStateBuffer(
          gpuContext.device,
          "sky state buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          sizeof(SkyStateLayout)),
      mRenderParamsBindGroup(),
      mBvhNodeBuffer(
          gpuContext.device,
          "bvh nodes buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const BvhNode>(scene.bvhNodes)),
      mPositionAttributesBuffer(
          gpuContext.device,
          "position attributes buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const PositionAttribute>(scene.positionAttributes)),
      mVertexAttributesBuffer(
          gpuContext.device,
          "vertex attributes buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const VertexAttributes>(scene.vertexAttributes)),
      mTextureDescriptorBuffer(),
      mTextureBuffer(),
      mSceneBindGroup(),
      mImageBuffer(
          gpuContext.device,
          "image buffer",
          WGPUBufferUsage_Storage,
          sizeof(float[4]) * rendererDesc.maxFramebufferSize.x * rendererDesc.maxFramebufferSize.y),
      mImageBindGroup(),
      mQuerySet(nullptr),
      mQueryBuffer(
          gpuContext.device,
          "render pass query buffer",
          WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc,
          sizeof(TimestampsLayout)),
      mTimestampBuffer(
          gpuContext.device,
          "render pass timestamp buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
          sizeof(TimestampsLayout)),
      mRenderPipeline(nullptr),
      mCurrentRenderParams(rendererDesc.renderParams),
      mCurrentPostProcessingParams(),
      mFrameCount(0),
      mAccumulatedSampleCount(0),
      mRenderPassDurationsNs()
{
    {
        // DirectX, Metal, wgpu share the same left-handed coordinate system
        // for their normalized device coordinates:
        // https://github.com/gfx-rs/gfx/tree/master/src/backend/dx12
        const glm::mat4 viewProjectionMatrix = glm::orthoLH(-0.5f, 0.5f, -0.5f, 0.5f, -1.f, 1.f);

        mUniformsBuffer = GpuBuffer(
            gpuContext.device,
            "uniforms buffer",
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(&viewProjectionMatrix[0]),
                sizeof(glm::mat4)));
    }

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
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
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
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
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

        const std::string shaderSource = loadShaderSource("reference_path_tracer.wgsl");

        const WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
            .chain =
                WGPUChainedStruct{
                    .next = nullptr,
                    .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                },
            .code = shaderSource.c_str(),
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

        // uniforms bind group layout

        const GpuBindGroupLayout uniformsBindGroupLayout{
            gpuContext.device,
            "Uniforms group layout",
            mUniformsBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Vertex)};

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

        const std::array<WGPUBindGroupLayout, 4> bindGroupLayouts{
            uniformsBindGroupLayout.ptr(),
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

        // uniforms bind group

        mUniformsBindGroup = GpuBindGroup{
            gpuContext.device,
            "Uniforms bind group",
            uniformsBindGroupLayout.ptr(),
            mUniformsBuffer.bindGroupEntry(0)};

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
        mUniformsBuffer = std::move(other.mUniformsBuffer);
        mUniformsBindGroup = std::move(other.mUniformsBindGroup);
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
        mUniformsBuffer = std::move(other.mUniformsBuffer);
        mUniformsBindGroup = std::move(other.mUniformsBindGroup);
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
    } while (wgpuBufferGetMapState(mTimestampBuffer.handle()) != WGPUBufferMapState_Unmapped);

    {
        assert(mAccumulatedSampleCount <= mCurrentRenderParams.samplingParams.numSamplesPerPixel);
        const RenderParamsLayout renderParamsLayout{
            mCurrentRenderParams.framebufferSize,
            mFrameCount++,
            mCurrentRenderParams,
            mAccumulatedSampleCount};
        wgpuQueueWriteBuffer(
            gpuContext.queue,
            mRenderParamsBuffer.handle(),
            0,
            &renderParamsLayout,
            sizeof(RenderParamsLayout));
        mAccumulatedSampleCount = std::min(
            mAccumulatedSampleCount + 1, mCurrentRenderParams.samplingParams.numSamplesPerPixel);
        wgpuQueueWriteBuffer(
            gpuContext.queue,
            mPostProcessingParamsBuffer.handle(),
            0,
            &mCurrentPostProcessingParams,
            sizeof(PostProcessingParameters));
        const SkyStateLayout skyStateLayout{mCurrentRenderParams.sky};
        wgpuQueueWriteBuffer(
            gpuContext.queue, mSkyStateBuffer.handle(), 0, &skyStateLayout, sizeof(SkyStateLayout));
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
                renderPassEncoder, 0, mUniformsBindGroup.ptr(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 1, mRenderParamsBindGroup.ptr(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 2, mSceneBindGroup.ptr(), 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 3, mImageBindGroup.ptr(), 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(
                renderPassEncoder, 0, mVertexBuffer.handle(), 0, mVertexBuffer.byteSize());
            wgpuRenderPassEncoderDraw(renderPassEncoder, 6, 1, 0, 0);
        }

        wgpuRenderPassEncoderEnd(renderPassEncoder);
    }
    wgpuCommandEncoderWriteTimestamp(encoder, mQuerySet, 1);

    wgpuCommandEncoderResolveQuerySet(
        encoder, mQuerySet, 0, TimestampsLayout::QUERY_COUNT, mQueryBuffer.handle(), 0);
    wgpuCommandEncoderCopyBufferToBuffer(
        encoder, mQueryBuffer.handle(), 0, mTimestampBuffer.handle(), 0, sizeof(TimestampsLayout));

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
        mTimestampBuffer.handle(),
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
                    timestampBuffer.handle(), 0, sizeof(TimestampsLayout));
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

                wgpuBufferUnmap(timestampBuffer.handle());
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
