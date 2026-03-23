#pragma once

#include "io/gltf/gltf_loader.hpp"
#include <tiny_gltf.h>

namespace io::gltf {

GltfSceneLoadResult buildSceneFromGltfModel(
    const tinygltf::Model& model,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
);

} // namespace io::gltf
