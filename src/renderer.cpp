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

struct FrameDataBuffer
{
    Extent2u      dimensions;
    std::uint32_t frameCount;
    std::uint32_t padding;
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

void computePipelineSafeRelease(const WGPUComputePipeline pipeline)
{
    if (pipeline)
    {
        wgpuComputePipelineRelease(pipeline);
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

Renderer::Renderer(const RendererDescriptor& rendererDesc, const GpuContext& gpuContext)
    : frameDataBuffer(nullptr),
      pixelBuffer(nullptr),
      computePixelsBindGroup(nullptr),
      computePipeline(nullptr),
      vertexBuffer(nullptr),
      vertexBufferByteSize(0),
      uniformsBuffer(nullptr),
      uniformsBindGroup(nullptr),
      renderPixelsBindGroup(nullptr),
      renderPipeline(nullptr),
      currentFramebufferSize(rendererDesc.currentFramebufferSize),
      frameCount(0)
{
    const Extent2i    largestResolution = rendererDesc.maxFramebufferSize;
    const std::size_t numPixels =
        static_cast<std::size_t>(largestResolution.x * largestResolution.y);
    const std::size_t pixelBufferNumBytes = sizeof(glm::vec3) * numPixels;

    {
        const WGPUBufferDescriptor frameDataBufferDesc{
            .nextInChain = nullptr,
            .label = "Image dimensions buffer",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            .size = sizeof(FrameDataBuffer),
            .mappedAtCreation = false,
        };
        frameDataBuffer = wgpuDeviceCreateBuffer(gpuContext.device, &frameDataBufferDesc);

        const WGPUBufferDescriptor pixelBufferDesc{
            .nextInChain = nullptr,
            .label = "Image buffer",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Storage,
            .size = pixelBufferNumBytes,
            .mappedAtCreation = false,
        };
        pixelBuffer = wgpuDeviceCreateBuffer(gpuContext.device, &pixelBufferDesc);
    }

    {
        // Shader module

        const char* const computeSource = R"(
        struct FrameData {
            dimensions: vec2<u32>,
            frameCount: u32,
        }

        @group(0) @binding(0) var<uniform> frameData: FrameData;
        @group(0) @binding(1) var<storage, read_write> pixelBuffer: array<vec3<f32>>;

        @compute @workgroup_size(8,8)
        fn main(@builtin(global_invocation_id) globalId: vec3<u32>) {
            let j = globalId.x;
            let i = globalId.y;

            var rngState = initRng(vec2(j, i), frameData.dimensions, frameData.frameCount);
            let v = rngNextFloat(&rngState);

            let r = rngNextFloat(&rngState);
            let g = rngNextFloat(&rngState);
            let b = rngNextFloat(&rngState);

            if j < frameData.dimensions.x && i < frameData.dimensions[1] {
                let idx = frameData.dimensions.x * i + j;
                pixelBuffer[idx] = vec3(r, g, b);
            }
        }

        fn initRng(pixel: vec2<u32>, resolution: vec2<u32>, frame: u32) -> u32 {
            // Adapted from https://github.com/boksajak/referencePT
            let seed = dot(pixel, vec2<u32>(1u, resolution.x)) ^ jenkinsHash(frame);
            return jenkinsHash(seed);
        }

        fn jenkinsHash(input: u32) -> u32 {
            var x = input;
            x += x << 10u;
            x ^= x >> 6u;
            x += x << 3u;
            x ^= x >> 11u;
            x += x << 15u;
            return x;
        }

        fn rngNextFloat(state: ptr<function, u32>) -> f32 {
            rngNextInt(state);
            return f32(*state) / f32(0xffffffffu);
        }

        fn rngNextInt(state: ptr<function, u32>) {
            // PCG random number generator
            // Based on https://www.shadertoy.com/view/XlGcRh

            let oldState = *state + 747796405u + 2891336453u;
            let word = ((oldState >> ((oldState >> 28u) + 4u)) ^ oldState) * 277803737u;
            *state = (word >> 22u) ^ word;
        }
        )";

        const WGPUShaderModuleWGSLDescriptor shaderWgslDesc = {
            .chain =
                WGPUChainedStruct{
                    .next = nullptr,
                    .sType = WGPUSType_ShaderModuleWGSLDescriptor,
                },
            .code = computeSource,
        };

        const WGPUShaderModuleDescriptor shaderDesc{
            .nextInChain = &shaderWgslDesc.chain,
            .label = "Compute shader module",
        };

        const WGPUShaderModule computeModule =
            wgpuDeviceCreateShaderModule(gpuContext.device, &shaderDesc);

        // pixels bind group layout

        assert(pixelBufferNumBytes < std::numeric_limits<std::uint64_t>::max());

        std::array<WGPUBindGroupLayoutEntry, 2> pixelsBindGroupLayoutEntries{
            WGPUBindGroupLayoutEntry{
                .nextInChain = nullptr,
                .binding = 0,
                .visibility = WGPUShaderStage_Compute,
                .buffer =
                    WGPUBufferBindingLayout{
                        .nextInChain = nullptr,
                        .type = WGPUBufferBindingType_Uniform,
                        .hasDynamicOffset = false,
                        .minBindingSize = static_cast<std::uint64_t>(sizeof(FrameDataBuffer))},
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
            },
            WGPUBindGroupLayoutEntry{
                WGPUBindGroupLayoutEntry{
                    .nextInChain = nullptr,
                    .binding = 1, // binding index used in the @binding attribute
                    .visibility = WGPUShaderStage_Compute,
                    .buffer =
                        WGPUBufferBindingLayout{
                            .nextInChain = nullptr,
                            .type = WGPUBufferBindingType_Storage,
                            .hasDynamicOffset = false,
                            .minBindingSize = static_cast<std::uint64_t>(pixelBufferNumBytes)},
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
                },
            },
        };

        const WGPUBindGroupLayoutDescriptor pixelsBindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "images group layout (compute pipeline)",
            .entryCount = pixelsBindGroupLayoutEntries.size(),
            .entries = pixelsBindGroupLayoutEntries.data(),
        };
        const WGPUBindGroupLayout pixelsBindGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &pixelsBindGroupLayoutDesc);

        // Bind group

        std::array<WGPUBindGroupEntry, 2> pixelsBindGroupEntries{
            WGPUBindGroupEntry{
                .nextInChain = nullptr,
                .binding = 0,
                .buffer = frameDataBuffer,
                .offset = 0,
                .size = sizeof(FrameDataBuffer),
                .sampler = nullptr,
                .textureView = nullptr,
            },
            WGPUBindGroupEntry{
                .nextInChain = nullptr,
                .binding = 1,
                .buffer = pixelBuffer,
                .offset = 0,
                .size = static_cast<std::uint64_t>(pixelBufferNumBytes),
                .sampler = nullptr,
                .textureView = nullptr,
            },
        };

        const WGPUBindGroupDescriptor pixelsBindGroupDesc{
            .nextInChain = nullptr,
            .label = "image bind group",
            .layout = pixelsBindGroupLayout,
            .entryCount = pixelsBindGroupEntries.size(),
            .entries = pixelsBindGroupEntries.data(),
        };
        computePixelsBindGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &pixelsBindGroupDesc);

        // Pipeline layout

        const WGPUPipelineLayoutDescriptor pipelineLayoutDesc{
            .nextInChain = nullptr,
            .label = "Compute pipeline layout",
            .bindGroupLayoutCount = 1,
            .bindGroupLayouts = &pixelsBindGroupLayout,
        };
        const WGPUPipelineLayout pipelineLayout =
            wgpuDeviceCreatePipelineLayout(gpuContext.device, &pipelineLayoutDesc);

        const WGPUComputePipelineDescriptor computeDesc{
            .nextInChain = nullptr,
            .label = "Compute pipeline",
            .layout = pipelineLayout,
            .compute =
                WGPUProgrammableStageDescriptor{
                    .nextInChain = nullptr,
                    .module = computeModule,
                    .entryPoint = "main",
                    .constantCount = 0,
                    .constants = nullptr,
                },
        };
        computePipeline = wgpuDeviceCreateComputePipeline(gpuContext.device, &computeDesc);
    }

    {
        const std::array<Vertex, 6> vertexData{
            Vertex{{-0.5f, -0.5f}, {0.0f, 0.0f}},
            Vertex{{0.5f, -0.5f}, {1.0f, 0.0f}},
            Vertex{{0.5f, 0.5f}, {1.0f, 1.0f}},
            Vertex{{0.5f, 0.5f}, {1.0f, 1.0f}},
            Vertex{{-0.5f, 0.5f}, {0.0f, 1.0f}},
            Vertex{{-0.5f, -0.5f}, {0.0f, 0.0f}},
        };
        vertexBufferByteSize = sizeof(Vertex) * vertexData.size();
        assert(vertexBufferByteSize <= std::numeric_limits<std::uint64_t>::max());

        const WGPUBufferDescriptor vertexBufferDesc{
            .nextInChain = nullptr,
            .label = "Vertex buffer",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex,
            .size = static_cast<std::uint64_t>(vertexBufferByteSize),
            .mappedAtCreation = false,
        };
        vertexBuffer = wgpuDeviceCreateBuffer(gpuContext.device, &vertexBufferDesc);
        wgpuQueueWriteBuffer(
            gpuContext.queue, vertexBuffer, 0, vertexData.data(), vertexBufferByteSize);
    }

    {
        const WGPUBufferDescriptor uniformDesc{
            .nextInChain = nullptr,
            .label = "Uniform buffer",
            .usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_Uniform,
            .size = sizeof(glm::mat4),
            .mappedAtCreation = false,
        };
        uniformsBuffer = wgpuDeviceCreateBuffer(gpuContext.device, &uniformDesc);

        {
            // DirectX, Metal, wgpu share the same left-handed coordinate system
            // for their normalized device coordinates:
            // https://github.com/gfx-rs/gfx/tree/master/src/backend/dx12
            glm::mat4 viewProjectionMatrix = glm::orthoLH(-0.5f, 0.5f, -0.5f, 0.5f, -1.f, 1.f);
            wgpuQueueWriteBuffer(
                gpuContext.queue,
                uniformsBuffer,
                0,
                &viewProjectionMatrix[0],
                sizeof(viewProjectionMatrix));
        }
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

        const char* const shaderSource = R"(
        struct Uniforms {
            viewProjectionMatrix: mat4x4<f32>,
        }

        @group(0) @binding(0) var<uniform> uniforms: Uniforms;

        struct FrameData {
            dimensions: vec2<u32>,
            frameCount: u32,
        }

        @group(1) @binding(0) var<uniform> frameData: FrameData;
        @group(1) @binding(1) var<storage, read_write> pixelBuffer: array<vec3<f32>>;

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
            let u = in.texCoord.x;
            let v = in.texCoord.y;

            let j =  u32(u * f32(frameData.dimensions.x));
            let i =  u32(v * f32(frameData.dimensions.y));
            let idx = frameData.dimensions.x * i + j;
            let rgb = pixelBuffer[idx];

            return vec4f(rgb, 1.0);
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

        // uniforms bind group layout

        const WGPUBindGroupLayoutEntry uniformsBindGroupLayoutEntry{
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

        const WGPUBindGroupLayoutDescriptor uniformsBindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "uniforms group layout",
            .entryCount = 1,
            .entries = &uniformsBindGroupLayoutEntry,
        };
        const WGPUBindGroupLayout uniformsBindGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &uniformsBindGroupLayoutDesc);

        // images bind group layout

        std::array<WGPUBindGroupLayoutEntry, 2> pixelsBindGroupLayoutEntries{
            WGPUBindGroupLayoutEntry{
                .nextInChain = nullptr,
                .binding = 0,
                .visibility = WGPUShaderStage_Fragment,
                .buffer =
                    WGPUBufferBindingLayout{
                        .nextInChain = nullptr,
                        .type = WGPUBufferBindingType_Uniform,
                        .hasDynamicOffset = false,
                        .minBindingSize = static_cast<std::uint64_t>(sizeof(FrameDataBuffer))},
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
            },
            WGPUBindGroupLayoutEntry{
                WGPUBindGroupLayoutEntry{
                    .nextInChain = nullptr,
                    .binding = 1,
                    .visibility = WGPUShaderStage_Fragment,
                    .buffer =
                        WGPUBufferBindingLayout{
                            .nextInChain = nullptr,
                            .type = WGPUBufferBindingType_Storage,
                            .hasDynamicOffset = false,
                            .minBindingSize = static_cast<std::uint64_t>(pixelBufferNumBytes)},
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
                },
            },
        };

        const WGPUBindGroupLayoutDescriptor pixelsBindGroupLayoutDesc{
            .nextInChain = nullptr,
            .label = "images group layout (render pipeline)",
            .entryCount = pixelsBindGroupLayoutEntries.size(),
            .entries = pixelsBindGroupLayoutEntries.data(),
        };
        const WGPUBindGroupLayout pixelsBindGroupLayout =
            wgpuDeviceCreateBindGroupLayout(gpuContext.device, &pixelsBindGroupLayoutDesc);

        std::array<WGPUBindGroupLayout, 2> bindGroupLayouts{
            uniformsBindGroupLayout,
            pixelsBindGroupLayout,
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

        const WGPUBindGroupEntry uniformsBindGroupEntry{
            .nextInChain = nullptr,
            .binding = 0,
            .buffer = uniformsBuffer,
            .offset = 0,
            .size = static_cast<std::uint64_t>(sizeof(glm::mat4)),
            .sampler = nullptr,
            .textureView = nullptr,
        };

        const WGPUBindGroupDescriptor uniformsBindGroupDesc{
            .nextInChain = nullptr,
            .label = "Bind group",
            .layout = uniformsBindGroupLayout,
            .entryCount = 1,
            .entries = &uniformsBindGroupEntry,
        };
        uniformsBindGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &uniformsBindGroupDesc);

        std::array<WGPUBindGroupEntry, 2> pixelsBindGroupEntries{
            WGPUBindGroupEntry{
                .nextInChain = nullptr,
                .binding = 0,
                .buffer = frameDataBuffer,
                .offset = 0,
                .size = sizeof(FrameDataBuffer),
                .sampler = nullptr,
                .textureView = nullptr,
            },
            WGPUBindGroupEntry{
                .nextInChain = nullptr,
                .binding = 1,
                .buffer = pixelBuffer,
                .offset = 0,
                .size = static_cast<std::uint64_t>(pixelBufferNumBytes),
                .sampler = nullptr,
                .textureView = nullptr,
            },
        };

        const WGPUBindGroupDescriptor pixelsBindGroupDesc{
            .nextInChain = nullptr,
            .label = "image bind group",
            .layout = pixelsBindGroupLayout,
            .entryCount = pixelsBindGroupEntries.size(),
            .entries = pixelsBindGroupEntries.data(),
        };
        renderPixelsBindGroup = wgpuDeviceCreateBindGroup(gpuContext.device, &pixelsBindGroupDesc);

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
}

Renderer::~Renderer()
{
    renderPipelineSafeRelease(renderPipeline);
    renderPipeline = nullptr;
    bindGroupSafeRelease(renderPixelsBindGroup);
    renderPixelsBindGroup = nullptr;
    bindGroupSafeRelease(uniformsBindGroup);
    uniformsBindGroup = nullptr;
    bufferSafeRelease(uniformsBuffer);
    uniformsBuffer = nullptr;
    bufferSafeRelease(vertexBuffer);
    vertexBuffer = nullptr;
    computePipelineSafeRelease(computePipeline);
    computePipeline = nullptr;
    bindGroupSafeRelease(computePixelsBindGroup);
    computePixelsBindGroup = nullptr;
    bufferSafeRelease(pixelBuffer);
    pixelBuffer = nullptr;
    bufferSafeRelease(frameDataBuffer);
    frameDataBuffer = nullptr;
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

    {
        const FrameDataBuffer frameData{
            .dimensions =
                Extent2u{
                    .x = static_cast<std::uint32_t>(currentFramebufferSize.x),
                    .y = static_cast<std::uint32_t>(currentFramebufferSize.y)},
            .frameCount = frameCount++,
            .padding = 0,
        };
        wgpuQueueWriteBuffer(
            gpuContext.queue, frameDataBuffer, 0, &frameData, sizeof(FrameDataBuffer));
    }

    const WGPUCommandEncoder encoder = [&gpuContext]() {
        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
            .nextInChain = nullptr,
            .label = "Command encoder",
        };
        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
    }();

    {
        const WGPUComputePassEncoder computePassEncoder = [encoder]() -> WGPUComputePassEncoder {
            const WGPUComputePassDescriptor computePassDesc{
                .nextInChain = nullptr,
                .label = "Compute pass encoder",
                .timestampWriteCount = 0,
                .timestampWrites = nullptr,
            };

            return wgpuCommandEncoderBeginComputePass(encoder, &computePassDesc);
        }();

        wgpuComputePassEncoderSetPipeline(computePassEncoder, computePipeline);
        wgpuComputePassEncoderSetBindGroup(
            computePassEncoder, 0, computePixelsBindGroup, 0, nullptr);

        const Extent2u workgroupSize{.x = 8, .y = 8};
        const Extent2u numWorkgroups{
            .x = (currentFramebufferSize.x + workgroupSize.x - 1) / workgroupSize.x,
            .y = (currentFramebufferSize.y + workgroupSize.y - 1) / workgroupSize.y,
        };

        wgpuComputePassEncoderDispatchWorkgroups(
            computePassEncoder, numWorkgroups.x, numWorkgroups.y, 1);

        wgpuComputePassEncoderEnd(computePassEncoder);
    }

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
                renderPassEncoder, 1, renderPixelsBindGroup, 0, nullptr);
            wgpuRenderPassEncoderSetVertexBuffer(
                renderPassEncoder, 0, vertexBuffer, 0, vertexBufferByteSize);
            wgpuRenderPassEncoderDraw(renderPassEncoder, 6, 1, 0, 0);
        }

        wgpuRenderPassEncoderEnd(renderPassEncoder);
    }

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

void Renderer::resizeFramebuffer(const Extent2i& newSize)
{
    if (newSize.x <= 0 || newSize.y <= 0)
    {
        return;
    }

    currentFramebufferSize = newSize;
}
} // namespace pt
