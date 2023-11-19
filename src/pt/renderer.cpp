#include "configuration.hpp"
#include "gpu_context.hpp"
#include "gui.hpp"
#include "renderer.hpp"

#include <common/bvh.hpp>
#include <common/gltf_model.hpp>
#include <common/platform.hpp>
#include <hw-skymodel/hw_skymodel.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <deque>
#include <format>
#include <fstream>
#include <limits>
#include <numbers>
#include <numeric>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace nlrs
{
inline constexpr float PI = std::numbers::pi_v<float>;
inline constexpr float DEGREES_TO_RADIANS = PI / 180.0f;

namespace
{
struct Vertex
{
    float position[2];
    float uv[2];
};

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
    float     lensRadius;

    CameraLayout(const Camera& c)
        : origin(c.origin),
          padding0(0.0f),
          lowerLeftCorner(c.lowerLeftCorner),
          padding1(0.0f),
          horizontal(c.horizontal),
          padding2(0.0f),
          vertical(c.vertical),
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
    float     params[27];
    float     radiances[3];
    float     padding1[2];
    glm::vec3 sunDirection;
    float     padding2;

    SkyStateLayout(const Sky& sky)
        : params{0},
          radiances{0},
          padding1{0.f, 0.f},
          sunDirection(0.0f),
          padding2(0.0f)
    {
        const float sunZenith = sky.sunZenithDegrees * DEGREES_TO_RADIANS;
        const float sunAzimuth = sky.sunAzimuthDegrees * DEGREES_TO_RADIANS;

        sunDirection = glm::normalize(glm::vec3(
            std::sin(sunZenith) * std::cos(sunAzimuth),
            std::cos(sunZenith),
            -std::sin(sunZenith) * std::sin(sunAzimuth)));

        const SkyParams skyParams{
            .elevation = 0.5f * PI - sunZenith,
            .turbidity = sky.turbidity,
            .albedo = {sky.albedo[0], sky.albedo[1], sky.albedo[2]}};

        SkyState                    skyState;
        [[maybe_unused]] const auto r = skyStateNew(&skyParams, &skyState);
        // TODO: exceptional error handling
        assert(r == SkyStateResult_Success);

        std::memcpy(params, skyState.params, sizeof(skyState.params));
        std::memcpy(radiances, skyState.radiances, sizeof(skyState.radiances));
    }
};

struct RenderParamsLayout
{
    FrameDataLayout     frameData;
    CameraLayout        camera;
    SamplingStateLayout samplingState;
    SkyStateLayout      skyState;

    RenderParamsLayout(
        const Extent2u&         dimensions,
        const std::uint32_t     frameCount,
        const RenderParameters& renderParams,
        const std::uint32_t     accumulatedSampleCount)
        : frameData(dimensions, frameCount),
          camera(renderParams.camera),
          samplingState(renderParams.samplingParams, accumulatedSampleCount),
          skyState(renderParams.sky)
    {
    }
};

struct TimestampsLayout
{
    std::uint64_t renderPassBegin;
    std::uint64_t renderPassEnd;
    std::uint64_t drawBegin;
    std::uint64_t drawEnd;

    static constexpr std::uint32_t QUERY_COUNT = 4;
};

void querySetSafeRelease(const WGPUQuerySet querySet)
{
    if (querySet)
    {
        wgpuQuerySetDestroy(querySet);
        wgpuQuerySetRelease(querySet);
    }
}

void bindGroupSafeRelease(const WGPUBindGroup bindGroup)
{
    if (bindGroup)
    {
        wgpuBindGroupRelease(bindGroup);
    }
}

void renderPipelineSafeRelease(const WGPURenderPipeline pipeline)
{
    if (pipeline)
    {
        wgpuRenderPipelineRelease(pipeline);
    }
}
} // namespace

Renderer::Renderer(
    const RendererDescriptor& rendererDesc,
    const GpuContext&         gpuContext,
    const Scene               scene)
    : vertexBuffer(),
      uniformsBuffer(),
      uniformsBindGroup(nullptr),
      renderParamsBuffer(
          gpuContext.device,
          "render params buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
          sizeof(RenderParamsLayout)),
      renderParamsBindGroup(nullptr),
      bvhNodeBuffer(
          gpuContext.device,
          "bvh nodes buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const BvhNode>(scene.bvh.nodes)),
      triangleBuffer(
          gpuContext.device,
          "triangles buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const Positions>(scene.bvh.positions)),
      normalsBuffer(
          gpuContext.device,
          "normals buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const Normals>(scene.normals)),
      texCoordsBuffer(
          gpuContext.device,
          "tex coords buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const TexCoords>(scene.texCoords)),
      textureDescriptorIndicesBuffer(
          gpuContext.device,
          "texture descriptor indices buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
          std::span<const std::uint32_t>(scene.textureIndices)),
      textureDescriptorBuffer(),
      textureBuffer(),
      sceneBindGroup(nullptr),
      imageBuffer(
          gpuContext.device,
          "image buffer",
          WGPUBufferUsage_Storage,
          sizeof(float[4]) * rendererDesc.maxFramebufferSize.x * rendererDesc.maxFramebufferSize.y),
      imageBindGroup(nullptr),
      querySet(nullptr),
      queryBuffer(
          gpuContext.device,
          "render pass query buffer",
          WGPUBufferUsage_QueryResolve | WGPUBufferUsage_CopySrc,
          sizeof(TimestampsLayout)),
      timestampBuffer(
          gpuContext.device,
          "render pass timestamp buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead,
          sizeof(TimestampsLayout)),
      renderPipeline(nullptr),
      currentRenderParams(rendererDesc.renderParams),
      frameCount(0),
      accumulatedSampleCount(0),
      timestampBufferMapContext{&timestampBuffer, &drawDurationsNs, &renderPassDurationsNs}
{
    {
        const std::array<Vertex, 6> vertexData{
            Vertex{{-0.5f, -0.5f}, {0.0f, 0.0f}},
            Vertex{{0.5f, -0.5f}, {1.0f, 0.0f}},
            Vertex{{0.5f, 0.5f}, {1.0f, 1.0f}},
            Vertex{{0.5f, 0.5f}, {1.0f, 1.0f}},
            Vertex{{-0.5f, 0.5f}, {0.0f, 1.0f}},
            Vertex{{-0.5f, -0.5f}, {0.0f, 0.0f}},
        };

        vertexBuffer = GpuBuffer(
            gpuContext.device,
            "Vertex buffer",
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            std::span<const Vertex>(vertexData));
    }

    {
        // DirectX, Metal, wgpu share the same left-handed coordinate system
        // for their normalized device coordinates:
        // https://github.com/gfx-rs/gfx/tree/master/src/backend/dx12
        const glm::mat4 viewProjectionMatrix = glm::orthoLH(-0.5f, 0.5f, -0.5f, 0.5f, -1.f, 1.f);

        uniformsBuffer = GpuBuffer(
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
        textureDescriptors.reserve(scene.textureIndices.size());

        std::vector<Texture::RgbaPixel> textureData;
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
                pixels.size() * sizeof(Texture::RgbaPixel));

            textureDescriptors.push_back({width, height, offset});
        }

        textureDescriptorBuffer = GpuBuffer(
            gpuContext.device,
            "texture descriptor buffer",
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
            std::span<const TextureDescriptor>(textureDescriptors));

        const std::size_t textureDataNumBytes = textureData.size() * sizeof(Texture::RgbaPixel);
        const std::size_t maxStorageBufferBindingSize =
            static_cast<std::size_t>(wgpuRequiredLimits.limits.maxStorageBufferBindingSize);
        if (textureDataNumBytes > maxStorageBufferBindingSize)
        {
            throw std::runtime_error(std::format(
                "Texture buffer size ({}) exceeds maxStorageBufferBindingSize ({}).",
                textureDataNumBytes,
                maxStorageBufferBindingSize));
        }

        textureBuffer = GpuBuffer(
            gpuContext.device,
            "texture buffer",
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
            std::span<const Texture::RgbaPixel>(textureData));
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
            .format = GpuContext::swapChainFormat,
            .blend = &blendState,
            // We could write to only some of the color channels.
            .writeMask = WGPUColorWriteMask_All,
        };

        // Shader modules

        auto loadShaderSource = [](std::string_view path) -> std::string {
            std::ifstream file(path.data());
            if (!file)
            {
                throw std::runtime_error(std::format("Error opening file: {}.", path));
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return buffer.str();
        };

        const std::string shaderSource = loadShaderSource("raytracer.wgsl");

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

        std::array<WGPUVertexAttribute, 2> vertexAttributes{
            WGPUVertexAttribute{
                // position
                .format = WGPUVertexFormat_Float32x2,
                .offset = 0,
                .shaderLocation = 0,
            },
            WGPUVertexAttribute{
                // texCoord
                .format = WGPUVertexFormat_Float32x2,
                .offset = 2 * sizeof(float),
                .shaderLocation = 1,
            },
        };

        WGPUVertexBufferLayout vertexBufferLayout{
            .arrayStride = 1 * sizeof(Vertex),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 2,
            .attributes = vertexAttributes.data(),
        };

        // uniforms bind group layout

        const WGPUBindGroupLayoutEntry uniformsBindGroupLayoutEntry =
            uniformsBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Vertex);

        const WGPUBindGroupLayoutDescriptor uniformsBindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "uniforms group layout",
            .entryCount = 1,
            .entries = &uniformsBindGroupLayoutEntry,
        };
        const WGPUBindGroupLayout uniformsBindGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &uniformsBindGroupLayoutDesc);

        // renderParams group layout

        const WGPUBindGroupLayoutEntry renderParamsBindGroupLayoutEntry =
            renderParamsBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment);

        const WGPUBindGroupLayoutDescriptor renderParamsBindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "renderParams bind group layout",
            .entryCount = 1,
            .entries = &renderParamsBindGroupLayoutEntry,
        };
        const WGPUBindGroupLayout renderParamsBindGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &renderParamsBindGroupLayoutDesc);

        // scene bind group layout

        const std::array<WGPUBindGroupLayoutEntry, 7> sceneBindGroupLayoutEntries{
            bvhNodeBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment),
            triangleBuffer.bindGroupLayoutEntry(1, WGPUShaderStage_Fragment),
            normalsBuffer.bindGroupLayoutEntry(2, WGPUShaderStage_Fragment),
            texCoordsBuffer.bindGroupLayoutEntry(3, WGPUShaderStage_Fragment),
            textureDescriptorIndicesBuffer.bindGroupLayoutEntry(4, WGPUShaderStage_Fragment),
            textureDescriptorBuffer.bindGroupLayoutEntry(5, WGPUShaderStage_Fragment),
            textureBuffer.bindGroupLayoutEntry(6, WGPUShaderStage_Fragment),
        };

        const WGPUBindGroupLayoutDescriptor sceneBindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "scene bind group layout",
            .entryCount = sceneBindGroupLayoutEntries.size(),
            .entries = sceneBindGroupLayoutEntries.data(),
        };

        const WGPUBindGroupLayout sceneBindGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &sceneBindGroupLayoutDesc);

        // image bind group layout

        const WGPUBindGroupLayoutEntry imageBindGroupLayoutEntry =
            imageBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment);

        const WGPUBindGroupLayoutDescriptor imageBindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "image bind group layout",
            .entryCount = 1,
            .entries = &imageBindGroupLayoutEntry,
        };

        const WGPUBindGroupLayout imageBindGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &imageBindGroupLayoutDesc);

        // pipeline layout

        std::array<WGPUBindGroupLayout, 4> bindGroupLayouts{
            uniformsBindGroupLayout,
            renderParamsBindGroupLayout,
            sceneBindGroupLayout,
            imageBindGroupLayout,
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

        const WGPUBindGroupEntry uniformsBindGroupEntry = uniformsBuffer.bindGroupEntry(0);

        const WGPUBindGroupDescriptor uniformsBindGroupDesc{
            .nextInChain = nullptr,
            .label = "Bind group",
            .layout = uniformsBindGroupLayout,
            .entryCount = 1,
            .entries = &uniformsBindGroupEntry,
        };
        uniformsBindGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &uniformsBindGroupDesc);

        // renderParams bind group

        const WGPUBindGroupEntry renderParamsBindGroupEntry = renderParamsBuffer.bindGroupEntry(0);

        const WGPUBindGroupDescriptor renderParamsBindGroupDesc{
            .nextInChain = nullptr,
            .label = "image bind group",
            .layout = renderParamsBindGroupLayout,
            .entryCount = 1,
            .entries = &renderParamsBindGroupEntry,
        };
        renderParamsBindGroup =
            wgpuDeviceCreateBindGroup(gpuContext.device, &renderParamsBindGroupDesc);

        // scene bind group

        const std::array<WGPUBindGroupEntry, 7> sceneBindGroupEntries{
            bvhNodeBuffer.bindGroupEntry(0),
            triangleBuffer.bindGroupEntry(1),
            normalsBuffer.bindGroupEntry(2),
            texCoordsBuffer.bindGroupEntry(3),
            textureDescriptorIndicesBuffer.bindGroupEntry(4),
            textureDescriptorBuffer.bindGroupEntry(5),
            textureBuffer.bindGroupEntry(6),
        };

        const WGPUBindGroupDescriptor sceneBindGroupDesc{
            .nextInChain = nullptr,
            .label = "scene bind group",
            .layout = sceneBindGroupLayout,
            .entryCount = sceneBindGroupEntries.size(),
            .entries = sceneBindGroupEntries.data(),
        };
        sceneBindGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &sceneBindGroupDesc);

        // image bind group

        const WGPUBindGroupEntry imageBindGroupEntry = imageBuffer.bindGroupEntry(0);

        const WGPUBindGroupDescriptor imageBindGroupDesc{
            .nextInChain = nullptr,
            .label = "image bind group",
            .layout = imageBindGroupLayout,
            .entryCount = 1,
            .entries = &imageBindGroupEntry,
        };

        imageBindGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &imageBindGroupDesc);

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
                    .cullMode = WGPUCullMode_None, // TODO: this could be Front, once a triangle is
                                                   // confirmed onscreen
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

        renderPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);
    }

    // Timestamp query sets
    {
        const WGPUQuerySetDescriptor querySetDesc{
            .nextInChain = nullptr,
            .label = "renderpass timestamp query set",
            .type = WGPUQueryType_Timestamp,
            .count = TimestampsLayout::QUERY_COUNT,
            .pipelineStatistics = nullptr,
            .pipelineStatisticsCount = 0u};
        querySet = wgpuDeviceCreateQuerySet(gpuContext.device, &querySetDesc);
    }
}

Renderer::~Renderer()
{
    renderPipelineSafeRelease(renderPipeline);
    renderPipeline = nullptr;
    querySetSafeRelease(querySet);
    querySet = nullptr;
    bindGroupSafeRelease(imageBindGroup);
    imageBindGroup = nullptr;
    bindGroupSafeRelease(sceneBindGroup);
    sceneBindGroup = nullptr;
    bindGroupSafeRelease(renderParamsBindGroup);
    renderParamsBindGroup = nullptr;
    bindGroupSafeRelease(uniformsBindGroup);
    uniformsBindGroup = nullptr;
}

void Renderer::setRenderParameters(const RenderParameters& renderParams)
{
    if (currentRenderParams != renderParams)
    {
        currentRenderParams = renderParams;
        accumulatedSampleCount = 0; // reset the temporal accumulation
    }
}

void Renderer::render(const GpuContext& gpuContext, Gui& gui)
{
    const WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(gpuContext.swapChain);
    if (!nextTexture)
    {
        // Getting the next texture can fail, if e.g. the window has been resized.
        std::fprintf(stderr, "Failed to get texture view from swap chain\n");
        return;
    }

    {
        assert(accumulatedSampleCount <= currentRenderParams.samplingParams.numSamplesPerPixel);
        const RenderParamsLayout renderParamsLayout{
            currentRenderParams.framebufferSize,
            frameCount++,
            currentRenderParams,
            accumulatedSampleCount};
        wgpuQueueWriteBuffer(
            gpuContext.queue,
            renderParamsBuffer.handle(),
            0,
            &renderParamsLayout,
            sizeof(RenderParamsLayout));
        accumulatedSampleCount = std::min(
            accumulatedSampleCount + 1, currentRenderParams.samplingParams.numSamplesPerPixel);
    }

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    wgpuCommandEncoderWriteTimestamp(encoder, querySet, 0);
    {
        const WGPURenderPassEncoder renderPassEncoder = [encoder,
                                                         nextTexture]() -> WGPURenderPassEncoder {
            const WGPURenderPassColorAttachment renderPassColorAttachment{
                .nextInChain = nullptr,
                .view = nextTexture,
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
                .timestampWriteCount = 0,
                .timestampWrites = nullptr,
            };

            return wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
        }();

        {
            wgpuRenderPassEncoderSetPipeline(renderPassEncoder, renderPipeline);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, uniformsBindGroup, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(
                renderPassEncoder, 1, renderParamsBindGroup, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 2, sceneBindGroup, 0, nullptr);
            wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 3, imageBindGroup, 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(
                renderPassEncoder, 0, vertexBuffer.handle(), 0, vertexBuffer.byteSize());

#ifdef TIMESTAMP_QUERY_INSIDE_PASSES_SUPPORTED
            wgpuRenderPassEncoderWriteTimestamp(renderPassEncoder, querySet, 2);
#endif
            wgpuRenderPassEncoderDraw(renderPassEncoder, 6, 1, 0, 0);
#if TIMESTAMP_QUERY_INSIDE_PASSES_SUPPORTED
            wgpuRenderPassEncoderWriteTimestamp(renderPassEncoder, querySet, 3);
#endif
        }

        gui.render(renderPassEncoder);

        wgpuRenderPassEncoderEnd(renderPassEncoder);
    }
    wgpuCommandEncoderWriteTimestamp(encoder, querySet, 1);

    wgpuCommandEncoderResolveQuerySet(
        encoder, querySet, 0, TimestampsLayout::QUERY_COUNT, queryBuffer.handle(), 0);
    wgpuCommandEncoderCopyBufferToBuffer(
        encoder, queryBuffer.handle(), 0, timestampBuffer.handle(), 0, sizeof(TimestampsLayout));

    const WGPUCommandBuffer cmdBuffer = [encoder]() {
        const WGPUCommandBufferDescriptor cmdBufferDesc{
            .nextInChain = nullptr,
            .label = "Renderer command buffer",
        };
        return wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    }();
    wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);

    wgpuTextureViewRelease(nextTexture);

    // Map query timers
    wgpuBufferMapAsync(
        timestampBuffer.handle(),
        WGPUMapMode_Read,
        0,
        sizeof(TimestampsLayout),
        [](const WGPUBufferMapAsyncStatus status, void* const userdata) -> void {
            if (status == WGPUBufferMapAsyncStatus_Success)
            {
                TimestampBufferMapContext& timestampBufferMapContext =
                    *reinterpret_cast<TimestampBufferMapContext*>(userdata);
                assert(timestampBufferMapContext.timestampBuffer);
                GpuBuffer&  timestampBuffer = *timestampBufferMapContext.timestampBuffer;
                const void* bufferData = wgpuBufferGetConstMappedRange(
                    timestampBuffer.handle(), 0, sizeof(TimestampsLayout));
                assert(bufferData);

                const TimestampsLayout* const timestamps =
                    reinterpret_cast<const TimestampsLayout*>(bufferData);

                assert(timestampBufferMapContext.renderPassDurationsNs);
                std::deque<std::uint64_t>& renderPassDurations =
                    *timestampBufferMapContext.renderPassDurationsNs;
                const std::uint64_t renderPassDelta =
                    timestamps->renderPassEnd - timestamps->renderPassBegin;

                renderPassDurations.push_back(renderPassDelta);
                if (renderPassDurations.size() > 30)
                {
                    renderPassDurations.pop_front();
                }

#ifdef TIMESTAMP_QUERY_INSIDE_PASSES_SUPPORTED
                assert(timestampBufferMapContext.drawDurationsNs);
                std::deque<std::uint64_t>& drawDurations =
                    *timestampBufferMapContext.drawDurationsNs;
                const std::uint64_t drawDelta = timestamps->drawEnd - timestamps->drawBegin;
                drawDurations.push_back(drawDelta);
                if (drawDurations.size() > 30)
                {
                    drawDurations.pop_front();
                }
#endif

                wgpuBufferUnmap(timestampBuffer.handle());
            }
            else
            {
                std::fprintf(stderr, "Failed to map query buffer\n");
            }
        },
        &timestampBufferMapContext);
}

float Renderer::averageDrawDurationMs() const
{
    if (drawDurationsNs.empty())
    {
        return 0.0f;
    }

    const std::uint64_t sum =
        std::accumulate(drawDurationsNs.begin(), drawDurationsNs.end(), std::uint64_t(0));
    return 0.000001f * static_cast<float>(sum) / drawDurationsNs.size();
}

float Renderer::averageRenderpassDurationMs() const
{
    if (renderPassDurationsNs.empty())
    {
        return 0.0f;
    }

    const std::uint64_t sum = std::accumulate(
        renderPassDurationsNs.begin(), renderPassDurationsNs.end(), std::uint64_t(0));
    return 0.000001f * static_cast<float>(sum) / renderPassDurationsNs.size();
}

float Renderer::renderProgressPercentage() const
{
    return 100.0f * static_cast<float>(accumulatedSampleCount) /
           static_cast<float>(currentRenderParams.samplingParams.numSamplesPerPixel);
}
} // namespace nlrs
