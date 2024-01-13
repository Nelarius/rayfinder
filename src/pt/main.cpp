#include "fly_camera_controller.hpp"
#include "gpu_context.hpp"
#include "gui.hpp"
#include "renderer.hpp"
#include "window.hpp"

#include <common/bvh.hpp>
#include <common/gltf_model.hpp>
#include <common/ray_intersection.hpp>
#include <common/triangle_attributes.hpp>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <webgpu/webgpu.h>

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <tuple>
#include <utility>

inline constexpr int defaultWindowWidth = 640;
inline constexpr int defaultWindowHeight = 480;

void printHelp() { std::printf("Usage: pt <input_gltf_file>\n"); }

struct UiState
{
    float vfovDegrees = 70.0f;
    float focusDistance = 5.0f;
    // sampling
    int numSamplesPerPixel = 128;
    int numBounces = 4;
    // sky
    float                sunZenithDegrees = 30.0f;
    float                sunAzimuthDegrees = 0.0f;
    float                skyTurbidity = 1.0f;
    std::array<float, 3> skyAlbedo = {1.0f, 1.0f, 1.0f};
    // tonemapping
    int exposureStops = 3;
    int tonemapFn = 1;
};

struct AppState
{
    nlrs::FlyCameraController    cameraController;
    std::vector<nlrs::BvhNode>   bvhNodes;
    std::vector<nlrs::Positions> positions;
    UiState                      ui;
    bool                         focusPressed = false;
};

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
        nlrs::GpuContext gpuContext(window.ptr(), nlrs::Renderer::wgpuRequiredLimits);
        nlrs::Gui        gui(window.ptr(), gpuContext);

        auto [appState, renderer] =
            [&gpuContext, &window, argv]() -> std::tuple<AppState, nlrs::Renderer> {
            const nlrs::GltfModel model(argv[1]);
            auto [bvhNodes, triangleIndices] = nlrs::buildBvh(model.positions());

            const auto positions = nlrs::reorderAttributes(model.positions(), triangleIndices);
            const auto normals = nlrs::reorderAttributes(model.normals(), triangleIndices);
            const auto texCoords = nlrs::reorderAttributes(model.texCoords(), triangleIndices);
            const auto textureIndices =
                nlrs::reorderAttributes(model.baseColorTextureIndices(), triangleIndices);

            assert(positions.size() == normals.size());
            assert(positions.size() == texCoords.size());
            assert(positions.size() == textureIndices.size());
            std::vector<nlrs::PositionAttribute> positionAttributes;
            std::vector<nlrs::VertexAttributes>  vertexAttributes;
            positionAttributes.reserve(positions.size());
            vertexAttributes.reserve(positions.size());
            for (std::size_t i = 0; i < normals.size(); ++i)
            {
                const auto& ps = positions[i];
                const auto& ns = normals[i];
                const auto& uvs = texCoords[i];
                const auto  textureIdx = textureIndices[i];

                positionAttributes.push_back(
                    nlrs::PositionAttribute{.p0 = ps.v0, .p1 = ps.v1, .p2 = ps.v2});
                vertexAttributes.push_back(nlrs::VertexAttributes{
                    .n0 = ns.n0,
                    .n1 = ns.n1,
                    .n2 = ns.n2,
                    .uv0 = uvs.uv0,
                    .uv1 = uvs.uv1,
                    .uv2 = uvs.uv2,
                    .textureIdx = textureIdx});
            }

            const nlrs::RendererDescriptor rendererDesc{
                nlrs::RenderParameters{
                    nlrs::Extent2u(window.resolution()),
                    nlrs::FlyCameraController{}.getCamera(),
                    nlrs::SamplingParams(),
                    nlrs::Sky()},
                window.largestMonitorResolution(),
            };

            nlrs::Scene scene{
                .bvhNodes = bvhNodes,
                .positionAttributes = positionAttributes,
                .vertexAttributes = vertexAttributes,
                .baseColorTextures = model.baseColorTextures(),
            };

            AppState app{
                .cameraController{},
                .bvhNodes = std::move(bvhNodes),
                .positions = std::move(positions),
                .ui = UiState{},
                .focusPressed = false,
            };

            nlrs::Renderer renderer{rendererDesc, gpuContext, std::move(scene)};

            return std::make_tuple(std::move(app), std::move(renderer));
        }();

        {
            nlrs::Extent2i curFramebufferSize = window.resolution();
            auto           lastTime = std::chrono::steady_clock::now();
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
                        const float renderAverageMs = renderer.averageRenderpassDurationMs();
                        const float progressPercentage = renderer.renderProgressPercentage();
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
                    ImGui::RadioButton("64", &appState.ui.numSamplesPerPixel, 64);
                    ImGui::SameLine();
                    ImGui::RadioButton("128", &appState.ui.numSamplesPerPixel, 128);
                    ImGui::SameLine();
                    ImGui::RadioButton("256", &appState.ui.numSamplesPerPixel, 256);

                    ImGui::Text("num bounces:");
                    ImGui::SameLine();
                    ImGui::RadioButton("2", &appState.ui.numBounces, 2);
                    ImGui::SameLine();
                    ImGui::RadioButton("4", &appState.ui.numBounces, 4);
                    ImGui::SameLine();
                    ImGui::RadioButton("8", &appState.ui.numBounces, 8);

                    ImGui::SliderFloat(
                        "sun zenith", &appState.ui.sunZenithDegrees, 0.0f, 90.0f, "%.2f");
                    ImGui::SliderFloat(
                        "sun azimuth", &appState.ui.sunAzimuthDegrees, 0.0f, 360.0f, "%.2f");
                    ImGui::SliderFloat(
                        "sky turbidity", &appState.ui.skyTurbidity, 1.0f, 10.0f, "%.2f");

                    ImGui::SliderFloat(
                        "camera speed",
                        &appState.cameraController.speed(),
                        0.05f,
                        100.0f,
                        "%.2f",
                        ImGuiSliderFlags_Logarithmic);
                    ImGui::SliderFloat("camera vfov", &appState.ui.vfovDegrees, 10.0f, 120.0f);
                    appState.cameraController.vfov() =
                        nlrs::Angle::degrees(appState.ui.vfovDegrees);
                    ImGui::SliderFloat(
                        "camera focus distance",
                        &appState.ui.focusDistance,
                        0.1f,
                        50.0f,
                        "%.2f",
                        ImGuiSliderFlags_Logarithmic);

                    ImGui::Separator();
                    ImGui::Text("Post processing");

                    ImGui::SliderInt("exposure stops", &appState.ui.exposureStops, 0, 10);
                    ImGui::Text("tonemap fn");
                    ImGui::SameLine();
                    ImGui::RadioButton("linear", &appState.ui.tonemapFn, 0);
                    ImGui::SameLine();
                    ImGui::RadioButton("filmic", &appState.ui.tonemapFn, 1);

                    ImGui::Separator();
                    ImGui::Text("Camera");
                    {
                        const glm::vec3 pos = appState.cameraController.position();
                        const auto      yaw = appState.cameraController.yaw();
                        const auto      pitch = appState.cameraController.pitch();
                        ImGui::Text("position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
                        ImGui::Text("yaw: %.2f", yaw.asDegrees());
                        ImGui::Text("pitch: %.2f", pitch.asDegrees());
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
                        appState.cameraController.update(window.ptr(), deltaTime);
                    }

                    // Check if mouse button pressed
                    if (glfwGetMouseButton(window.ptr(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
                        !appState.focusPressed)
                    {
                        appState.focusPressed = true;

                        double x, y;
                        glfwGetCursorPos(window.ptr(), &x, &y);

                        const auto windowResolution = window.resolution();

                        const float u =
                            static_cast<float>(x) / static_cast<float>(windowResolution.x);
                        const float v =
                            1.f - static_cast<float>(y) / static_cast<float>(windowResolution.y);

                        const auto camera = appState.cameraController.getCamera();
                        const auto ray = nlrs::generateCameraRay(camera, u, v);

                        nlrs::Intersection hitData;
                        if (nlrs::rayIntersectBvh(
                                ray,
                                appState.bvhNodes,
                                appState.positions,
                                1000.f,
                                hitData,
                                nullptr))
                        {
                            const float focusDistance =
                                glm::distance(appState.cameraController.position(), hitData.p);
                            appState.ui.focusDistance = focusDistance;
                        }
                    }

                    if (glfwGetMouseButton(window.ptr(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
                    {
                        appState.focusPressed = false;
                    }
                }

                // Render

                {
                    const nlrs::RenderParameters renderParams{
                        nlrs::Extent2u(window.resolution()),
                        appState.cameraController.getCamera(),
                        nlrs::SamplingParams{
                            static_cast<std::uint32_t>(appState.ui.numSamplesPerPixel),
                            static_cast<std::uint32_t>(appState.ui.numBounces),
                        },
                        nlrs::Sky{
                            appState.ui.skyTurbidity,
                            appState.ui.skyAlbedo,
                            appState.ui.sunZenithDegrees,
                            appState.ui.sunAzimuthDegrees,
                        },
                    };
                    renderer.setRenderParameters(renderParams);

                    const nlrs::PostProcessingParameters postProcessingParams{
                        static_cast<std::uint32_t>(appState.ui.exposureStops),
                        static_cast<nlrs::Tonemapping>(appState.ui.tonemapFn),
                    };
                    renderer.setPostProcessingParameters(postProcessingParams);

                    renderer.render(gpuContext, gui);
                }

                wgpuSwapChainPresent(gpuContext.swapChain);
            }
        }
    }

    return 0;
}
