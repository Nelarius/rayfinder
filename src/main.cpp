#include "common/framebuffer_size.hpp"
#include "gpu_context.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

const char* WGPUDeviceLostReasonToStr(WGPUDeviceLostReason reason)
{
    switch (reason)
    {
    case WGPUDeviceLostReason_Undefined:
        return "Undefined";
    case WGPUDeviceLostReason_Destroyed:
        return "Destroyed";
    }

    assert(!"Unknown WGPUDeviceLostReason");

    return nullptr;
}

const char* WGPUErrorTypeToStr(const WGPUErrorType type)
{
    switch (type)
    {
    case WGPUErrorType_NoError:
        return "NoError";
    case WGPUErrorType_Validation:
        return "Validation";
    case WGPUErrorType_OutOfMemory:
        return "OutOfMemory";
    case WGPUErrorType_Internal:
        return "Internal";
    case WGPUErrorType_Unknown:
        return "Unknown";
    case WGPUErrorType_DeviceLost:
        return "DeviceLost";
    }

    assert(!"Unknown WGPUErrorType");

    return nullptr;
}

void onDeviceLost(WGPUDeviceLostReason reason, const char* const message, void* /*userdata*/)
{
    std::fprintf(stderr, "Device lost reason: %s\n", WGPUDeviceLostReasonToStr(reason));
    if (message)
    {
        std::fprintf(stderr, "%s\n", message);
    }
}

void onDeviceError(WGPUErrorType type, const char* message, void* /*userdata*/)
{
    std::fprintf(stderr, "Uncaptured device error: %s\n", WGPUErrorTypeToStr(type));
    if (message)
    {
        std::fprintf(stderr, ": %s\n", message);
    }
}

const char* WGPUQueueWorkDoneStatusToStr(const WGPUQueueWorkDoneStatus status)
{
    switch (status)
    {
    case WGPUQueueWorkDoneStatus_Success:
        return "Success";
    case WGPUQueueWorkDoneStatus_Error:
        return "Error";
    case WGPUQueueWorkDoneStatus_Unknown:
        return "Unknown";
    case WGPUQueueWorkDoneStatus_DeviceLost:
        return "DeviceLost";
    }

    assert(!"Unknown WGPUQueueWorkDoneStatus");

    return nullptr;
}

void onQueueWorkDone(WGPUQueueWorkDoneStatus status, void* /*userdata*/)
{
    std::fprintf(stderr, "Queue work done status: %s\n", WGPUQueueWorkDoneStatusToStr(status));
}

inline constexpr int defaultWindowWidth = 640;
inline constexpr int defaultWindowHeight = 480;

int main()
{
    if (!glfwInit())
    {
        std::fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    // NOTE: with this hint in place, GLFW assumes that we will manage the API and we can skip
    // calling glfwSwapBuffers.
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* const window = glfwCreateWindow(
        defaultWindowWidth, defaultWindowHeight, "pt-playground 🛝", nullptr, nullptr);
    if (!window)
    {
        std::fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return -1;
    }

    const WGPURequiredLimits requiredLimits{
        .nextInChain = nullptr,
        .limits =
            WGPULimits{
                .maxTextureDimension1D = 0,
                .maxTextureDimension2D = 0,
                .maxTextureDimension3D = 0,
                .maxTextureArrayLayers = 0,
                .maxBindGroups = 0,
                .maxBindGroupsPlusVertexBuffers = 0,
                .maxBindingsPerBindGroup = 0,
                .maxDynamicUniformBuffersPerPipelineLayout = 0,
                .maxDynamicStorageBuffersPerPipelineLayout = 0,
                .maxSampledTexturesPerShaderStage = 0,
                .maxSamplersPerShaderStage = 0,
                .maxStorageBuffersPerShaderStage = 0,
                .maxStorageTexturesPerShaderStage = 0,
                .maxUniformBuffersPerShaderStage = 0,
                .maxUniformBufferBindingSize = 0,
                .maxStorageBufferBindingSize = 0,
                .minUniformBufferOffsetAlignment = 64,
                .minStorageBufferOffsetAlignment = 16,
                .maxVertexBuffers = 0,
                .maxBufferSize = 0,
                .maxVertexAttributes = 0,
                .maxVertexBufferArrayStride = 0,
                .maxInterStageShaderComponents = 0,
                .maxInterStageShaderVariables = 0,
                .maxColorAttachments = 0,
                .maxColorAttachmentBytesPerSample = 0,
                .maxComputeWorkgroupStorageSize = 0,
                .maxComputeInvocationsPerWorkgroup = 0,
                .maxComputeWorkgroupSizeX = 0,
                .maxComputeWorkgroupSizeY = 0,
                .maxComputeWorkgroupSizeZ = 0,
                .maxComputeWorkgroupsPerDimension = 0,
            },
    };

    {
        pt::GpuContext gpuContext(window, requiredLimits);

        pt::FramebufferSize framebufferSize;
        glfwGetFramebufferSize(window, &framebufferSize.width, &framebufferSize.height);

        {
            glfwMakeContextCurrent(window);

            while (!glfwWindowShouldClose(window))
            {
                // Non-standard Dawn way to ensure that Dawn checks that whether the async operation
                // is actually done and calls the callback.
                wgpuDeviceTick(gpuContext.device);
                glfwPollEvents();

                // Resize
                {
                    pt::FramebufferSize currentSize;
                    glfwGetFramebufferSize(window, &currentSize.width, &currentSize.height);

                    if (currentSize != framebufferSize)
                    {
                        framebufferSize = currentSize;
                        gpuContext.resizeFramebuffer(framebufferSize);
                    }
                }

                // Update

                {
                    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
                    {
                        glfwSetWindowShouldClose(window, GLFW_TRUE);
                    }
                }

                // Render

                {
                    const WGPUTextureView nextTexture =
                        wgpuSwapChainGetCurrentTextureView(gpuContext.swapChain);
                    if (!nextTexture)
                    {
                        // Getting the next texture can fail, if e.g. the window has been resized.
                        std::fprintf(stderr, "Failed to get texture view from swap chain\n");
                        continue;
                    }

                    const WGPUCommandEncoder encoder = [&gpuContext]() {
                        const WGPUCommandEncoderDescriptor cmdEncoderDesc{
                            .nextInChain = nullptr,
                            .label = "Command encoder",
                        };
                        return wgpuDeviceCreateCommandEncoder(gpuContext.device, &cmdEncoderDesc);
                    }();

                    const WGPURenderPassEncoder renderPass =
                        [encoder, nextTexture]() -> WGPURenderPassEncoder {
                        const WGPURenderPassColorAttachment renderPassColorAttachment{
                            .nextInChain = nullptr,
                            .view = nextTexture,
                            .resolveTarget = nullptr,
                            .loadOp = WGPULoadOp_Clear,
                            .storeOp = WGPUStoreOp_Store,
                            .clearValue = WGPUColor{0.9, 0.1, 0.8, 1.0},
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
                    wgpuRenderPassEncoderEnd(renderPass);

                    const WGPUCommandBuffer cmdBuffer = [encoder]() {
                        const WGPUCommandBufferDescriptor cmdBufferDesc{
                            .nextInChain = nullptr,
                            .label = "Command buffer",
                        };
                        return wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
                    }();
                    wgpuQueueSubmit(gpuContext.queue, 1, &cmdBuffer);

                    wgpuTextureViewRelease(nextTexture);
                }

                wgpuSwapChainPresent(gpuContext.swapChain);

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
