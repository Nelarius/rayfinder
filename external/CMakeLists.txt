include(FetchContent)

# Declare
FetchContent_Declare(Catch2
    GIT_REPOSITORY "https://github.com/catchorg/Catch2"
    GIT_TAG 6e79e682b726f524310d55dec8ddac4e9c52fb5f) # v.3.4.0

FetchContent_Declare(cgltf
    GIT_REPOSITORY "https://github.com/jkuhlmann/cgltf"
    GIT_TAG c887e783d0f11046adda8848d5bd27aed5f9cb9b) # v.1.13

FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG 7482de6) # 3.3.8

FetchContent_Declare(glfw3webgpu
    GIT_REPOSITORY "https://github.com/eliemichel/glfw3webgpu"
    GIT_TAG cf88329) # v1.0.1

FetchContent_Declare(glm GIT_REPOSITORY "https://github.com/g-truc/glm"
    GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e) # v.0.9.9.8

FetchContent_Declare(imgui GIT_REPOSITORY "https://github.com/ocornut/imgui"
    GIT_TAG c6e0284ac58b3f205c95365478888f7b53b077e2) # v1.89.9

FetchContent_Declare(stb
    GIT_REPOSITORY "https://github.com/nothings/stb"
    GIT_TAG beebb24b945efdea3b9bba23affb8eb3ba8982e7) # Oct 12, 2023

FetchContent_Declare(webgpu
    GIT_REPOSITORY "https://github.com/eliemichel/WebGPU-distribution"
    GIT_TAG 1058c7eba4fdad74adb87192eede272c0c69c39e) # dawn-branch, 2023-09-04

# Fetch
message(STATUS "Fetching Catch2...")
FetchContent_MakeAvailable(Catch2)

message(STATUS "Fetching cgltf...")
FetchContent_MakeAvailable(cgltf)
set(CGLTF_SOURCE_DIR ${cgltf_SOURCE_DIR})

message(STATUS "Fetching glfw...")
FetchContent_MakeAvailable(glfw)

message(STATUS "Fetching glfw3webgpu...")
FetchContent_MakeAvailable(glfw3webgpu)

message(STATUS "Fetching glm...")
FetchContent_MakeAvailable(glm)

message(STATUS "Fetcing imgui...")
FetchContent_MakeAvailable(imgui)
set(IMGUI_SOURCE_DIR ${imgui_SOURCE_DIR})

message(STATUS "Fetching stb...")
FetchContent_MakeAvailable(stb)
set(STB_SOURCE_DIR ${stb_SOURCE_DIR})

message(STATUS "Fetching WebGPU...")
FetchContent_MakeAvailable(webgpu)

# Configure

# cgltf
set(CGLTF_INCLUDE_DIR ${CGLTF_SOURCE_DIR} CACHE INTERNAL "cgltf Include Directory")

# imgui
set(IMGUI_INCLUDE_DIR ${IMGUI_SOURCE_DIR} CACHE INTERNAL "imgui Include Directory")
add_library(imgui STATIC
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
    ${IMGUI_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_SOURCE_DIR} ${IMGUI_SOURCE_DIR}/backends)
target_link_libraries(imgui PRIVATE glfw webgpu)

#stb
set(STB_INCLUDE_DIR ${STB_SOURCE_DIR} CACHE INTERNAL "stb Include Directory")
