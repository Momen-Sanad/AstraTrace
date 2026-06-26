#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>
#include <tiny_gltf.h>

namespace io::gltf {

bool readIndexAccessor(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<uint32_t>& out_indices
);

bool readAttributeVec3(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t expected_count,
    std::vector<glm::vec3>& out
);

bool readAttributeVec2(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t expected_count,
    std::vector<glm::vec2>& out
);

bool readAttributeVec4(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t expected_count,
    std::vector<glm::vec4>& out
);

} // namespace io::gltf
