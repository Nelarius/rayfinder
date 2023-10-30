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

    int framebufferWidth;
    int framebufferHeight;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    //      ____           __
    //     /  _/___  _____/ /_____ _____  ________
    //     / // __ \/ ___/ __/ __ `/ __ \/ ___/ _ \
    //   _/ // / / (__  ) /_/ /_/ / / / / /__/  __/
    //  /___/_/ /_/____/\__/\__,_/_/ /_/\___/\___/
    //

    const WGPUInstance instance = []() -> WGPUInstance {
        const WGPUInstanceDescriptor instanceDesc{
            .nextInChain = nullptr,
        };
        return wgpuCreateInstance(&instanceDesc);
    }();

    if (!instance)
    {
        std::fprintf(stderr, "Failed to create WebGPU instance\n");
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    const WGPUSurface surface = glfwGetWGPUSurface(instance, window);

    //      ___       __            __
    //     /   | ____/ /___ _____  / /____  _____
    //    / /| |/ __  / __ `/ __ \/ __/ _ \/ ___/
    //   / ___ / /_/ / /_/ / /_/ / /_/  __/ /
    //  /_/  |_\__,_/\__,_/ .___/\__/\___/_/
    //                   /_/

    const WGPUAdapter adapter = [instance]() -> WGPUAdapter {
        const WGPURequestAdapterOptions adapterOptions = {};
        WGPUAdapter                     adapter = nullptr;

        auto onAdapterResponse = [](WGPURequestAdapterStatus status,
                                    WGPUAdapter              adapterResponse,
                                    char const*              message,
                                    void*                    userData) {
            WGPUAdapter* adapter = reinterpret_cast<WGPUAdapter*>(userData);
            if (status == WGPURequestAdapterStatus_Success)
            {
                *adapter = adapterResponse;
            }
            else
            {
                std::fprintf(stderr, "Failed to request adapter: %s\n", message);
            }
        };

        wgpuInstanceRequestAdapter(instance, &adapterOptions, onAdapterResponse, &adapter);

        return adapter;
    }();

    if (!adapter)
    {
        std::fprintf(stderr, "Failed to request adapter\n");
        wgpuInstanceRelease(instance);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    //      ____            _
    //     / __ \___ _   __(_)_______
    //    / / / / _ \ | / / / ___/ _ \
    //   / /_/ /  __/ |/ / / /__/  __/
    //  /_____/\___/|___/_/\___/\___/
    //

    const WGPUDevice device = [adapter]() -> WGPUDevice {
        // TODO: The renderer should enumerate the base minimum support required.
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

        const WGPUDeviceDescriptor deviceDesc{
            .nextInChain = nullptr,
            .label = "Learn WebGPU device",
            .requiredFeaturesCount = 0,
            .requiredFeatures = nullptr,
            .requiredLimits = &requiredLimits,
            .defaultQueue = WGPUQueueDescriptor{.nextInChain = nullptr, .label = "Default queue"},
            .deviceLostCallback = onDeviceLost,
            .deviceLostUserdata = nullptr,
        };
        WGPUDevice device = nullptr;

        auto onDeviceResponse = [](WGPURequestDeviceStatus status,
                                   WGPUDevice              maybeDevice,
                                   char const* const       message,
                                   void*                   userData) -> void {
            WGPUDevice* device = reinterpret_cast<WGPUDevice*>(userData);
            if (status == WGPURequestDeviceStatus_Success)
            {
                *device = maybeDevice;
                wgpuDeviceSetUncapturedErrorCallback(*device, onDeviceError, nullptr);
            }
            else
            {
                std::fprintf(stderr, "Failed to request device: %s\n", message);
            }
        };

        wgpuAdapterRequestDevice(adapter, &deviceDesc, onDeviceResponse, &device);

        return device;
    }();

    if (!device)
    {
        std::fprintf(stderr, "Failed to request device\n");
        wgpuSurfaceRelease(surface);
        wgpuAdapterRelease(adapter);
        wgpuInstanceRelease(instance);
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    const WGPUQueue queue = wgpuDeviceGetQueue(device);
    wgpuQueueOnSubmittedWorkDone(queue, 0, onQueueWorkDone, nullptr);

    //     _____                          __          _
    //    / ___/      ______ _____  _____/ /_  ____ _(_)___
    //    \__ \ | /| / / __ `/ __ \/ ___/ __ \/ __ `/ / __ \
    //   ___/ / |/ |/ / /_/ / /_/ / /__/ / / / /_/ / / / / /
    //  /____/|__/|__/\__,_/ .___/\___/_/ /_/\__,_/_/_/ /_/
    //                    /_/

    auto createSwapChain = [device, surface](
                               const WGPUTextureFormat swapChainFormat,
                               const int               framebufferWidth,
                               const int               framebufferHeight) -> WGPUSwapChain {
        const WGPUSwapChainDescriptor swapChainDesc{
            .nextInChain = nullptr,
            .label = "Swap chain",
            .usage = WGPUTextureUsage_RenderAttachment,
            .format = swapChainFormat,
            .width = static_cast<std::uint32_t>(framebufferWidth),
            .height = static_cast<std::uint32_t>(framebufferHeight),
            .presentMode = WGPUPresentMode_Fifo,
        };
        return wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
    };

    constexpr WGPUTextureFormat swapChainFormat = WGPUTextureFormat_BGRA8Unorm;
    WGPUSwapChain swapChain = createSwapChain(swapChainFormat, framebufferWidth, framebufferHeight);

    {
        glfwMakeContextCurrent(window);

        while (!glfwWindowShouldClose(window))
        {
            // Non-standard Dawn way to ensure that Dawn checks that whether the async operation is
            // actually done and calls the callback.
            wgpuDeviceTick(device);
            glfwPollEvents();

            // Resize
            {
                int currentWidth;
                int currentHeight;
                glfwGetFramebufferSize(window, &currentWidth, &currentHeight);

                if (currentWidth != framebufferWidth || currentHeight != framebufferHeight)
                {
                    framebufferWidth = currentWidth;
                    framebufferHeight = currentHeight;

                    wgpuSwapChainRelease(swapChain);
                    swapChain =
                        createSwapChain(swapChainFormat, framebufferWidth, framebufferHeight);
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
                const WGPUTextureView nextTexture = wgpuSwapChainGetCurrentTextureView(swapChain);
                if (!nextTexture)
                {
                    // Getting the next texture can fail, if e.g. the window has been resized.
                    std::fprintf(stderr, "Failed to get texture view from swap chain\n");
                    continue;
                }

                const WGPUCommandEncoder encoder = [device]() {
                    const WGPUCommandEncoderDescriptor cmdEncoderDesc{
                        .nextInChain = nullptr,
                        .label = "Command encoder",
                    };
                    return wgpuDeviceCreateCommandEncoder(device, &cmdEncoderDesc);
                }();

                const WGPURenderPassEncoder renderPass = [encoder,
                                                          nextTexture]() -> WGPURenderPassEncoder {
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
                wgpuQueueSubmit(queue, 1, &cmdBuffer);

                wgpuTextureViewRelease(nextTexture);
            }

            wgpuSwapChainPresent(swapChain);

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    wgpuSwapChainRelease(swapChain);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuSurfaceRelease(surface);
    wgpuInstanceRelease(instance);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
