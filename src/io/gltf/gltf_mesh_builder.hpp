#pragma once

#include <memory>
#include <string>

#include <glm/glm.hpp>
#include <tiny_gltf.h>

#include "scene/geometry/triangle_mesh.hpp"

namespace io::gltf {

std::shared_ptr<TriangleMesh> buildMeshFromPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const glm::mat4& world_transform,
    std::string& warnings
);

} // namespace io::gltf
