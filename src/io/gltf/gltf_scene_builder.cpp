#include "io/gltf/gltf_scene_builder.hpp"

namespace io::gltf {

GltfSceneLoadResult buildSceneFromGltfModel(
    const tinygltf::Model& model,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
) {
    (void)model;
    (void)scene;
    (void)camera;
    (void)fallback_aspect_ratio;
    GltfSceneLoadResult result;
    result.success = false;
    result.error = "Scene builder placeholder is not wired to the loader pipeline yet.";
    return result;
}

} // namespace io::gltf
