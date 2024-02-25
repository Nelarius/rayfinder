#pragma once

#include <fmt/core.h>
#include <webgpu/webgpu.h>

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace nlrs
{
inline void bindGroupSafeRelease(const WGPUBindGroup bindGroup) noexcept
{
    if (bindGroup)
    {
        wgpuBindGroupRelease(bindGroup);
    }
}

inline void querySetSafeRelease(const WGPUQuerySet querySet) noexcept
{
    if (querySet)
    {
        wgpuQuerySetDestroy(querySet);
        wgpuQuerySetRelease(querySet);
    }
}

inline void renderPipelineSafeRelease(const WGPURenderPipeline pipeline) noexcept
{
    if (pipeline)
    {
        wgpuRenderPipelineRelease(pipeline);
    }
}

constexpr std::array<float[2], 6> quadVertexData{{
    // clang-format off
    {-0.5f, -0.5f,},
    {0.5f, -0.5f,},
    {0.5f, 0.5f,},
    {0.5f, 0.5f,},
    {-0.5f, 0.5f,},
    {-0.5f, -0.5f,},
    // clang-format on
}};

inline std::string loadShaderSource(std::string_view path)
{
    // TODO: path could be an actual path
    std::ifstream file(path.data());
    std::ifstream(path.data());
    if (!file)
    {
        throw std::runtime_error(fmt::format("Error opening shader source: {}.", path));
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}
} // namespace nlrs
