#include "fly_camera_controller.hpp"
#include "gpu_context.hpp"
#include "gui.hpp"
#include "hybrid_renderer.hpp"
#include "reference_path_tracer.hpp"
#include "texture_blit_renderer.hpp"
#include "window.hpp"

#include <common/assert.hpp>
#include <common/bvh.hpp>
#include <common/file_stream.hpp>
#include <common/flattened_model.hpp>
#include <common/gltf_model.hpp>
#include <common/ray_intersection.hpp>
#include <common/triangle_attributes.hpp>
#include <pt-format/vertex_attributes.hpp>
#include <pt-format/pt_format.hpp>

#include <fmt/core.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <webgpu/webgpu.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <tuple>
#include <utility>

namespace fs = std::filesystem;

inline constexpr int defaultWindowWidth = 640;
inline constexpr int defaultWindowHeight = 480;

void printHelp() { std::printf("Usage:\n\tpt <input_pt_file>\n"); }

enum RendererType
{
    RendererType_PathTracer,
    RendererType_Hybrid,
    RendererType_Debug,
};

struct UiState
{
    int   rendererType = RendererType_Hybrid;
    float vfovDegrees = 70.0f;
    // sampling
    int numSamplesPerPixel = 128;
    int numBounces = 2;
    // sky
    float                sunZenithDegrees = 30.0f;
    float                sunAzimuthDegrees = 0.0f;
    float                skyTurbidity = 1.0f;
    std::array<float, 3> skyAlbedo = {1.0f, 1.0f, 1.0f};
    // tonemapping
    int exposureStops = 2;
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

nlrs::Extent2i largestMonitorResolution()
{
    int           monitorCount;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);

    NLRS_ASSERT(monitorCount > 0);

    int            maxArea = 0;
    nlrs::Extent2i maxResolution;

    for (int i = 0; i < monitorCount; ++i)
    {
        GLFWmonitor* const monitor = monitors[i];

        float xscale, yscale;
        glfwGetMonitorContentScale(monitor, &xscale, &yscale);

        const GLFWvidmode* const mode = glfwGetVideoMode(monitor);

        const int xpixels = static_cast<int>(xscale * mode->width + 0.5f);
        const int ypixels = static_cast<int>(yscale * mode->height + 0.5f);
        const int area = xpixels * ypixels;

        if (area > maxArea)
        {
            maxArea = area;
            maxResolution = nlrs::Extent2i(xpixels, ypixels);
        }
    }

    return maxResolution;
}

int main(int argc, char** argv)
try
{
    if (argc != 2)
    {
        printHelp();
        return 0;
    }

    nlrs::GpuContext gpuContext{nlrs::ReferencePathTracer::wgpuRequiredLimits};
    nlrs::Window     window = [&gpuContext]() -> nlrs::Window {
        const nlrs::WindowDescriptor windowDesc{
                .windowSize = nlrs::Extent2i{defaultWindowWidth, defaultWindowHeight},
                .title = "pt-playground 🛝",
        };
        return nlrs::Window{windowDesc, gpuContext};
    }();

    nlrs::Gui gui(window.ptr(), gpuContext);

    nlrs::TextureBlitRenderer textureBlitter{
        gpuContext,
        nlrs::TextureBlitRendererDescriptor{
            .framebufferSize = nlrs::Extent2u(window.resolution())}};

    nlrs::HybridRenderer hybridRenderer = [&gpuContext, &window, argv]() -> nlrs::HybridRenderer {
        fs::path path = argv[1];
        path.replace_extension(".glb");
        if (!fs::exists(path))
        {
            fmt::print(stderr, "File {} does not exist\n", path.string());
            std::exit(1);
        }
        nlrs::GltfModel gltf{path};

        const auto [numModelVertices, numModelIndices] =
            [&gltf]() -> std::tuple<std::size_t, std::size_t> {
            std::size_t numModelVertices = 0;
            std::size_t numModelIndices = 0;
            for (const auto& mesh : gltf.meshes)
            {
                numModelVertices += mesh.positions.size();
                numModelIndices += mesh.indices.size();
                NLRS_ASSERT(mesh.positions.size() == mesh.texCoords.size());
            }
            return std::make_tuple(numModelVertices, numModelIndices);
        }();

        std::vector<glm::vec4> flattenedVertices;
        flattenedVertices.reserve(numModelVertices);
        std::vector<std::span<const glm::vec4>> modelVertices;

        std::vector<glm::vec4> flattenedNormals;
        flattenedNormals.reserve(numModelVertices);
        std::vector<std::span<const glm::vec4>> modelNormals;

        std::vector<glm::vec2> flattenedTexCoords;
        flattenedTexCoords.reserve(numModelVertices);
        std::vector<std::span<const glm::vec2>> modelTexCoords;

        std::vector<std::uint32_t> flattenedIndices;
        flattenedIndices.reserve(numModelIndices);
        std::vector<std::span<const std::uint32_t>> modelIndices;

        std::vector<std::size_t> baseColorTextureIndices;

        for (const auto& mesh : gltf.meshes)
        {
            const std::size_t vertexOffsetIdx = flattenedVertices.size();
            const std::size_t numVertices = mesh.positions.size();

            NLRS_ASSERT(mesh.positions.size() == mesh.normals.size());
            NLRS_ASSERT(mesh.positions.size() == mesh.texCoords.size());

            std::transform(
                mesh.positions.begin(),
                mesh.positions.end(),
                std::back_inserter(flattenedVertices),
                [](const glm::vec3& v) -> glm::vec4 { return glm::vec4(v, 1.0f); });
            modelVertices.emplace_back(flattenedVertices.data() + vertexOffsetIdx, numVertices);

            std::transform(
                mesh.normals.begin(),
                mesh.normals.end(),
                std::back_inserter(flattenedNormals),
                [](const glm::vec3& n) -> glm::vec4 { return glm::vec4(n, 0.0f); });
            modelNormals.emplace_back(flattenedNormals.data() + vertexOffsetIdx, numVertices);

            flattenedTexCoords.insert(
                flattenedTexCoords.end(), mesh.texCoords.begin(), mesh.texCoords.end());
            modelTexCoords.emplace_back(flattenedTexCoords.data() + vertexOffsetIdx, numVertices);

            const std::size_t indexOffsetIdx = flattenedIndices.size();
            const std::size_t numIndices = mesh.indices.size();
            flattenedIndices.insert(
                flattenedIndices.end(), mesh.indices.begin(), mesh.indices.end());
            modelIndices.emplace_back(flattenedIndices.data() + indexOffsetIdx, numIndices);

            baseColorTextureIndices.push_back(mesh.baseColorTextureIndex);
        }

        return nlrs::HybridRenderer{
            gpuContext,
            nlrs::HybridRendererDescriptor{
                .framebufferSize = nlrs::Extent2u(window.resolution()),
                .modelPositions = modelVertices,
                .modelNormals = modelNormals,
                .modelTexCoords = modelTexCoords,
                .modelIndices = modelIndices,
                .baseColorTextureIndices = baseColorTextureIndices,
                .baseColorTextures = gltf.baseColorTextures,
            }};
    }();

    auto [appState, renderer] =
        [&gpuContext, &window, argv]() -> std::tuple<AppState, nlrs::ReferencePathTracer> {
        nlrs::PtFormat ptFormat;
        {
            const fs::path path = argv[1];
            if (!fs::exists(path))
            {
                fmt::print(stderr, "File {} does not exist\n", path.string());
                std::exit(1);
            }
            nlrs::InputFileStream file(argv[1]);
            nlrs::deserialize(file, ptFormat);
        }

        const nlrs::RendererDescriptor rendererDesc{
            nlrs::RenderParameters{
                nlrs::Extent2u(window.resolution()),
                nlrs::FlyCameraController{}.getCamera(),
                nlrs::SamplingParams(),
                nlrs::Sky()},
            largestMonitorResolution(),
        };

        nlrs::Scene scene{
            .bvhNodes = ptFormat.bvhNodes,
            .positionAttributes = ptFormat.gpuPositionAttributes,
            .vertexAttributes = ptFormat.gpuVertexAttributes,
            .baseColorTextures = ptFormat.baseColorTextures,
        };

        AppState app{
            .cameraController{},
            .bvhNodes = std::move(ptFormat.bvhNodes),
            .positions = std::move(ptFormat.bvhPositionAttributes),
            .ui = UiState{},
            .focusPressed = false,
        };

        nlrs::ReferencePathTracer renderer{rendererDesc, gpuContext, std::move(scene)};

        return std::make_tuple(std::move(app), std::move(renderer));
    }();

    auto onNewFrame = [&gui]() -> void { gui.beginFrame(); };

    auto onUpdate = [&appState, &renderer](GLFWwindow* windowPtr, float deltaTime) -> void {
        {
            // Skip input if ImGui captured input
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                appState.cameraController.update(windowPtr, deltaTime);
            }

            // Check if mouse button pressed
            if (glfwGetMouseButton(windowPtr, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS &&
                !appState.focusPressed)
            {
                appState.focusPressed = true;

                double x, y;
                glfwGetCursorPos(windowPtr, &x, &y);
                nlrs::Extent2i windowSize;
                glfwGetWindowSize(windowPtr, &windowSize.x, &windowSize.y);
                if ((x >= 0.0 && x < static_cast<double>(windowSize.x)) &&
                    (y >= 0.0 && y < static_cast<double>(windowSize.y)))
                {
                    const float u = static_cast<float>(x) / static_cast<float>(windowSize.x);
                    const float v = 1.f - static_cast<float>(y) / static_cast<float>(windowSize.y);

                    const auto camera = appState.cameraController.getCamera();
                    const auto ray = nlrs::generateCameraRay(camera, u, v);

                    nlrs::Intersection hitData;
                    if (nlrs::rayIntersectBvh(
                            ray, appState.bvhNodes, appState.positions, 1000.f, hitData, nullptr))
                    {
                        const glm::vec3 dir = hitData.p - appState.cameraController.position();
                        const glm::vec3 cameraForward =
                            appState.cameraController.orientation().forward;
                        const float focusDistance = glm::dot(dir, cameraForward);
                        appState.cameraController.focusDistance() = focusDistance;
                    }
                }
            }

            if (glfwGetMouseButton(windowPtr, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_RELEASE)
            {
                appState.focusPressed = false;
            }
        }
        {
            ImGui::Begin("pt");

            ImGui::Text("Renderer");
            ImGui::RadioButton("path tracer", &appState.ui.rendererType, RendererType_PathTracer);
            ImGui::SameLine();
            ImGui::RadioButton("hybrid", &appState.ui.rendererType, RendererType_Hybrid);
            ImGui::SameLine();
            ImGui::RadioButton("debug", &appState.ui.rendererType, RendererType_Debug);
            ImGui::Separator();

            ImGui::Text("Renderer stats");
            {
                const float renderAverageMs = renderer.averageRenderpassDurationMs();
                const float progressPercentage = renderer.renderProgressPercentage();
                ImGui::Text(
                    "render pass: %.2f ms (%.1f FPS)", renderAverageMs, 1000.0f / renderAverageMs);
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

            ImGui::SliderFloat("sun zenith", &appState.ui.sunZenithDegrees, 0.0f, 90.0f, "%.2f");
            ImGui::SliderFloat("sun azimuth", &appState.ui.sunAzimuthDegrees, 0.0f, 360.0f, "%.2f");
            ImGui::SliderFloat("sky turbidity", &appState.ui.skyTurbidity, 1.0f, 10.0f, "%.2f");

            ImGui::SliderFloat(
                "camera speed",
                &appState.cameraController.speed(),
                0.05f,
                100.0f,
                "%.2f",
                ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat("camera vfov", &appState.ui.vfovDegrees, 10.0f, 120.0f);
            appState.cameraController.vfov() = nlrs::Angle::degrees(appState.ui.vfovDegrees);
            ImGui::SliderFloat(
                "camera focus distance",
                &appState.cameraController.focusDistance(),
                0.1f,
                50.0f,
                "%.2f",
                ImGuiSliderFlags_Logarithmic);
            ImGui::SliderFloat(
                "camera lens radius", &appState.cameraController.aperture(), 0.0f, 0.5f, "%.2f");

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
    };

    auto onRender = [&appState, &gpuContext, &gui, &renderer, &hybridRenderer, &textureBlitter](
                        GLFWwindow* windowPtr, WGPUSwapChain swapChain) -> void {
        nlrs::Extent2i windowResolution;
        glfwGetFramebufferSize(windowPtr, &windowResolution.x, &windowResolution.y);
        const nlrs::RenderParameters renderParams{
            nlrs::Extent2u(windowResolution),
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

        switch (appState.ui.rendererType)
        {
        case RendererType_PathTracer:
            renderer.render(gpuContext, textureBlitter.textureView());
            break;
        case RendererType_Hybrid:
        {
            const glm::mat4 viewProjectionMat = appState.cameraController.viewProjectionMatrix();
            const nlrs::Sky sky{
                appState.ui.skyTurbidity,
                appState.ui.skyAlbedo,
                appState.ui.sunZenithDegrees,
                appState.ui.sunAzimuthDegrees,
            };
            hybridRenderer.render(
                gpuContext,
                viewProjectionMat,
                appState.cameraController.position(),
                sky,
                textureBlitter.textureView());
            break;
        }
        case RendererType_Debug:
        {
            const glm::mat4 viewProjectionMat = appState.cameraController.viewProjectionMatrix();
            hybridRenderer.renderDebug(gpuContext, viewProjectionMat, textureBlitter.textureView());
            break;
        }
        }
        textureBlitter.render(gpuContext, gui, swapChain);
    };

    auto onResize = [&gpuContext, &hybridRenderer, &textureBlitter](
                        const nlrs::FramebufferSize newSize) -> void {
        const auto sz = nlrs::Extent2u(newSize);
        hybridRenderer.resize(gpuContext, sz);
        textureBlitter.resize(gpuContext, sz);
    };

    window.run(
        gpuContext,
        std::move(onNewFrame),
        std::move(onUpdate),
        std::move(onRender),
        std::move(onResize));

    return 0;
}
catch (const std::exception& e)
{
    fmt::println(stderr, "Exception occurred. {}", e.what());
    return 1;
}
catch (...)
{
    fmt::println(stderr, "Unknown exception occurred.");
    return 1;
}