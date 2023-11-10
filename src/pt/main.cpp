#include "fly_camera_controller.hpp"
#include "gpu_context.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <common/bvh.hpp>
#include <common/gltf_model.hpp>

#include <cassert>
#include <chrono>
#include <cstdio>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <imgui.h>
#include <webgpu/webgpu.h>

inline constexpr int defaultWindowWidth = 640;
inline constexpr int defaultWindowHeight = 480;

int main()
{
    nlrs::Window window{nlrs::WindowDescriptor{
        .windowSize = nlrs::Extent2i{defaultWindowWidth, defaultWindowHeight},
        .title = "pt-playground 🛝",
    }};

    {
        const nlrs::GltfModel model("Duck.glb");
        const nlrs::Bvh bvh = nlrs::buildBvh(model.positions(), model.normals(), model.texCoords());

        nlrs::FlyCameraController cameraController;
        nlrs::GpuContext          gpuContext(window.ptr(), nlrs::Renderer::wgpuRequiredLimits);

        {
            const nlrs::BvhNode& rootNode = bvh.nodes[0];
            const glm::vec3      rootCentroid = centroid(rootNode.aabb);

            cameraController.lookAt(rootCentroid);
        }

        const nlrs::RendererDescriptor rendererDesc{
            .renderParams = [&window, &cameraController]() -> nlrs::RenderParameters {
                const nlrs::Extent2i framebufferSize = window.resolution();
                return nlrs::RenderParameters{
                    .framebufferSize = nlrs::Extent2u(framebufferSize),
                    .camera = cameraController.getCamera(),
                };
            }(),
            .maxFramebufferSize = window.largestMonitorResolution(),
        };

        nlrs::Renderer renderer(rendererDesc, gpuContext, bvh);

        {
            nlrs::Extent2i curFramebufferSize = window.resolution();
            float          vfovDegrees = 70.0f;
            while (!glfwWindowShouldClose(window.ptr()))
            {
                glfwPollEvents();

                // Resize
                {
                    const nlrs::Extent2i newFramebufferSize = window.resolution();
                    if (newFramebufferSize != curFramebufferSize)
                    {
                        curFramebufferSize = newFramebufferSize;
                        gpuContext.resizeFramebuffer(curFramebufferSize);
                    }
                }

                // Non-standard Dawn way to ensure that Dawn ticks pending async operations.
                // TODO: implement some kind of pending buffer map queue and tick them here
                while (wgpuBufferGetMapState(renderer.timestampBuffer.handle()) !=
                       WGPUBufferMapState_Unmapped)
                {
                    wgpuDeviceTick(gpuContext.device);
                }

                renderer.beginFrame();
                window.beginFrame();

                // ImGui

                {
                    ImGui::Begin("pt");

                    ImGui::Text("Performance");
                    {
                        const float drawAverageMs = renderer.averageDrawDurationMs();
                        const float renderAverageMs = renderer.averageRenderpassDurationMs();
                        ImGui::Text(
                            "render pass draw: %.2f ms (%.1f FPS)",
                            drawAverageMs,
                            1000.0f / drawAverageMs);
                        ImGui::Text(
                            "render pass: %.2f ms (%.1f FPS)",
                            renderAverageMs,
                            1000.0f / renderAverageMs);
                    }
                    ImGui::Separator();

                    ImGui::Text("Parameters");
                    ImGui::SliderFloat(
                        "camera speed",
                        &cameraController.speed(),
                        0.05f,
                        100.0f,
                        "%.2f",
                        ImGuiSliderFlags_Logarithmic);
                    ImGui::SliderFloat("camera vfov", &vfovDegrees, 10.0f, 120.0f);
                    cameraController.vfov() = nlrs::Angle::degrees(vfovDegrees);

                    ImGui::Separator();
                    ImGui::Text("Scene");
                    {
                        const nlrs::BvhNode& rootNode = bvh.nodes[0];
                        const glm::vec3      rootCenter = centroid(rootNode.aabb);
                        const glm::vec3      camPos = cameraController.position();
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
                    const nlrs::RenderParameters renderParams{
                        .camera = cameraController.getCamera(),
                    };
                    renderer.setRenderParameters(renderParams);
                    renderer.render(gpuContext);
                }

                wgpuSwapChainPresent(gpuContext.swapChain);
            }
        }
    }

    return 0;
}
