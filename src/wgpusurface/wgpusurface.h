#pragma once

#include <webgpu/webgpu.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GLFWwindow GLFWwindow;

// Get a WGPUSurfaceDescriptor from a GLFW window.
WGPUSurfaceDescriptor glfwGetWGPUSurfaceDescriptor(GLFWwindow* window);

#ifdef __cplusplus
}
#endif
