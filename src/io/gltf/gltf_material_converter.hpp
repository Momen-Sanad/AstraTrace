#pragma once

#include <memory>
#include <string>
#include "scene/materials/materials.hpp"
#include <tiny_gltf.h>

namespace io::gltf {

std::shared_ptr<Material> convertMaterialFromGltf(
    const tinygltf::Model& model,
    int material_index
);

std::shared_ptr<Material> createDefaultMaterial();
std::shared_ptr<Material> createMaterialFromGLTF(
    const tinygltf::Model& model,
    const tinygltf::Material& material,
    std::string& warnings
);

float getMaterialExtensionNumber(
    const tinygltf::Material& material,
    const char* extension_name,
    const char* property_name,
    float fallback
);

bool hasAuthoredEmission(const tinygltf::Material& material);

} // namespace io::gltf
