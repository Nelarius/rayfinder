#pragma once

#include <cstddef>
#include <webgpu/webgpu.h>

namespace pt
{
struct GpuContext;

struct Renderer
{
    WGPUBuffer         vertexBuffer;
    std::size_t        vertexBufferByteSize;
    WGPUBuffer         uniformsBuffer;
    WGPUBindGroup      uniformsBindGroup;
    WGPURenderPipeline renderPipeline;

    explicit Renderer(const GpuContext&);
    ~Renderer();

    void render(const GpuContext&);
};
} // namespace pt
