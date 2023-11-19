#include "fly_camera_controller.hpp"
#include "gpu_context.hpp"
#include "gui.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <common/bvh.hpp>
#include <common/gltf_model.hpp>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>

#include <GLFW/glfw3.h>
#include <glfw3webgpu.h>
#include <imgui.h>
#include <webgpu/webgpu.h>

inline constexpr int defaultWindowWidth = 640;
inline constexpr int defaultWindowHeight = 480;

void printHelp() { std::printf("Usage: pt <input_gltf_file>\n"); }

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printHelp();
        return 0;
    }

    nlrs::Window window{nlrs::WindowDescriptor{
        .windowSize = nlrs::Extent2i{defaultWindowWidth, defaultWindowHeight},
        .title = "pt-playground 🛝",
    }};

    {
        nlrs::FlyCameraController cameraController;
        nlrs::GpuContext          gpuContext(window.ptr(), nlrs::Renderer::wgpuRequiredLimits);
        nlrs::Gui                 gui(window.ptr(), gpuContext);

        nlrs::Renderer renderer =
            [&cameraController, &gpuContext, &window, argv]() -> nlrs::Renderer {
            const nlrs::RendererDescriptor rendererDesc{
                [&window, &cameraController]() -> nlrs::RenderParameters {
                    const nlrs::Extent2i framebufferSize = window.resolution();
                    return nlrs::RenderParameters{
                        nlrs::Extent2u(framebufferSize),
                        cameraController.getCamera(),
                        nlrs::SamplingParams(),
                        nlrs::Sky()};
                }(),
                window.largestMonitorResolution(),
            };

            const nlrs::GltfModel model(argv[1]);
            const nlrs::Bvh       bvh = nlrs::buildBvh(model.positions());
            const auto normals = nlrs::reorderAttributes(model.normals(), bvh.positionIndices);
            const auto texCoords = nlrs::reorderAttributes(model.texCoords(), bvh.positionIndices);
            const auto textureIndices =
                nlrs::reorderAttributes(model.baseColorTextureIndices(), bvh.positionIndices);

            nlrs::Scene scene{
                .bvh = bvh,
                .normals = normals,
                .texCoords = texCoords,
                .textureIndices = textureIndices,
                .baseColorTextures = model.baseColorTextures(),
            };

            return nlrs::Renderer(rendererDesc, gpuContext, std::move(scene));
        }();

        {
            nlrs::Extent2i       curFramebufferSize = window.resolution();
            float                vfovDegrees = 70.0f;
            int                  numSamplesPerPixel = 128;
            int                  numBounces = 4;
            float                sunZenithDegrees = 30.0f;
            float                sunAzimuthDegrees = 0.0f;
            float                skyTurbidity = 1.0f;
            std::array<float, 3> skyAlbedo = {1.0f, 1.0f, 1.0f};
            auto                 lastTime = std::chrono::steady_clock::now();
            while (!glfwWindowShouldClose(window.ptr()))
            {
                const auto  currentTime = std::chrono::steady_clock::now();
                const float deltaTime = std::chrono::duration<float, std::chrono::seconds::period>(
                                            currentTime - lastTime)
                                            .count();
                lastTime = currentTime;

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

                gui.beginFrame();

                // ImGui

                {
                    ImGui::Begin("pt");

                    ImGui::Text("Renderer stats");
                    {
                        const float drawAverageMs = renderer.averageDrawDurationMs();
                        const float renderAverageMs = renderer.averageRenderpassDurationMs();
                        const float progressPercentage = renderer.renderProgressPercentage();
                        ImGui::Text(
                            "render pass draw: %.2f ms (%.1f FPS)",
                            drawAverageMs,
                            1000.0f / drawAverageMs);
                        ImGui::Text(
                            "render pass: %.2f ms (%.1f FPS)",
                            renderAverageMs,
                            1000.0f / renderAverageMs);
                        ImGui::Text("render progress: %.2f %%", progressPercentage);
                    }
                    ImGui::Separator();

                    ImGui::Text("Parameters");

                    ImGui::Text("num samples:");
                    ImGui::SameLine();
                    ImGui::RadioButton("64", &numSamplesPerPixel, 64);
                    ImGui::SameLine();
                    ImGui::RadioButton("128", &numSamplesPerPixel, 128);
                    ImGui::SameLine();
                    ImGui::RadioButton("256", &numSamplesPerPixel, 256);

                    ImGui::Text("num bounces:");
                    ImGui::SameLine();
                    ImGui::RadioButton("4", &numBounces, 4);
                    ImGui::SameLine();
                    ImGui::RadioButton("8", &numBounces, 8);
                    ImGui::SameLine();
                    ImGui::RadioButton("16", &numBounces, 16);

                    ImGui::SliderFloat("sun zenith", &sunZenithDegrees, 0.0f, 90.0f, "%.2f");
                    ImGui::SliderFloat("sun azimuth", &sunAzimuthDegrees, 0.0f, 360.0f, "%.2f");

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
                    ImGui::Text("Camera");
                    {
                        const glm::vec3 camPos = cameraController.position();
                        const auto      camYaw = cameraController.yaw();
                        const auto      camPitch = cameraController.pitch();
                        ImGui::Text("position: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
                        ImGui::Text("yaw: %.2f", camYaw.asDegrees());
                        ImGui::Text("pitch: %.2f", camPitch.asDegrees());
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
                        cameraController.update(window.ptr(), deltaTime);
                    }
                }

                // Render

                {
                    const nlrs::RenderParameters renderParams{
                        nlrs::Extent2u(window.resolution()),
                        cameraController.getCamera(),
                        nlrs::SamplingParams{
                            static_cast<std::uint32_t>(numSamplesPerPixel),
                            static_cast<std::uint32_t>(numBounces),
                        },
                        nlrs::Sky{
                            skyTurbidity,
                            skyAlbedo,
                            sunZenithDegrees,
                            sunAzimuthDegrees,
                        },
                    };
                    renderer.setRenderParameters(renderParams);
                    renderer.render(gpuContext, gui);
                }

                wgpuSwapChainPresent(gpuContext.swapChain);
            }
        }
    }

    return 0;
}
