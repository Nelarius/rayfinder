#include "wgpusurface.h"

// This implementation is based on glfw3webgpu, but just returns the descriptor.
// https://github.com/eliemichel/glfw3webgpu

#define WGPU_TARGET_MACOS 1
#define WGPU_TARGET_WINDOWS 2

#if defined(_WIN32)
#define WGPU_TARGET WGPU_TARGET_WINDOWS
#elif defined(__APPLE__)
#define WGPU_TARGET WGPU_TARGET_MACOS
#else
#error "Unsupported WGPU_TARGET"
#endif

#if WGPU_TARGET == WGPU_TARGET_MACOS
#include <Foundation/Foundation.h>
#include <QuartzCore/CAMetalLayer.h>
#endif

#include <GLFW/glfw3.h>
#if WGPU_TARGET == WGPU_TARGET_MACOS
#define GLFW_EXPOSE_NATIVE_COCOA
#elif WGPU_TARGET == WGPU_TARGET_WINDOWS
#define GLFW_EXPOSE_NATIVE_WIN32
#endif
#include <GLFW/glfw3native.h>

WGPUSurfaceDescriptor glfwGetWGPUSurfaceDescriptor(GLFWwindow* window)
{
#if WGPU_TARGET == WGPU_TARGET_MACOS
    {
        id        metal_layer = NULL;
        NSWindow* ns_window = glfwGetCocoaWindow(window);
        [ns_window.contentView setWantsLayer:YES];
        metal_layer = [CAMetalLayer layer];
        [ns_window.contentView setLayer:metal_layer];
        return (WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct*)&(WGPUSurfaceDescriptorFromMetalLayer){
                    .chain =
                        (WGPUChainedStruct){
                            .next = NULL,
                            .sType = WGPUSType_SurfaceDescriptorFromMetalLayer,
                        },
                    .layer = metal_layer,
                },
            .label = "MacOS GLFW surface descriptor",
        };
    }
#elif WGPU_TARGET == WGPU_TARGET_WINDOWS
    {
        HWND      hwnd = glfwGetWin32Window(window);
        HINSTANCE hinstance = GetModuleHandle(NULL);

        return (WGPUSurfaceDescriptor){
            .nextInChain =
                (const WGPUChainedStruct*)&(WGPUSurfaceDescriptorFromWindowsHWND){
                    .chain =
                        (WGPUChainedStruct){
                            .next = NULL,
                            .sType = WGPUSType_SurfaceDescriptorFromWindowsHWND,
                        },
                    .hinstance = hinstance,
                    .hwnd = hwnd,
                },
            .label = "Windows GLFW surface descriptor",
        };
    }
#endif
}
