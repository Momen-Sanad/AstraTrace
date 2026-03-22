#pragma once

#include <cstddef>
#include <string>
#include "camera.hpp"
#include "scene.hpp"

struct GltfSceneLoadResult {
    bool success = false;
    bool camera_loaded = false;
    std::string warning;
    std::string error;
    std::size_t object_count = 0;
    std::size_t light_count = 0;
};

// Load a glTF scene (.gltf/.glb) into the given Scene and Camera.
// The existing Scene is expected to be empty before calling this function.
GltfSceneLoadResult loadSceneFromGLTF(
    const std::string& file_path,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
);
