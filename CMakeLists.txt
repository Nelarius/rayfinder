cmake_minimum_required(VERSION 3.15)

project(pt-playground)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

add_library(wgpusurface
    src/wgpusurface/wgpusurface.c)
target_link_libraries(wgpusurface PRIVATE glfw webgpu)
target_include_directories(wgpusurface PUBLIC src/wgpusurface)

add_executable(pt
    src/main.cpp
    src/gpu_context.cpp)
target_link_libraries(pt PRIVATE glfw wgpusurface webgpu)
set_target_properties(pt PROPERTIES COMPILE_WARNING_AS_ERROR ON)
