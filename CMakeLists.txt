cmake_minimum_required(VERSION 4.0.0)
project(AstraTrace VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_subdirectory(vendor/SDL3 EXCLUDE_FROM_ALL)
add_subdirectory(vendor/glm EXCLUDE_FROM_ALL)
find_package(OpenMP)
find_package(Python3 COMPONENTS Interpreter)

add_library(mikktspace STATIC
    vendor/mikktspace/mikktspace.c
)
target_include_directories(mikktspace PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mikktspace
)

add_library(imgui STATIC
    vendor/imgui/imgui.cpp
    vendor/imgui/imgui_draw.cpp
    vendor/imgui/imgui_tables.cpp
    vendor/imgui/imgui_widgets.cpp
    vendor/imgui/backends/imgui_impl_sdl3.cpp
    vendor/imgui/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui/backends
)
target_link_libraries(imgui PUBLIC SDL3::SDL3-shared)

set(ASTRATRACE_PUBLIC_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
set(ASTRATRACE_VENDOR_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyobjloader
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinygltf
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json/single_include
)

add_library(astratrace_core STATIC
    src/core/image.cpp
)
target_include_directories(astratrace_core PUBLIC
    ${ASTRATRACE_PUBLIC_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/stb
)
target_link_libraries(astratrace_core PUBLIC glm)

add_library(astratrace_scene STATIC
    src/scene/lights/directional_light.cpp
    src/scene/lights/point_light.cpp
    src/scene/lights/spot_light.cpp
    src/scene/geometry/triangle.cpp
    src/scene/geometry/triangle_mesh.cpp
    src/scene/geometry/sphere.cpp
    src/scene/geometry/obj_loader.cpp
    src/scene/world/scene_object.cpp
    src/scene/world/scene.cpp
)
target_include_directories(astratrace_scene PUBLIC
    ${ASTRATRACE_PUBLIC_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinyobjloader
)
target_link_libraries(astratrace_scene PUBLIC
    astratrace_core
    SDL3::SDL3-shared
    glm
)

add_library(astratrace_io_gltf STATIC
    src/io/gltf/gltf_loader.cpp
    src/io/gltf/gltf_parser.cpp
    src/io/gltf/gltf_scene_builder.cpp
    src/io/gltf/gltf_material_converter.cpp
    src/io/gltf/gltf_texture_cache.cpp
)
target_include_directories(astratrace_io_gltf PUBLIC
    ${ASTRATRACE_PUBLIC_INCLUDES}
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/tinygltf
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json/single_include
)
target_link_libraries(astratrace_io_gltf PUBLIC
    astratrace_scene
    astratrace_core
    mikktspace
    glm
)

add_library(astratrace_render_common STATIC
    src/render/common/shadow_utils.cpp
)
target_include_directories(astratrace_render_common PUBLIC ${ASTRATRACE_PUBLIC_INCLUDES})
target_link_libraries(astratrace_render_common PUBLIC
    astratrace_scene
    astratrace_core
    glm
)

add_library(astratrace_render_cpu STATIC
    src/render/cpu/whitted_integrator.cpp
    src/render/cpu/path_integrator.cpp
    src/render/cpu/cpu_whitted_renderer.cpp
    src/render/cpu/cpu_path_renderer.cpp
)
target_include_directories(astratrace_render_cpu PUBLIC ${ASTRATRACE_PUBLIC_INCLUDES})
target_link_libraries(astratrace_render_cpu PUBLIC
    astratrace_render_common
    astratrace_scene
    astratrace_core
    glm
)
if(OpenMP_CXX_FOUND)
    target_link_libraries(astratrace_render_cpu PUBLIC OpenMP::OpenMP_CXX)
endif()

add_library(astratrace_render_gpu STATIC
    src/render/gpu/gpu_path_renderer.cpp
)
target_include_directories(astratrace_render_gpu PUBLIC ${ASTRATRACE_PUBLIC_INCLUDES})
target_link_libraries(astratrace_render_gpu PUBLIC
    astratrace_render_cpu
    astratrace_render_common
    astratrace_scene
    astratrace_core
    SDL3::SDL3-shared
    glm
)

add_executable(astratrace_app
    src/app/main.cpp
    src/app/application.cpp
    src/app/frame_loop.cpp
    src/app/input/camera_controller.cpp
    src/render/renderer.cpp
)
target_include_directories(astratrace_app PRIVATE
    ${ASTRATRACE_PUBLIC_INCLUDES}
    ${ASTRATRACE_VENDOR_INCLUDES}
)
target_link_libraries(astratrace_app PRIVATE
    astratrace_io_gltf
    astratrace_render_common
    astratrace_render_cpu
    astratrace_render_gpu
    astratrace_scene
    astratrace_core
    imgui
    SDL3::SDL3-shared
    glm
)

if(Python3_Interpreter_FOUND)
    add_custom_target(check_include_boundaries
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_include_boundaries.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking include boundary rules"
    )
endif()
