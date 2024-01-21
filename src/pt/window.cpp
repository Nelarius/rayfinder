#include "gpu_context.hpp"
#include "window.hpp"

#include <common/assert.hpp>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>

#include <format>
#include <stdexcept>

namespace nlrs
{
namespace
{
void surfaceSafeRelease(const WGPUSurface surface)
{
    if (surface != nullptr)
    {
        wgpuSurfaceRelease(surface);
    }
}

void swapChainSafeRelease(const WGPUSwapChain swapChain)
{
    if (swapChain != nullptr)
    {
        wgpuSwapChainRelease(swapChain);
    }
}

WGPUSwapChain createSwapChain(
    const WGPUDevice        device,
    const WGPUSurface       surface,
    const WGPUTextureFormat swapChainFormat,
    const Extent2i          framebufferSize)
{
    const WGPUSwapChainDescriptor swapChainDesc{
        .nextInChain = nullptr,
        .label = "Swap chain",
        .usage = WGPUTextureUsage_RenderAttachment,
        .format = swapChainFormat,
        .width = static_cast<std::uint32_t>(framebufferSize.x),
        .height = static_cast<std::uint32_t>(framebufferSize.y),
        .presentMode = WGPUPresentMode_Fifo,
    };
    return wgpuDeviceCreateSwapChain(device, surface, &swapChainDesc);
}
} // namespace

int Window::glfwRefCount = 0;

Window::Window(const WindowDescriptor& windowDesc, const GpuContext& gpuContext)
    : mWindow(nullptr),
      mSurface(nullptr),
      mSwapChain(nullptr)
{
    if (glfwRefCount++ == 0)
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW.");
        }
        // NOTE: with this hint in place, GLFW assumes that we will manage the API and we can skip
        // calling glfwSwapBuffers.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    }

    mWindow = glfwCreateWindow(
        windowDesc.windowSize.x,
        windowDesc.windowSize.y,
        windowDesc.title.data(),
        nullptr,
        nullptr);

    if (!mWindow)
    {
        throw std::runtime_error("Failed to create GLFW window.");
    }

    mSurface = glfwGetWGPUSurface(gpuContext.instance, mWindow);
    Extent2i framebufferSize;
    glfwGetFramebufferSize(mWindow, &framebufferSize.x, &framebufferSize.y);
    mSwapChain = createSwapChain(gpuContext.device, mSurface, SWAP_CHAIN_FORMAT, framebufferSize);
}

Window::Window(Window&& other)
    : mWindow(other.mWindow),
      mSurface(other.mSurface),
      mSwapChain(other.mSwapChain)
{
    other.mWindow = nullptr;
    other.mSurface = nullptr;
    other.mSwapChain = nullptr;
}

Window& Window::operator=(Window&& other)
{
    if (this != &other)
    {
        if (mWindow)
        {
            glfwDestroyWindow(mWindow);
        }

        surfaceSafeRelease(mSurface);
        swapChainSafeRelease(mSwapChain);

        glfwRefCount -= 1;
        NLRS_ASSERT(glfwRefCount > 0);

        mWindow = other.mWindow;
        mSurface = other.mSurface;
        mSwapChain = other.mSwapChain;

        other.mWindow = nullptr;
        other.mSurface = nullptr;
        other.mSwapChain = nullptr;
    }

    return *this;
}

Window::~Window()
{
    if (mWindow)
    {
        glfwDestroyWindow(mWindow);
        mWindow = nullptr;
    }

    surfaceSafeRelease(mSurface);
    mSurface = nullptr;
    swapChainSafeRelease(mSwapChain);
    mSwapChain = nullptr;

    if (--glfwRefCount == 0)
    {
        glfwTerminate();
    }
}

Extent2i Window::size() const
{
    Extent2i result;
    glfwGetWindowSize(mWindow, &result.x, &result.y);
    return result;
}

Extent2i Window::resolution() const
{
    Extent2i result;
    glfwGetFramebufferSize(mWindow, &result.x, &result.y);
    return result;
}

void Window::run(
    const GpuContext&  gpuContext,
    NewFrameCallback&& newFrameCallback,
    UpdateCallback&&   updateCallback,
    RenderCallback&&   renderCallback)
{
    Extent2i currentFramebufferSize = resolution();
    auto     lastTime = std::chrono::steady_clock::now();
    while (!glfwWindowShouldClose(mWindow))
    {
        const auto  currentTime = std::chrono::steady_clock::now();
        const float deltaTime =
            std::chrono::duration<float, std::chrono::seconds::period>(currentTime - lastTime)
                .count();
        lastTime = currentTime;

        glfwPollEvents();

        // Resize
        {
            const Extent2i newFramebufferSize = resolution();
            if (newFramebufferSize != currentFramebufferSize)
            {
                currentFramebufferSize = newFramebufferSize;

                if (newFramebufferSize.x == 0 || newFramebufferSize.y == 0)
                {
                    return;
                }

                swapChainSafeRelease(mSwapChain);
                mSwapChain = createSwapChain(
                    gpuContext.device, mSurface, SWAP_CHAIN_FORMAT, newFramebufferSize);
            }
        }

        if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(mWindow, GLFW_TRUE);
        }

        newFrameCallback();
        updateCallback(mWindow, deltaTime);
        renderCallback(mWindow, mSwapChain);

        wgpuSwapChainPresent(mSwapChain);
    }
}
} // namespace nlrs
