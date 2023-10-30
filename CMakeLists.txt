cmake_minimum_required(VERSION 3.15)

project(pt-playground)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

add_executable(pt src/main.cpp)
target_link_libraries(pt PRIVATE glfw glfw3webgpu webgpu)
set_target_properties(pt PROPERTIES COMPILE_WARNING_AS_ERROR ON)
