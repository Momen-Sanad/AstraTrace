cmake_minimum_required(VERSION 4.0.0)
project(Lab4 VERSION 0.1.0 LANGUAGES C CXX)

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

add_executable(${PROJECT_NAME} 
    src/main.cpp
    src/image.cpp
    src/camera_controller.cpp
    src/shape.cpp
    src/light.cpp
    src/scene.cpp
    src/gltf_loader.cpp
)
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-shared glm)
if(OpenMP_CXX_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE OpenMP::OpenMP_CXX)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE
    vendor/stb
    vendor/tinyobjloader
    vendor/tinygltf
    vendor/json/single_include
)
