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
        pt::CameraController cameraController;
        pt::GpuContext       gpuContext(window.ptr(), pt::Renderer::wgpuRequiredLimits);

        const pt::GltfModel model("Duck.glb");
        const pt::Bvh       bvh = pt::buildBvh(model.triangles());

        const pt::RendererDescriptor rendererDesc{
            .renderParams = [&window, &bvh]() -> pt::RenderParameters {
                const pt::Extent2i framebufferSize = window.resolution();
                return pt::RenderParameters{
                    .framebufferSize = pt::Extent2u(framebufferSize),
                    .camera = [&window, &bvh]() -> pt::Camera {
                        const pt::Extent2i framebufferSize = window.resolution();
                        const pt::BvhNode& rootNode = bvh.nodes[0];
                        const pt::Aabb  rootAabb = pt::Aabb(rootNode.aabb.min, rootNode.aabb.max);
                        const glm::vec3 rootDiagonal = diagonal(rootAabb);
                        const glm::vec3 rootCentroid = centroid(rootAabb);
                        const int       maxDim = maxDimension(rootAabb);

                        const float     aperture = 0.0f;
                        const float     focusDistance = 1.0f;
                        const pt::Angle vfov = pt::Angle::degrees(70.0f);

                        return createCamera(
                            rootCentroid -
                                glm::vec3(
                                    -0.8 * rootDiagonal[maxDim], 0.0f, 0.8f * rootDiagonal[maxDim]),
                            rootCentroid,
                            aperture,
                            focusDistance,
                            vfov,
                            pt::aspectRatio(framebufferSize));
                    }(),
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

                    cameraController.leftPressed =
                        glfwGetKey(window.ptr(), GLFW_KEY_A) == GLFW_PRESS;
                    cameraController.rightPressed =
                        glfwGetKey(window.ptr(), GLFW_KEY_D) == GLFW_PRESS;
                    cameraController.forwardPressed =
                        glfwGetKey(window.ptr(), GLFW_KEY_W) == GLFW_PRESS;
                    cameraController.backwardPressed =
                        glfwGetKey(window.ptr(), GLFW_KEY_S) == GLFW_PRESS;
                    cameraController.upPressed = glfwGetKey(window.ptr(), GLFW_KEY_E) == GLFW_PRESS;
                    cameraController.downPressed =
                        glfwGetKey(window.ptr(), GLFW_KEY_Q) == GLFW_PRESS;
                    cameraController.update(0.0167f);
                }

                // Render

                {
                    // const pt::RenderParameters renderParams{
                    //     .camera = cameraController.getCamera(curFramebufferSize),
                    // };
                    // renderer.setRenderParameters(renderParams);
                    renderer.render(gpuContext);
                }

                wgpuSwapChainPresent(gpuContext.swapChain);

                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
        }
    }

    return 0;
}
