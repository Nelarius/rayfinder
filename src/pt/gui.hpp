#pragma once

#include <imgui.h>
#include <webgpu/webgpu.h>

struct GLFWwindow;

namespace nlrs
{
struct GpuContext;

class Gui
{
public:
    Gui(GLFWwindow*, const GpuContext&);
    ~Gui();

    void beginFrame();
    void render(WGPURenderPassEncoder renderPass);

private:
    ImGuiContext* mImGuiContext = nullptr;
};
} // namespace nlrs
