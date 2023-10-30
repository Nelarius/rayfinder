#include "common/framebuffer_size.hpp"
#include "gpu_context.hpp"
#include "renderer.hpp"

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

    {
        pt::FramebufferSize framebufferSize;
        glfwGetFramebufferSize(window, &framebufferSize.width, &framebufferSize.height);

        pt::GpuContext gpuContext(window, pt::Renderer::wgpuRequiredLimits);
        pt::Renderer   renderer(gpuContext);

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
                    renderer.render(gpuContext);
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
