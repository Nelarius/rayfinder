cmake_minimum_required(VERSION 3.15)

project(pt-playground)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external)

if(MSVC)
    # /wd4201 silences warning about nameless structs and unions used by GLM.
    add_compile_options(/W4 /WX /wd4201)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    # -Wno-deprecated silences warnings about deprecated use of volatile qualifier by GLM.
    add_compile_options(-W -Wall -Wextra -pedantic -Werror -Wno-deprecated)
endif()

# common
set(COMMON_SOURCE_FILES
    bvh.cpp
    camera.cpp
    cgltf.c
    gltf_model.cpp
    ray_intersection.cpp
    stb_image.c
    texture.cpp)
list(TRANSFORM COMMON_SOURCE_FILES PREPEND src/common/)

add_library(common ${COMMON_SOURCE_FILES})
target_include_directories(common PUBLIC ${CMAKE_SOURCE_DIR}/src ${CGLTF_INCLUDE_DIR} ${STB_INCLUDE_DIR})
target_link_libraries(common PRIVATE glm::glm)

# pt
set(PT_SOURCE_FILES
    fly_camera_controller.cpp
    main.cpp
    gpu_buffer.cpp
    gpu_context.cpp
    renderer.cpp
    window.cpp)
list(TRANSFORM PT_SOURCE_FILES PREPEND src/pt/)

add_executable(pt ${PT_SOURCE_FILES})
target_link_libraries(pt PRIVATE common glfw glfw3webgpu glm::glm imgui webgpu)
set_target_properties(pt PROPERTIES COMPILE_WARNING_AS_ERROR ON)
add_custom_command(
    TARGET pt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/src/pt/raytracer.wgsl
    $<TARGET_FILE_DIR:pt>/raytracer.wgsl
    COMMENT "Copying $<TARGET_FILE_DIR:pt>/raytracer.wgsl"
)
add_custom_command(
    TARGET pt POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/assets/Duck.glb
    $<TARGET_FILE_DIR:pt>/Duck.glb
    COMMENT "Copying $<TARGET_FILE_DIR:pt>/Duck.glb"
)

# bvh-visualizer
add_executable(bvh-visualizer src/bvh-visualizer/main.cpp)
target_link_libraries(bvh-visualizer PRIVATE common glm::glm)
add_custom_command(
    TARGET bvh-visualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/assets/Duck.glb
    $<TARGET_FILE_DIR:bvh-visualizer>/Duck.glb
    COMMENT "Copying $<TARGET_FILE_DIR:bvh-visualizer>/Duck.glb"
)

# tests
set(TESTS_SOURCE_FILES
    aabb.cpp
    angle.cpp
    bvh.cpp
    gltf.cpp
    intersection.cpp)
list(TRANSFORM TESTS_SOURCE_FILES PREPEND src/tests/)

add_executable(tests ${TESTS_SOURCE_FILES})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain common glm::glm)
add_custom_command(
    TARGET tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_SOURCE_DIR}/assets/Duck.glb
    $<TARGET_FILE_DIR:tests>/Duck.glb
    COMMENT "Copying $<TARGET_FILE_DIR:tests>/Duck.glb"
)
