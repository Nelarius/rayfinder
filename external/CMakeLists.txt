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

FetchContent_Declare(webgpu
    GIT_REPOSITORY "https://github.com/eliemichel/WebGPU-distribution"
    GIT_TAG dawn)

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

message(STATUS "Fetching WebGPU...")
FetchContent_MakeAvailable(webgpu)

# Configure

# cgltf
set(CGLTF_INCLUDE_DIR ${CGLTF_SOURCE_DIR} CACHE INTERNAL "cgltf Include Directory")
