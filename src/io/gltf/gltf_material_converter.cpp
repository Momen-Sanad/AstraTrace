#include "io/gltf/gltf_material_converter.hpp"

namespace io::gltf {

std::shared_ptr<Material> convertMaterialFromGltf(
    const tinygltf::Model& model,
    int material_index
) {
    (void)model;
    (void)material_index;
    return std::make_shared<PBRMaterial>();
}

} // namespace io::gltf
