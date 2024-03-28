#include "gpu_context.hpp"
#include "hybrid_renderer.hpp"
#include "shader_source.hpp"
#include "webgpu_utils.hpp"
#include "window.hpp"

#include <common/assert.hpp>

#include <algorithm>
#include <array>

namespace nlrs
{
namespace
{
const WGPUTextureFormat DEPTH_TEXTURE_FORMAT = WGPUTextureFormat_Depth24Plus;
const WGPUTextureFormat ALBEDO_TEXTURE_FORMAT = WGPUTextureFormat_BGRA8Unorm;
const WGPUTextureFormat NORMAL_TEXTURE_FORMAT = WGPUTextureFormat_RGBA16Float;

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
} // namespace

HybridRenderer::HybridRenderer(
    const GpuContext&               gpuContext,
    const HybridRendererDescriptor& rendererDesc)
    : mDepthTexture(nullptr),
      mDepthTextureView(nullptr),
      mAlbedoTexture(nullptr),
      mAlbedoTextureView(nullptr),
      mNormalTexture(nullptr),
      mNormalTextureView(nullptr),
      mGbufferBindGroupLayout(),
      mGbufferBindGroup(),
      mGbufferPass(gpuContext, rendererDesc),
      mDebugPass()
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

    mGbufferBindGroupLayout = GpuBindGroupLayout{
        gpuContext.device,
        "Gbuffer bind group layout",
        std::array<WGPUBindGroupLayoutEntry, 3>{
            textureBindGroupLayoutEntry(0, WGPUTextureSampleType_UnfilterableFloat),
            textureBindGroupLayoutEntry(1, WGPUTextureSampleType_UnfilterableFloat),
            textureBindGroupLayoutEntry(2, WGPUTextureSampleType_Depth)}};

    mGbufferBindGroup = GpuBindGroup{
        gpuContext.device,
        "Gbuffer bind group",
        mGbufferBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 3>{
            textureBindGroupEntry(0, mAlbedoTextureView),
            textureBindGroupEntry(1, mNormalTextureView),
            textureBindGroupEntry(2, mDepthTextureView)}};

    mDebugPass = DebugPass{gpuContext, mGbufferBindGroupLayout, rendererDesc.framebufferSize};
}

HybridRenderer::~HybridRenderer()
{
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

void HybridRenderer::render(
    const GpuContext&     gpuContext,
    const WGPUTextureView textureView,
    const glm::mat4&      viewProjectionMat)
{
    wgpuDeviceTick(gpuContext.device);

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    mGbufferPass.render(
        gpuContext,
        viewProjectionMat,
        encoder,
        mDepthTextureView,
        mAlbedoTextureView,
        mNormalTextureView);

    mDebugPass.render(mGbufferBindGroup, encoder, textureView);

    const WGPUCommandBuffer cmdBuffer = [encoder]() {
        const WGPUCommandBufferDescriptor cmdBufferDesc{
            .nextInChain = nullptr,
            .label = "HybridRenderer command buffer",
        };
        return wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    }();
    wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);
}

void HybridRenderer::resize(const GpuContext& gpuContext, const Extent2u& newSize)
{
    NLRS_ASSERT(newSize.x > 0 && newSize.y > 0);

    mDebugPass.resize(gpuContext, newSize);

    textureViewSafeRelease(mNormalTextureView);
    textureSafeRelease(mNormalTexture);
    textureViewSafeRelease(mAlbedoTextureView);
    textureSafeRelease(mAlbedoTexture);
    textureViewSafeRelease(mDepthTextureView);
    textureSafeRelease(mDepthTexture);

    mNormalTextureView = nullptr;
    mNormalTexture = nullptr;
    mAlbedoTextureView = nullptr;
    mAlbedoTexture = nullptr;
    mDepthTextureView = nullptr;
    mDepthTexture = nullptr;

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

    mGbufferBindGroup = GpuBindGroup{
        gpuContext.device,
        "Gbuffer bind group",
        mGbufferBindGroupLayout.ptr(),
        std::array<WGPUBindGroupEntry, 3>{
            textureBindGroupEntry(0, mAlbedoTextureView),
            textureBindGroupEntry(1, mNormalTextureView),
            textureBindGroupEntry(2, mDepthTextureView)}};
}

HybridRenderer::GbufferPass::GbufferPass(
    const GpuContext&               gpuContext,
    const HybridRendererDescriptor& rendererDesc)
    : mPositionBuffers([&gpuContext, &rendererDesc]() -> std::vector<GpuBuffer> {
          std::vector<GpuBuffer> buffers;
          std::transform(
              rendererDesc.modelPositions.begin(),
              rendererDesc.modelPositions.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const glm::vec4> vertices) -> GpuBuffer {
                  return GpuBuffer(
                      gpuContext.device, "Mesh Vertex Buffer", WGPUBufferUsage_Vertex, vertices);
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
                      gpuContext.device, "Mesh normal buffer", WGPUBufferUsage_Vertex, normals};
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
                      gpuContext.device, "Mesh TexCoord Buffer", WGPUBufferUsage_Vertex, texCoords);
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
                          gpuContext.device, "Mesh Index Buffer", WGPUBufferUsage_Index, indices),
                      .count = static_cast<std::uint32_t>(indices.size()),
                      .format = WGPUIndexFormat_Uint32,
                  };
              });
          return buffers;
      }()),
      mBaseColorTextureIndices(
          rendererDesc.baseColorTextureIndices.begin(),
          rendererDesc.baseColorTextureIndices.end()),
      mBaseColorTextures([&gpuContext, &rendererDesc]() -> std::vector<GpuTexture> {
          std::vector<GpuTexture> textures;
          std::transform(
              rendererDesc.baseColorTextures.begin(),
              rendererDesc.baseColorTextures.end(),
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
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
          sizeof(glm::mat4)),
      mUniformBindGroup(),
      mSamplerBindGroup(),
      mPipeline(nullptr)
{
    const GpuBindGroupLayout uniformBindGroupLayout{
        gpuContext.device,
        "Uniform bind group layout",
        mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Vertex, sizeof(glm::mat4))};

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
        textureBindGroupLayoutEntry(0, WGPUTextureSampleType_Float)};

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
            .depthCompare = WGPUCompareFunction_Less,
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
                .code = HYBRID_RENDERER_GBUFFER_PASS_SOURCE,
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
                    .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha,
                },
            .alpha =
                WGPUBlendComponent{
                    .operation = WGPUBlendOperation_Add,
                    .srcFactor = WGPUBlendFactor_Zero,
                    .dstFactor = WGPUBlendFactor_One,
                },
        };

        const std::array<WGPUColorTargetState, 2> colorTargets{
            WGPUColorTargetState{
                .nextInChain = nullptr,
                .format = ALBEDO_TEXTURE_FORMAT,
                .blend = &blendState,
                .writeMask = WGPUColorWriteMask_All,
            },
            WGPUColorTargetState{
                .nextInChain = nullptr,
                .format = NORMAL_TEXTURE_FORMAT,
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

HybridRenderer::GbufferPass::~GbufferPass()
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

void HybridRenderer::GbufferPass::render(
    const GpuContext&        gpuContext,
    const glm::mat4&         viewProjectionMat,
    const WGPUCommandEncoder cmdEncoder,
    const WGPUTextureView    depthTextureView,
    const WGPUTextureView    albedoTextureView,
    const WGPUTextureView    normalTextureView)
{
    wgpuQueueWriteBuffer(
        gpuContext.queue, mUniformBuffer.ptr(), 0, &viewProjectionMat[0][0], sizeof(glm::mat4));

    const WGPURenderPassEncoder renderPassEncoder = [cmdEncoder,
                                                     depthTextureView,
                                                     albedoTextureView,
                                                     normalTextureView]() -> WGPURenderPassEncoder {
        const std::array<WGPURenderPassColorAttachment, 2> colorAttachments{
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
                .clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0},
            }};

        const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
            .view = depthTextureView,
            .depthLoadOp = WGPULoadOp_Clear,
            .depthStoreOp = WGPUStoreOp_Store,
            .depthClearValue = 1.0f,
            .depthReadOnly = false,
            .stencilLoadOp = WGPULoadOp_Undefined, // ops must not be set if no stencil aspect
            .stencilStoreOp = WGPUStoreOp_Undefined,
            .stencilClearValue = 0,
            .stencilReadOnly = true,
        };

        const WGPURenderPassDescriptor renderPassDesc = {
            .nextInChain = nullptr,
            .label = "Render pass encoder",
            .colorAttachmentCount = colorAttachments.size(),
            .colorAttachments = colorAttachments.data(),
            .depthStencilAttachment = &depthStencilAttachment,
            .occlusionQuerySet = nullptr,
            .timestampWrites = nullptr,
        };

        return wgpuCommandEncoderBeginRenderPass(cmdEncoder, &renderPassDesc);
    }();

    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mPipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, mUniformBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, mSamplerBindGroup.ptr(), 0, nullptr);

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

        const std::size_t   textureIdx = mBaseColorTextureIndices[idx];
        const GpuBindGroup& baseColorBindGroup = mBaseColorTextureBindGroups[textureIdx];
        wgpuRenderPassEncoderSetBindGroup(
            renderPassEncoder, 2, baseColorBindGroup.ptr(), 0, nullptr);

        wgpuRenderPassEncoderDrawIndexed(renderPassEncoder, indexBuffer.count, 1, 0, 0, 0);
    }

    wgpuRenderPassEncoderEnd(renderPassEncoder);
}

HybridRenderer::DebugPass::DebugPass(
    const GpuContext&         gpuContext,
    const GpuBindGroupLayout& gbufferBindGroupLayout,
    const Extent2u&           framebufferSize)
    : mVertexBuffer(
          gpuContext.device,
          "Vertex buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
          std::span<const float[2]>(quadVertexData)),
      mUniformBuffer(),
      mUniformBindGroup(),
      mPipeline(nullptr)
{
    {
        const auto uniformData = Extent2<float>{framebufferSize};
        mUniformBuffer = GpuBuffer{
            gpuContext.device,
            "Uniform buffer",
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            std::span<const float>(&uniformData.x, sizeof(Extent2<float>))};
    }

    const GpuBindGroupLayout uniformBindGroupLayout{
        gpuContext.device,
        "Uniform bind group layout",
        mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Fragment, sizeof(Extent2<float>))};

    mUniformBindGroup = GpuBindGroup{
        gpuContext.device,
        "Uniform bind group",
        uniformBindGroupLayout.ptr(),
        mUniformBuffer.bindGroupEntry(0)};

    {
        // Pipeline layout

        const std::array<WGPUBindGroupLayout, 2> bindGroupLayouts{
            uniformBindGroupLayout.ptr(), gbufferBindGroupLayout.ptr()};

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
                .code = HYBRID_RENDERER_DEBUG_PASS_SOURCE,
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

HybridRenderer::DebugPass::~DebugPass()
{
    renderPipelineSafeRelease(mPipeline);
    mPipeline = nullptr;
}

HybridRenderer::DebugPass::DebugPass(DebugPass&& other) noexcept
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
}

HybridRenderer::DebugPass& HybridRenderer::DebugPass::operator=(DebugPass&& other) noexcept
{
    if (this != &other)
    {
        mVertexBuffer = std::move(other.mVertexBuffer);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = std::move(other.mUniformBindGroup);
        renderPipelineSafeRelease(mPipeline);
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
    return *this;
}

void HybridRenderer::DebugPass::render(
    const GpuBindGroup&      gbufferBindGroup,
    const WGPUCommandEncoder cmdEncoder,
    const WGPUTextureView    textureView)
{
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
    wgpuRenderPassEncoderSetBindGroup(renderPass, 1, gbufferBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(
        renderPass, 0, mVertexBuffer.ptr(), 0, mVertexBuffer.byteSize());
    wgpuRenderPassEncoderDraw(renderPass, 6, 1, 0, 0);

    wgpuRenderPassEncoderEnd(renderPass);
}

void HybridRenderer::DebugPass::resize(const GpuContext& gpuContext, const Extent2u& newSize)
{
    const auto uniformData = Extent2<float>{newSize};
    wgpuQueueWriteBuffer(
        gpuContext.queue, mUniformBuffer.ptr(), 0, &uniformData.x, sizeof(Extent2<float>));
}
} // namespace nlrs
