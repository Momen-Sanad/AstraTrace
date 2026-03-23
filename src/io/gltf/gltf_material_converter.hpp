#pragma once

#include <memory>
#include "scene/materials/materials.hpp"
#include <tiny_gltf.h>

namespace io::gltf {

std::shared_ptr<Material> convertMaterialFromGltf(
    const tinygltf::Model& model,
    int material_index
);

} // namespace io::gltf
