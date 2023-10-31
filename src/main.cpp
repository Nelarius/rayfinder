#include "gpu_context.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <webgpu/webgpu.h>

inline constexpr int defaultWindowWidth = 640;
inline constexpr int defaultWindowHeight = 480;

int main()
{
    pt::Window window{pt::WindowDescriptor{
        .windowSize = pt::Extent2i{defaultWindowWidth, defaultWindowHeight},
        .title = "pt-playground 🛝",
    }};

    {
        pt::GpuContext gpuContext(window.ptr(), pt::Renderer::wgpuRequiredLimits);
        pt::Renderer   renderer(
            pt::RendererDescriptor{
                  .currentFramebufferSize = window.resolution(),
                  .maxFramebufferSize = window.largestMonitorResolution(),
            },
            gpuContext);

        {
            pt::Extent2i curFramebufferSize = window.resolution();
            while (!glfwWindowShouldClose(window.ptr()))
            {
                // Non-standard Dawn way to ensure that Dawn checks that whether the async operation
                // is actually done and calls the callback.
                wgpuDeviceTick(gpuContext.device);
                glfwPollEvents();

                // Resize
                {
                    const pt::Extent2i newFramebufferSize = window.resolution();
                    if (newFramebufferSize != curFramebufferSize)
                    {
                        curFramebufferSize = newFramebufferSize;
                        gpuContext.resizeFramebuffer(curFramebufferSize);
                        renderer.resizeFramebuffer(curFramebufferSize);
                    }
                }

                // Update

                {
                    if (glfwGetKey(window.ptr(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                    {
                        glfwSetWindowShouldClose(window.ptr(), GLFW_TRUE);
                    }
                }

                // Render

                {
                    renderer.render(gpuContext);
                }

                wgpuSwapChainPresent(gpuContext.swapChain);

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }
    }

    return 0;
}
