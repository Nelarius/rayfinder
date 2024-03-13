#include "gpu_context.hpp"
#include "hybrid_renderer.hpp"
#include "window.hpp"

#include <common/assert.hpp>

#include <algorithm>
#include <array>

namespace nlrs
{
HybridRenderer::HybridRenderer(const GpuContext& gpuContext, HybridRendererSceneDescriptor desc)
    : mVertexBuffers([&gpuContext, &desc]() -> std::vector<GpuBuffer> {
          std::vector<GpuBuffer> buffers;
          std::transform(
              desc.meshVertices.begin(),
              desc.meshVertices.end(),
              std::back_inserter(buffers),
              [&gpuContext](const std::span<const glm::vec4> vertices) {
                  return GpuBuffer(
                      gpuContext.device, "Mesh Vertex Buffer", WGPUBufferUsage_Vertex, vertices);
              });
          return buffers;
      }()),
      mIndexBuffers([&gpuContext, &desc]() -> std::vector<IndexBuffer> {
          std::vector<IndexBuffer> buffers;
          std::transform(
              desc.meshIndices.begin(),
              desc.meshIndices.end(),
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
      mUniformBuffer(
          gpuContext.device,
          "Uniform buffer",
          WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
          sizeof(glm::mat4)),
      mUniformBindGroup(nullptr),
      mPipeline(nullptr)
{
    NLRS_ASSERT(mVertexBuffers.size() == mIndexBuffers.size());

    const WGPUBindGroupLayout uniformBindGroupLayout = [this,
                                                        &gpuContext]() -> WGPUBindGroupLayout {
        const WGPUBindGroupLayoutEntry uniformsBindGroupLayoutEntry =
            mUniformBuffer.bindGroupLayoutEntry(0, WGPUShaderStage_Vertex, sizeof(glm::mat4));

        const WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "Uniform bind group layout",
            .entryCount = 1,
            .entries = &uniformsBindGroupLayoutEntry,
        };

        return wgpuDeviceCreateBindGroupLayout(gpuContext.device, &bindGroupLayoutDesc);
    }();

    {
        const WGPUBindGroupEntry uniformsBindGroupEntry = mUniformBuffer.bindGroupEntry(0);

        const WGPUBindGroupDescriptor bindGroupDesc{
            .nextInChain = nullptr,
            .label = "Uniform bind group",
            .layout = uniformBindGroupLayout,
            .entryCount = 1,
            .entries = &uniformsBindGroupEntry,
        };

        mUniformBindGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &bindGroupDesc);
    }

    // Render pipeline
    {
        // Vertex layout

        const std::array<WGPUVertexAttribute, 1> vertexAttributes{
            WGPUVertexAttribute{
                .format = WGPUVertexFormat_Float32x4,
                .offset = 0,
                .shaderLocation = 0,
            },
        };

        const WGPUVertexBufferLayout vertexBufferLayout{
            .arrayStride = sizeof(glm::vec4),
            .stepMode = WGPUVertexStepMode_Vertex,
            .attributeCount = vertexAttributes.size(),
            .attributes = vertexAttributes.data(),
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

        const std::string shaderSource = loadShaderSource("hybrid_renderer.wgsl");

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

        // pipeline layout

        const std::array<WGPUBindGroupLayout, 1> bindGroupLayouts{
            uniformBindGroupLayout,
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

        mPipeline = wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

        wgpuPipelineLayoutRelease(pipelineLayout);
    }
}

HybridRenderer::~HybridRenderer()
{
    renderPipelineSafeRelease(mPipeline);
    bindGroupSafeRelease(mUniformBindGroup);
}

HybridRenderer::HybridRenderer(HybridRenderer&& other)
    : mVertexBuffers()
{
    if (this != &other)
    {
        mVertexBuffers = std::move(other.mVertexBuffers);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = other.mUniformBindGroup;
        other.mUniformBindGroup = nullptr;
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
}

HybridRenderer& HybridRenderer::operator=(HybridRenderer&& other)
{
    if (this != &other)
    {
        mVertexBuffers = std::move(other.mVertexBuffers);
        mUniformBuffer = std::move(other.mUniformBuffer);
        mUniformBindGroup = other.mUniformBindGroup;
        other.mUniformBindGroup = nullptr;
        mPipeline = other.mPipeline;
        other.mPipeline = nullptr;
    }
    return *this;
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

    const WGPURenderPassEncoder renderPassEncoder = [encoder,
                                                     textureView]() -> WGPURenderPassEncoder {
        const WGPURenderPassColorAttachment renderPassColorAttachment{
            .nextInChain = nullptr,
            .view = textureView,
            .depthSlice = WGPU_DEPTH_SLICE_UNDEFINED, // depthSlice must be initialized with
                                                      // 'undefined' value for 2d color attachments.
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

    wgpuRenderPassEncoderSetPipeline(renderPassEncoder, mPipeline);
    wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, mUniformBindGroup, 0, nullptr);

    for (std::size_t idx = 0; idx < mVertexBuffers.size(); ++idx)
    {
        const GpuBuffer& vertexBuffer = mVertexBuffers[idx];
        wgpuRenderPassEncoderSetVertexBuffer(
            renderPassEncoder, 0, vertexBuffer.handle(), 0, vertexBuffer.byteSize());
        const IndexBuffer& indexBuffer = mIndexBuffers[idx];
        wgpuRenderPassEncoderSetIndexBuffer(
            renderPassEncoder,
            indexBuffer.buffer.handle(),
            indexBuffer.format,
            0,
            indexBuffer.buffer.byteSize());
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
} // namespace nlrs
