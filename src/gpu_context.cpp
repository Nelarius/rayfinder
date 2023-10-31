#include "common/framebuffer_size.hpp"
#include "gpu_context.hpp"

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <format>
#include <stdexcept>

namespace pt
{
namespace
{
const char* WGPUDeviceLostReasonToStr(WGPUDeviceLostReason reason)
{
    switch (reason)
    {
    case WGPUDeviceLostReason_Undefined:
        return "Undefined";
    case WGPUDeviceLostReason_Destroyed:
        return "Destroyed";
    default:
        assert(!"Unknown WGPUDeviceLostReason");
        return nullptr;
    }
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
    default:
        assert(!"Unknown WGPUErrorType");
        return nullptr;
    }
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
    default:
        assert(!"Unknown WGPUQueueWorkDoneStatus");
        return nullptr;
    }
}

void onQueueWorkDone(WGPUQueueWorkDoneStatus status, void* /*userdata*/)
{
    std::fprintf(stderr, "Queue work done status: %s\n", WGPUQueueWorkDoneStatusToStr(status));
}

WGPUSwapChain createSwapChain(
    const WGPUDevice        device,
    const WGPUSurface       surface,
    const WGPUTextureFormat swapChainFormat,
    const FramebufferSize   framebufferSize)
{
    const WGPUSwapChainDescriptor swapChainDesc{
        .nextInChain = nullptr,
        .label = "Swap chain",
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = swapChainFormat,
        .width = static_cast<std::uint32_t>(framebufferSize.width),
        .height = static_cast<std::uint32_t>(framebufferSize.height),
        .presentMode = WGPUPresentMode_Fifo,
    };
    return wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
}

void instanceSafeRelease(const WGPUInstance instance)
{
    if (instance != nullptr)
    {
        wgpuInstanceRelease(instance);
    }
}

void surfaceSafeRelease(const WGPUSurface surface)
{
    if (surface != nullptr)
    {
        wgpuSurfaceRelease(surface);
    }
}

void adapterSafeRelease(const WGPUAdapter adapter)
{
    if (adapter != nullptr)
    {
        wgpuAdapterRelease(adapter);
    }
}

void deviceSafeRelease(const WGPUDevice device)
{
    if (device != nullptr)
    {
        wgpuDeviceRelease(device);
    }
}

void queueSafeRelease(const WGPUQueue queue)
{
    if (queue != nullptr)
    {
        wgpuQueueRelease(queue);
    }
}

void swapChainSafeRelease(const WGPUSwapChain swapChain)
{
    if (swapChain != nullptr)
    {
        wgpuSwapChainRelease(swapChain);
    }
}
} // namespace

GpuContext::GpuContext(GLFWwindow* const window, const WGPURequiredLimits& requiredLimits)
    : instance(nullptr), surface(nullptr), adapter(nullptr), device(nullptr), queue(nullptr),
      swapChain(nullptr)
{
    assert(window != nullptr);

    FramebufferSize framebufferSize;
    glfwGetFramebufferSize(window, &framebufferSize.width, &framebufferSize.height);

    instance = []() -> WGPUInstance {
        const WGPUInstanceDescriptor instanceDesc{
            .nextInChain = nullptr,
        };
        return wgpuCreateInstance(&instanceDesc);
    }();

    if (!instance)
    {
        throw std::runtime_error("Failed to create WGPUInstance instance.");
    }

    // TODO: passing in `WGPUSurfaceDescriptor` instance would be a way to not require a GLFW window
    // pointer. `glfwGetWGPUSurface` creates that instance under the hood using platform-specific
    // code.
    surface = glfwGetWGPUSurface(instance, window);

    adapter = [this]() -> WGPUAdapter {
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
        throw std::runtime_error("Failed to create WGPUAdapter instance.");
    }

    device = [this, &requiredLimits]() -> WGPUDevice {
        const WGPUDeviceDescriptor deviceDesc{
            .nextInChain = nullptr,
            .label = "Device",
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
        throw std::runtime_error("Failed to create WGPUDevice instance.");
    }

    queue = wgpuDeviceGetQueue(device);
    wgpuQueueOnSubmittedWorkDone(queue, 0, onQueueWorkDone, nullptr);

    swapChain = createSwapChain(device, surface, swapChainFormat, framebufferSize);
}

GpuContext::~GpuContext()
{
    swapChainSafeRelease(swapChain);
    swapChain = nullptr;
    queueSafeRelease(queue);
    queue = nullptr;
    deviceSafeRelease(device);
    device = nullptr;
    adapterSafeRelease(adapter);
    adapter = nullptr;
    surfaceSafeRelease(surface);
    surface = nullptr;
    instanceSafeRelease(instance);
    instance = nullptr;
}

void GpuContext::resizeFramebuffer(const FramebufferSize& newSize)
{
    if (newSize.width == 0 || newSize.height == 0)
    {
        return;
    }

    swapChainSafeRelease(swapChain);
    swapChain = createSwapChain(device, surface, swapChainFormat, newSize);
}
} // namespace pt
