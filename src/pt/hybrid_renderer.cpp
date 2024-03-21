#include "gpu_bind_group_layout.hpp"
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
HybridRenderer::HybridRenderer(const GpuContext& gpuContext, HybridRendererDescriptor desc)
    : mPositionBuffers([&gpuContext, &desc]() -> std::vector<GpuBuffer> {
          std::vector<GpuBuffer> buffers;
          std::transform(
              desc.modelPositions.begin(),
              desc.modelPositions.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const glm::vec4> vertices) {
                  return GpuBuffer(
                      gpuContext.device, "Mesh Vertex Buffer", WGPUBufferUsage_Vertex, vertices);
              });
          return buffers;
      }()),
      mTexCoordBuffers([&gpuContext, &desc]() -> std::vector<GpuBuffer> {
          std::vector<GpuBuffer> buffers;
          std::transform(
              desc.modelTexCoords.begin(),
              desc.modelTexCoords.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const glm::vec2> texCoords) {
                  return GpuBuffer(
                      gpuContext.device, "Mesh TexCoord Buffer", WGPUBufferUsage_Vertex, texCoords);
              });
          return buffers;
      }()),
      mIndexBuffers([&gpuContext, &desc]() -> std::vector<IndexBuffer> {
          std::vector<IndexBuffer> buffers;
          std::transform(
              desc.modelIndices.begin(),
              desc.modelIndices.end(),
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
          desc.baseColorTextureIndices.begin(),
          desc.baseColorTextureIndices.end()),
      mBaseColorTextures([&gpuContext, &desc]() -> std::vector<GpuTexture> {
          std::vector<GpuTexture> textures;
          std::transform(
              desc.baseColorTextures.begin(),
              desc.baseColorTextures.end(),
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
      mSampler(nullptr),
      mUniformBuffer(
          gpuContext.device,
          "Uniform buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
          sizeof(glm::mat4)),
      mUniformBindGroup(),
      mSamplerBindGroup(),
      mDepthTexture(nullptr),
      mDepthTextureView(nullptr),
      mGbufferPipeline(nullptr)
{
    NLRS_ASSERT(mPositionBuffers.size() == mIndexBuffers.size());

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
        mSampler = wgpuDeviceCreateSampler(gpuContext.device, &samplerDesc);
        NLRS_ASSERT(mSampler != nullptr);
    }

    const GpuBindGroupLayout samplerBindGroupLayout{
        gpuContext.device, "Sampler bind group layout", samplerBindGroupLayoutEntry(0)};

    mSamplerBindGroup = GpuBindGroup{
        gpuContext.device,
        "Sampler bind group",
        samplerBindGroupLayout.ptr(),
        samplerBindGroupEntry(0, mSampler)};

    const GpuBindGroupLayout textureBindGroupLayout{
        gpuContext.device,
        "Texture bind group layout",
        textureBindGroupLayoutEntry(0),
    };

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

    const GpuBindGroupLayout uniformBindGroupLayout{
        gpuContext.device,
        "Uniform bind group layout",
        mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Vertex, sizeof(glm::mat4))};

    mUniformBindGroup = GpuBindGroup{
        gpuContext.device,
        "Uniform bind group",
        uniformBindGroupLayout.ptr(),
        mUniformBuffer.bindGroupEntry(0)};

    constexpr WGPUTextureFormat DEPTH_TEXTURE_FORMAT = WGPUTextureFormat_Depth24Plus;
    {
        const std::array<WGPUTextureFormat, 1> depthFormats{
            DEPTH_TEXTURE_FORMAT,
        };
        const WGPUTextureDescriptor depthTextureDesc{
            .nextInChain = nullptr,
            .label = "Depth texture",
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {desc.framebufferSize.x, desc.framebufferSize.y, 1},
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

    // Render pipeline
    {
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

        const WGPUVertexAttribute texCoordAttribute{
            .format = WGPUVertexFormat_Float32x2,
            .offset = 0,
            .shaderLocation = 1,
        };

        const WGPUVertexBufferLayout texCoordBufferLayout{
            .arrayStride = sizeof(glm::vec2),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = 1,
            .attributes = &texCoordAttribute,
        };

        const std::array<WGPUVertexBufferLayout, 2> vertexBufferLayouts{
            positionBufferLayout,
            texCoordBufferLayout,
        };

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

        // Shader modules

        const WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
            .chain =
                WGPUChainedStruct{
                    .next = nullptr,
                    .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                },
            .code = HYBRID_RENDERER_SOURCE,
        };

        const WGPUShaderModuleDescriptor shaderDesc{
            .nextInChain = &shaderCodeDesc.chain,
            .label = "Hybrid renderer shader",
        };

        const WGPUShaderModule shaderModule =
            wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);

        const WGPUColorTargetState colorTarget{
            .nextInChain = nullptr,
            .format = Window::SWAP_CHAIN_FORMAT,
            .blend = &blendState,
            // We could write to only some of the color channels.
            .writeMask = WGPUColorWriteMask_All,
        };

        const WGPUFragmentState fragmentState{
            .nextInChain = nullptr,
            .module = shaderModule,
            .entryPoint = "fsMain",
            .constantCount = 0,
            .constants = nullptr,
            .targetCount = 1,
            .targets = &colorTarget,
        };

        // Depth Stencil

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
            .depthStencil = &depthStencilState,
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

        mGbufferPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

        wgpuPipelineLayoutRelease(pipelineLayout);
    }

    NLRS_ASSERT(mPositionBuffers.size() == mIndexBuffers.size());
    NLRS_ASSERT(mPositionBuffers.size() == mTexCoordBuffers.size());
    NLRS_ASSERT(mPositionBuffers.size() == mBaseColorTextureIndices.size());
}

HybridRenderer::~HybridRenderer()
{
    renderPipelineSafeRelease(mGbufferPipeline);
    mGbufferPipeline = nullptr;
    textureViewSafeRelease(mDepthTextureView);
    mDepthTextureView = nullptr;
    textureSafeRelease(mDepthTexture);
    mDepthTexture = nullptr;
    samplerSafeRelease(mSampler);
    mSampler = nullptr;
    for (const auto& texture : mBaseColorTextures)
    {
        textureSafeRelease(texture.texture);
        textureViewSafeRelease(texture.view);
    }
    mBaseColorTextures.clear();
}

void HybridRenderer::render(
    const GpuContext&     gpuContext,
    const WGPUTextureView textureView,
    const glm::mat4&      viewProjectionMat)
{
    wgpuDeviceTick(gpuContext.device);

    {
        wgpuQueueWriteBuffer(
            gpuContext.queue, mUniformBuffer.handle(), 0, &viewProjectionMat[0], sizeof(glm::mat4));
    }

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    const WGPURenderPassEncoder renderPassEncoder =
        [encoder, textureView, this]() -> WGPURenderPassEncoder {
        const WGPURenderPassColorAttachment colorAttachment{
            .nextInChain = nullptr,
            .view = textureView,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // depthSlice must be initialized with
                                                      // 'undefined' value for 2d color attachments.
            .resolveTarget = nullptr,
            .loadOp = WGPULoadOp_Clear,
            .storeOp = WGPUStoreOp_Store,
            .clearValue = WGPUColor{0.0, 0.0, 0.0, 1.0},
        };

        const WGPURenderPassDepthStencilAttachment depthStencilAttachment{
            .view = mDepthTextureView,
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
            .colorAttachmentCount = 1,
            .colorAttachments = &colorAttachment,
            .depthStencilAttachment = &depthStencilAttachment,
            .occlusionQuerySet = nullptr,
            .timestampWrites = nullptr,
        };

        return wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);
    }();

    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mGbufferPipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, mUniformBindGroup.ptr(), 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, mSamplerBindGroup.ptr(), 0, nullptr);

    for (std::size_t idx = 0; idx < mPositionBuffers.size(); ++idx)
    {
        const GpuBuffer& positionBuffer = mPositionBuffers[idx];
        const GpuBuffer& texCoordBuffer = mTexCoordBuffers[idx];
        wgpuRenderPassEncoderSetVertexBuffer(
            renderPassEncoder, 0, positionBuffer.handle(), 0, positionBuffer.byteSize());
        wgpuRenderPassEncoderSetVertexBuffer(
            renderPassEncoder, 1, texCoordBuffer.handle(), 0, texCoordBuffer.byteSize());

        const IndexBuffer& indexBuffer = mIndexBuffers[idx];
        wgpuRenderPassEncoderSetIndexBuffer(
            renderPassEncoder,
            indexBuffer.buffer.handle(),
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

    textureViewSafeRelease(mDepthTextureView);
    textureSafeRelease(mDepthTexture);

    mDepthTextureView = nullptr;
    mDepthTexture = nullptr;

    constexpr WGPUTextureFormat DEPTH_TEXTURE_FORMAT = WGPUTextureFormat_Depth24Plus;
    {
        const std::array<WGPUTextureFormat, 1> depthFormats{
            DEPTH_TEXTURE_FORMAT,
        };
        const WGPUTextureDescriptor depthTextureDesc{
            .nextInChain = nullptr,
            .label = "Depth texture",
            .usage = WGPUTextureUsage_RenderAttachment,
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
    }

    {
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
}
} // namespace nlrs
