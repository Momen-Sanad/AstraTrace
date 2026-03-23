#pragma once

#include <string>
#include <tiny_gltf.h>

namespace io::gltf {

bool parseModelFromFile(
    const std::string& file_path,
    tinygltf::Model& model,
    std::string& warning,
    std::string& error
);

} // namespace io::gltf
