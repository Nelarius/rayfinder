#pragma once

namespace nlrs
{
constexpr WGPULimits REQUIRED_LIMITS{
    .maxTextureDimension1D = 0,
    .maxTextureDimension2D = 0,
    .maxTextureDimension3D = 0,
    .maxTextureArrayLayers = 0,
    .maxBindGroups = 4,
    .maxBindGroupsPlusVertexBuffers = 0,
    .maxBindingsPerBindGroup = 8,
    .maxDynamicUniformBuffersPerPipelineLayout = 0,
    .maxDynamicStorageBuffersPerPipelineLayout = 0,
    .maxSampledTexturesPerShaderStage = 0,
    .maxSamplersPerShaderStage = 0,
    .maxStorageBuffersPerShaderStage = 8,
    .maxStorageTexturesPerShaderStage = 0,
    .maxUniformBuffersPerShaderStage = 1,
    .maxUniformBufferBindingSize = 1 << 10,
    .maxStorageBufferBindingSize = 1 << 30,
    .minUniformBufferOffsetAlignment = 256,
    .minStorageBufferOffsetAlignment = 256,
    .maxVertexBuffers = 4,
    .maxBufferSize = 1 << 30,
    .maxVertexAttributes = 8,
    .maxVertexBufferArrayStride = sizeof(float[4]),
    .maxInterStageShaderComponents = 0,
    .maxInterStageShaderVariables = 0,
    .maxColorAttachments = 4,
    .maxColorAttachmentBytesPerSample = 0,
    .maxComputeWorkgroupStorageSize = 0,
    .maxComputeInvocationsPerWorkgroup = 0,
    .maxComputeWorkgroupSizeX = 0,
    .maxComputeWorkgroupSizeY = 0,
    .maxComputeWorkgroupSizeZ = 0,
    .maxComputeWorkgroupsPerDimension = 0,
};
}
