#include "gpu_context.hpp"

#include <common/platform.hpp>

#include <GLFW/glfw3.h>

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <format>
#include <stdexcept>

namespace nlrs
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

void instanceSafeRelease(const WGPUInstance instance)
{
    if (instance != nullptr)
    {
        wgpuInstanceRelease(instance);
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
} // namespace

GpuContext::GpuContext(const WGPURequiredLimits& requiredLimits)
    : instance(nullptr),
      device(nullptr),
      queue(nullptr)
{
    instance = []() -> WGPUInstance {
        // GPU timers are an unsafe API and are disabled by default due to exposing client
        // information. E.g. `ValidationTest::SetUp()` in
        // `src/dawn/tests/unittests/validation/ValidationTest.cpp` contains an example of how this
        // works.
        const char*               allowUnsafeApisToggle = "allow_unsafe_apis";
        WGPUDawnTogglesDescriptor instanceToggles = {
            .chain =
                WGPUChainedStruct{
                    .next = nullptr,
                    .sType = WGPUSType_DawnTogglesDescriptor,
                },
            .enabledToggleCount = 1,
            .enabledToggles = &allowUnsafeApisToggle,
            .disabledToggleCount = 0,
            .disabledToggles = nullptr,
        };

        const WGPUInstanceDescriptor instanceDesc{
            .nextInChain = &instanceToggles.chain,
        };
        return wgpuCreateInstance(&instanceDesc);
    }();

    if (!instance)
    {
        throw std::runtime_error("Failed to create WGPUInstance instance.");
    }

    const WGPUAdapter adapter = [this]() -> WGPUAdapter {
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
        instanceSafeRelease(instance);
        throw std::runtime_error("Failed to create WGPUAdapter instance.");
    }

    device = [adapter, &requiredLimits]() -> WGPUDevice {
        const std::array<WGPUFeatureName, 1> requiredFeatures{
            WGPUFeatureName_TimestampQuery,
        };

        const WGPUDeviceDescriptor deviceDesc{
            .nextInChain = nullptr,
            .label = "Device",
            .requiredFeatureCount = requiredFeatures.size(),
            .requiredFeatures = requiredFeatures.data(),
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
        adapterSafeRelease(adapter);
        instanceSafeRelease(instance);
        throw std::runtime_error("Failed to create WGPUDevice instance.");
    }

    queue = wgpuDeviceGetQueue(device);
    wgpuQueueOnSubmittedWorkDone(queue, onQueueWorkDone, nullptr);

    adapterSafeRelease(adapter);
}

GpuContext::~GpuContext()
{
    queueSafeRelease(queue);
    queue = nullptr;
    deviceSafeRelease(device);
    device = nullptr;
    instanceSafeRelease(instance);
    instance = nullptr;
}
} // namespace nlrs
