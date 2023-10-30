cmake_minimum_required(VERSION 3.15)

project(pt-playground)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

add_executable(pt
    src/main.cpp
    src/gpu_context.cpp
    src/renderer.cpp)
target_link_libraries(pt PRIVATE glfw glfw3webgpu glm::glm webgpu)
set_target_properties(pt PROPERTIES COMPILE_WARNING_AS_ERROR ON)

if(MSVC)
    # /wd4201 silences warning about nameless structs and unions used by GLM.
    target_compile_options(pt PRIVATE /W4 /WX /wd4201)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # -Wno-deprecated silences warnings about deprecated use of volatile qualifier by GLM.
    target_compile_options(pt PRIVATE -W -Wall -Wextra -pedantic -Werror -Wno-deprecated)
endif()
