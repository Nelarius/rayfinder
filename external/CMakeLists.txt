include(FetchContent)

# Declare
FetchContent_Declare(Catch2
    GIT_REPOSITORY "https://github.com/catchorg/Catch2"
    GIT_TAG 6e79e682b726f524310d55dec8ddac4e9c52fb5f) # v.3.4.0

FetchContent_Declare(cgltf
    GIT_REPOSITORY "https://github.com/jkuhlmann/cgltf"
    GIT_TAG c887e783d0f11046adda8848d5bd27aed5f9cb9b) # v.1.13

FetchContent_Declare(dawn

    # Manual download mode, even shallower than GIT_SHALLOW ON
    DOWNLOAD_COMMAND
    cd ${FETCHCONTENT_BASE_DIR}/dawn-src &&
    git init &&
    git fetch --depth=1 https://dawn.googlesource.com/dawn chromium/6245 &&
    git reset --hard FETCH_HEAD
)

FetchContent_Declare(fmt
    GIT_REPOSITORY "https://github.com/fmtlib/fmt"
    GIT_TAG e69e5f977d458f2650bb346dadf2ad30c5320281) # 10.2.1

FetchContent_Declare(glfw
    GIT_REPOSITORY "https://github.com/glfw/glfw"
    GIT_TAG 7482de6) # 3.3.8

FetchContent_Declare(glm GIT_REPOSITORY "https://github.com/g-truc/glm"
    GIT_TAG bf71a834948186f4097caa076cd2663c69a10e1e) # v.0.9.9.8

FetchContent_Declare(imgui GIT_REPOSITORY "https://github.com/ocornut/imgui"
    GIT_TAG c6e0284ac58b3f205c95365478888f7b53b077e2) # v1.89.9

FetchContent_Declare(stb
    GIT_REPOSITORY "https://github.com/nothings/stb"
    GIT_TAG beebb24b945efdea3b9bba23affb8eb3ba8982e7) # Oct 12, 2023

set(ZSTD_BUILD_STATIC ON)
set(ZSTD_BUILD_SHARED OFF)
FetchContent_Declare(zstd
    GIT_REPOSITORY "https://github.com/facebook/zstd"
    GIT_TAG 63779c798237346c2b245c546c40b72a5a5913fe # v1.5.5 (Apr 5, 2023)
    SOURCE_SUBDIR build/cmake)

# Fetch
message(STATUS "Fetching Catch2...")
FetchContent_MakeAvailable(Catch2)

message(STATUS "Fetching cgltf...")
FetchContent_MakeAvailable(cgltf)
set(CGLTF_SOURCE_DIR ${cgltf_SOURCE_DIR})

message(STATUS "Fetching dawn...")

if(NOT dawn_POPULATED)
    FetchContent_Populate(dawn)

    # This option replaces depot_tools
    set(DAWN_FETCH_DEPENDENCIES ON)

    # A more minimalistic choice of backend than Dawn's default
    if(APPLE)
        set(USE_VULKAN OFF)
        set(USE_METAL ON)
        add_definitions(
            -Wno-deprecated-builtins # Warning from Abseil
            -Wno-unknown-warning-option # SPIRV-Tools
        )
    else()
        set(USE_VULKAN ON)
        set(USE_METAL OFF)
    endif()

    set(DAWN_ENABLE_D3D11 OFF)
    set(DAWN_ENABLE_D3D12 OFF)
    set(DAWN_ENABLE_METAL ${USE_METAL})
    set(DAWN_ENABLE_NULL OFF)
    set(DAWN_ENABLE_DESKTOP_GL OFF)
    set(DAWN_ENABLE_OPENGLES OFF)
    set(DAWN_ENABLE_VULKAN ${USE_VULKAN})

    # Disable unneeded parts
    set(DAWN_BUILD_SAMPLES OFF)
    set(DAWN_USE_GLFW OFF)
    set(TINT_BUILD_TINT OFF)
    set(TINT_BUILD_CMD_TOOLS OFF)
    set(TINT_BUILD_SAMPLES OFF)
    set(TINT_BUILD_DOCS OFF)
    set(TINT_BUILD_TESTS OFF)
    set(TINT_BUILD_FUZZERS OFF)
    set(TINT_BUILD_SPIRV_TOOLS_FUZZER OFF)
    set(TINT_BUILD_AST_FUZZER OFF)
    set(TINT_BUILD_REGEX_FUZZER OFF)
    set(TINT_BUILD_BENCHMARKS OFF)
    set(TINT_BUILD_AS_OTHER_OS OFF)
    set(TINT_BUILD_REMOTE_COMPILE OFF)

    # Disable unneeded Tint functionality
    set(TINT_BUILD_GLSL_WRITER OFF)
    set(TINT_BUILD_SPV_READER OFF)

    add_subdirectory(${dawn_SOURCE_DIR} ${dawn_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

message(STATUS "Fetching fmt...")
FetchContent_MakeAvailable(fmt)

message(STATUS "Fetching glfw...")
FetchContent_MakeAvailable(glfw)

message(STATUS "Fetching glm...")
FetchContent_MakeAvailable(glm)

message(STATUS "Fetcing imgui...")
FetchContent_MakeAvailable(imgui)
set(IMGUI_SOURCE_DIR ${imgui_SOURCE_DIR})

message(STATUS "Fetching stb...")
FetchContent_MakeAvailable(stb)
set(STB_SOURCE_DIR ${stb_SOURCE_DIR})

message(STATUS "Fetching zstd...")
FetchContent_MakeAvailable(zstd)

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
target_link_libraries(imgui PRIVATE glfw webgpu_dawn)

# stb
set(STB_INCLUDE_DIR ${STB_SOURCE_DIR} CACHE INTERNAL "stb Include Directory")

# zstd
# NOTE: zstd_SOURCE_DIR points to zstd/build/cmake
set(ZSTD_INCLUDE_DIR ${zstd_SOURCE_DIR}/lib CACHE INTERNAL "zstd Include Directory")
