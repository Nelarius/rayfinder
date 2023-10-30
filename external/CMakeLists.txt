include(FetchContent)

# Declare
FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG 7482de6) # 3.3.8

FetchContent_Declare(webgpu
    GIT_REPOSITORY "https://github.com/eliemichel/WebGPU-distribution"
    GIT_TAG dawn)

# Fetch
message(STATUS "Fetching glfw...")
FetchContent_MakeAvailable(glfw)

message(STATUS "Fetching WebGPU...")
FetchContent_MakeAvailable(webgpu)

# Configure
