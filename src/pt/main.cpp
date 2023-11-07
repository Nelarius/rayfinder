#include "fly_camera_controller.hpp"
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
#include <imgui.h>
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
            .renderParams = [&window, &cameraController]() -> pt::RenderParameters {
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
            float        vfovDegrees = 80.0f;
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

                // ImGui

                renderer.beginFrame();
                window.beginFrame();

                {
                    ImGui::Begin("Settings");
                    ImGui::Text("Parameters");
                    ImGui::SliderFloat(
                        "camera speed",
                        &cameraController.speed(),
                        0.05f,
                        100.0f,
                        "%.2f",
                        ImGuiSliderFlags_Logarithmic);
                    ImGui::SliderFloat("camera vfov", &vfovDegrees, 10.0f, 120.0f);
                    cameraController.vfov() = pt::Angle::degrees(vfovDegrees);

                    ImGui::Separator();
                    ImGui::Text("Scene");
                    {
                        const pt::BvhNode& rootNode = bvh.nodes[0];
                        const glm::vec3    rootCenter = centroid(rootNode.aabb);
                        const glm::vec3    camPos = cameraController.position();
                        ImGui::Text(
                            "camera position: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
                        ImGui::Text(
                            "root centroid: (%.2f, %.2f, %.2f)",
                            rootCenter.x,
                            rootCenter.y,
                            rootCenter.z);
                    }

                    ImGui::End();
                }

                // Update

                {
                    if (glfwGetKey(window.ptr(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
                    {
                        glfwSetWindowShouldClose(window.ptr(), GLFW_TRUE);
                    }

                    // Skip input if ImGui captured input
                    if (!ImGui::GetIO().WantCaptureMouse)
                    {
                        cameraController.update(window.ptr(), 0.0167f);
                    }
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
