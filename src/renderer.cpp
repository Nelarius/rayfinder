#include "common/framebuffer_size.hpp"
#include "gpu_context.hpp"
#include "renderer.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <tuple>

namespace pt
{
namespace
{
struct Vertex
{
    float position[2];
    float uv[2];
};

void bufferSafeRelease(const WGPUBuffer buffer)
{
    if (buffer)
    {
        wgpuBufferDestroy(buffer);
        wgpuBufferRelease(buffer);
    }
}

void bindGroupSafeRelease(const WGPUBindGroup bindGroup)
{
    if (bindGroup)
    {
        wgpuBindGroupRelease(bindGroup);
    }
}

void pipelineSafeRelease(const WGPURenderPipeline pipeline)
{
    if (pipeline)
    {
        wgpuRenderPipelineRelease(pipeline);
    }
}
} // namespace

Renderer::Renderer(const GpuContext& gpuContext)
    : vertexBuffer(nullptr), vertexBufferByteSize(0), uniformsBuffer(nullptr),
      uniformsBindGroup(nullptr), renderPipeline(nullptr)
{
    auto [buffer, byteSize] = [&gpuContext]() -> std::tuple<WGPUBuffer, std::size_t> {
        const std::array<Vertex, 6> vertexData{
            Vertex{{-0.5f, -0.5f}, {0.0f, 0.0f}},
            Vertex{{0.5f, -0.5f}, {1.0f, 0.0f}},
            Vertex{{0.5f, 0.5f}, {1.0f, 1.0f}},
            Vertex{{0.5f, 0.5f}, {1.0f, 1.0f}},
            Vertex{{-0.5f, 0.5f}, {0.0f, 1.0f}},
            Vertex{{-0.5f, -0.5f}, {0.0f, 0.0f}},
        };
        const std::size_t vertexDataByteSize = sizeof(Vertex) * vertexData.size();
        assert(vertexDataByteSize <= std::numeric_limits<std::uint64_t>::max());

        const WGPUBufferDescriptor vertexBufferDesc{
            .nextInChain = nullptr,
            .label = "Vertex buffer",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            .size = static_cast<std::uint64_t>(vertexDataByteSize),
            .mappedAtCreation = false,
        };
        const WGPUBuffer vertexBuffer =
            wgpuDeviceCreateBuffer(gpuContext.device, &vertexBufferDesc);
        wgpuQueueWriteBuffer(
            gpuContext.queue, vertexBuffer, 0, vertexData.data(), vertexDataByteSize);

        return std::make_tuple(vertexBuffer, vertexDataByteSize);
    }();
    vertexBuffer = buffer;
    vertexBufferByteSize = byteSize;

    uniformsBuffer = [&gpuContext]() -> WGPUBuffer {
        const WGPUBufferDescriptor uniformDesc{
            .nextInChain = nullptr,
            .label = "Uniform buffer",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            .size = sizeof(glm::mat4),
            .mappedAtCreation = false,
        };
        const WGPUBuffer buffer = wgpuDeviceCreateBuffer(gpuContext.device, &uniformDesc);

        {
            // DirectX, Metal, wgpu share the same left-handed coordinate system
            // for their normalized device coordinates:
            // https://github.com/gfx-rs/gfx/tree/master/src/backend/dx12
            glm::mat4 viewProjectionMatrix = glm::orthoLH(-0.5f, 0.5f, -0.5f, 0.5f, -1.f, 1.f);
            wgpuQueueWriteBuffer(
                gpuContext.queue,
                buffer,
                0,
                &viewProjectionMatrix[0],
                sizeof(viewProjectionMatrix));
        }

        return buffer;
    }();

    auto [bindGroup, pipeline] = [&gpuContext,
                                  this]() -> std::tuple<WGPUBindGroup, WGPURenderPipeline> {
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

        const char* const shaderSource = R"(
        struct Uniforms {
            viewProjectionMatrix: mat4x4<f32>,
        }

        @group(0) @binding(0) var<uniform> uniforms: Uniforms;

        struct VertexInput {
            @location(0) position: vec2f,
            @location(1) texCoord: vec2f,
        }

        struct VertexOutput {
            @builtin(position) position: vec4f,
            @location(0) texCoord: vec2f,
        }

        @vertex
        fn vsMain(in: VertexInput) -> VertexOutput {
            var out: VertexOutput;
            out.position = uniforms.viewProjectionMatrix * vec4f(in.position, 0.0, 1.0);
            out.texCoord = in.texCoord;

            return out;
        }

        @fragment
        fn fsMain(in: VertexOutput) -> @location(0) vec4<f32> {
            return vec4f(in.texCoord, 0.0, 1.0);
        }
        )";

        const WGPUShaderModuleWGSLDescriptor shaderCodeDesc = {
            .chain =
                WGPUChainedStruct{
                    .next = nullptr,
                    .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                },
            .code = shaderSource,
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

        // Bind group layout

        const WGPUBindGroupLayoutEntry uniformsGroupLayoutEntry{
            .nextInChain = nullptr,
            .binding = 0, // binding index used in the @binding attribute
            .visibility = WGPUShaderStage_Vertex,
            .buffer =
                WGPUBufferBindingLayout{
                    .nextInChain = nullptr,
                    .type = WGPUBufferBindingType_Uniform,
                    .hasDynamicOffset = false,
                    .minBindingSize = static_cast<std::uint64_t>(sizeof(glm::mat4))},
            .sampler =
                WGPUSamplerBindingLayout{
                    .nextInChain = nullptr,
                    .type = WGPUSamplerBindingType_Undefined,
                },
            .texture =
                WGPUTextureBindingLayout{
                    .nextInChain = nullptr,
                    .sampleType = WGPUTextureSampleType_Undefined,
                    .viewDimension = WGPUTextureViewDimension_Undefined,
                    .multisampled = false,
                },
            .storageTexture =
                WGPUStorageTextureBindingLayout{
                    .nextInChain = nullptr,
                    .access = WGPUStorageTextureAccess_Undefined,
                    .format = WGPUTextureFormat_Undefined,
                    .viewDimension = WGPUTextureViewDimension_Undefined,
                },
        };

        const WGPUBindGroupLayoutDescriptor uniformsGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "uniforms group layout",
            .entryCount = 1,
            .entries = &uniformsGroupLayoutEntry,
        };
        const WGPUBindGroupLayout uniformsGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &uniformsGroupLayoutDesc);

        const WGPUPipelineLayoutDescriptor pipelineLayoutDesc{
            .nextInChain = nullptr,
            .label = "Pipeline layout",
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &uniformsGroupLayout,
        };
        const WGPUPipelineLayout pipelineLayout =
            wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);

        // Bind group

        const WGPUBindGroupEntry uniformsGroupEntry{
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = uniformsBuffer,
            .offset = 0,
            .size = static_cast<std::uint64_t>(sizeof(glm::mat4)),
            .sampler = nullptr,
            .textureView = nullptr,
        };

        const WGPUBindGroupDescriptor uniformsGroupDesc{
            .nextInChain = nullptr,
            .label = "Bind group",
            .layout = uniformsGroupLayout,
            .entryCount = 1,
            .entries = &uniformsGroupEntry,
        };
        const WGPUBindGroup uniformsGroup =
            wgpuDeviceCreateBindGroup(gpuContext.device, &uniformsGroupDesc);

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

        const WGPURenderPipeline renderPipeline =
            wgpuDeviceCreateRenderPipeline(gpuContext.device, &pipelineDesc);

        return std::make_tuple(uniformsGroup, renderPipeline);
    }();
    uniformsBindGroup = bindGroup;
    renderPipeline = pipeline;
}

Renderer::~Renderer()
{
    pipelineSafeRelease(renderPipeline);
    bindGroupSafeRelease(uniformsBindGroup);
    bufferSafeRelease(uniformsBuffer);
    bufferSafeRelease(vertexBuffer);
}

void Renderer::render(const GpuContext& gpuContext)
{
    const WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(gpuContext.swapChain);
    if (!nextTexture)
    {
        // Getting the next texture can fail, if e.g. the window has been resized.
        std::fprintf(stderr, "Failed to get texture view from swap chain\n");
        return;
    }

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    const WGPURenderPassEncoder renderPass = [encoder, nextTexture]() -> WGPURenderPassEncoder {
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
        wgpuRenderPassEncoderSetPipeline(renderPass, renderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, uniformsBindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, vertexBuffer, 0, vertexBufferByteSize);
        wgpuRenderPassEncoderDraw(renderPass, 6, 1, 0, 0);
    }

    wgpuRenderPassEncoderEnd(renderPass);

    const WGPUCommandBuffer cmdBuffer = [encoder]() {
        const WGPUCommandBufferDescriptor cmdBufferDesc{
            .nextInChain = nullptr,
            .label = "Renderer command buffer",
        };
        return wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    }();
    wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);

    wgpuTextureViewRelease(nextTexture);
}
} // namespace pt
