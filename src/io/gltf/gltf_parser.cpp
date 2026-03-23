#include "io/gltf/gltf_parser.hpp"

#include <algorithm>
#include <cctype>

namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string getExtension(const std::string& path) {
    std::size_t dot = path.find_last_of('.');
    if(dot == std::string::npos) return "";
    return toLower(path.substr(dot));
}

} // namespace

namespace io::gltf {

bool parseModelFromFile(
    const std::string& file_path,
    tinygltf::Model& model,
    std::string& warning,
    std::string& error
) {
    tinygltf::TinyGLTF loader;
    std::string extension = getExtension(file_path);
    if(extension == ".glb") {
        return loader.LoadBinaryFromFile(&model, &error, &warning, file_path);
    }
    return loader.LoadASCIIFromFile(&model, &error, &warning, file_path);
}

} // namespace io::gltf
