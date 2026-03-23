#pragma once

#include <cstddef>
#include <string>
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"

struct GltfSceneLoadResult {
    bool success = false;
    bool camera_loaded = false;
    std::string warning;
    std::string error;
    std::size_t object_count = 0;
    std::size_t light_count = 0;
};

namespace io::gltf {

// Load a glTF scene (.gltf/.glb) into the given Scene and Camera.
// The existing Scene is expected to be empty before calling this function.
GltfSceneLoadResult loadSceneFromGLTF(
    const std::string& file_path,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
);

} // namespace io::gltf

// Backward compatibility shim for older call sites.
inline GltfSceneLoadResult loadSceneFromGLTF(
    const std::string& file_path,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
) {
    return io::gltf::loadSceneFromGLTF(file_path, scene, camera, fallback_aspect_ratio);
}
