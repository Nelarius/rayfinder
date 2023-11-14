#include "gpu_context.hpp"
#include "gui.hpp"

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_wgpu.h>

#include <stdexcept>

namespace nlrs
{
Gui::Gui(GLFWwindow* window, const GpuContext& gpuContext)
    : mImGuiContext(nullptr)
{
    IMGUI_CHECKVERSION();
    mImGuiContext = ImGui::CreateContext();

    if (!mImGuiContext)
    {
        throw std::runtime_error("Failed to create ImGui context.");
    }

    ImGui::SetCurrentContext(mImGuiContext);
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window, true);

    const int               swapchainFramesInFlight = 3;
    const WGPUTextureFormat depthStencilTextureFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(
        gpuContext.device,
        swapchainFramesInFlight,
        GpuContext::swapChainFormat,
        depthStencilTextureFormat);
}

Gui::~Gui()
{
    if (mImGuiContext)
    {
        ImGui::SetCurrentContext(mImGuiContext);
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext(mImGuiContext);
        mImGuiContext = nullptr;
    }
}

void Gui::beginFrame()
{
    ImGui::SetCurrentContext(mImGuiContext);
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Gui::render(const WGPURenderPassEncoder renderPassEncoder)
{
    ImGui::SetCurrentContext(mImGuiContext);
    ImGui::Render();
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), renderPassEncoder);
}
} // namespace nlrs
