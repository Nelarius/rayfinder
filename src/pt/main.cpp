#include "camera_controller.hpp"
#include "gpu_context.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <common/bvh.hpp>
#include <common/gltf_model.hpp>

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
        const pt::GltfModel model("Duck.glb");
        const pt::Bvh       bvh = pt::buildBvh(model.triangles());

        pt::FlyCameraController cameraController;
        pt::GpuContext          gpuContext(window.ptr(), pt::Renderer::wgpuRequiredLimits);

        {
            const pt::BvhNode& rootNode = bvh.nodes[0];
            const glm::vec3    rootCentroid = centroid(rootNode.aabb);

            cameraController.lookAt(rootCentroid);
        }

        const pt::RendererDescriptor rendererDesc{
            .renderParams = [&window, &bvh, &cameraController]() -> pt::RenderParameters {
                const pt::Extent2i framebufferSize = window.resolution();
                return pt::RenderParameters{
                    .framebufferSize = pt::Extent2u(framebufferSize),
                    .camera = cameraController.getCamera(),
                };
            }(),
            .maxFramebufferSize = window.largestMonitorResolution(),
        };

        pt::Renderer renderer(rendererDesc, gpuContext, bvh);

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
                    }
                }

                // Update

                {
                    if (glfwGetKey(window.ptr(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                    {
                        glfwSetWindowShouldClose(window.ptr(), GLFW_TRUE);
                    }

                    cameraController.update(window.ptr(), 0.0167f);
                }

                // Render

                {
                    const pt::RenderParameters renderParams{
                        .camera = cameraController.getCamera(),
                    };
                    renderer.setRenderParameters(renderParams);
                    renderer.render(gpuContext);
                }

                wgpuSwapChainPresent(gpuContext.swapChain);

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }
    }

    return 0;
}
