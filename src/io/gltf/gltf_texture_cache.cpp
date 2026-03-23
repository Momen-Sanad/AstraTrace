#include "io/gltf/gltf_texture_cache.hpp"

namespace io::gltf {

std::shared_ptr<Image<Color>> GltfTextureCache::getColorTexture(const tinygltf::Model& model, int texture_index, bool linearize) {
    (void)model;
    (void)linearize;
    if(texture_index < 0) return nullptr;
    auto it = color_cache.find(texture_index);
    if(it != color_cache.end()) return it->second;
    color_cache[texture_index] = nullptr;
    return nullptr;
}

std::shared_ptr<Image<ColorA>> GltfTextureCache::getColorATexture(const tinygltf::Model& model, int texture_index, bool linearize) {
    (void)model;
    (void)linearize;
    if(texture_index < 0) return nullptr;
    auto it = color_a_cache.find(texture_index);
    if(it != color_a_cache.end()) return it->second;
    color_a_cache[texture_index] = nullptr;
    return nullptr;
}

std::shared_ptr<Image<float>> GltfTextureCache::getScalarTexture(const tinygltf::Model& model, int texture_index, bool linearize) {
    (void)model;
    (void)linearize;
    if(texture_index < 0) return nullptr;
    auto it = scalar_cache.find(texture_index);
    if(it != scalar_cache.end()) return it->second;
    scalar_cache[texture_index] = nullptr;
    return nullptr;
}

} // namespace io::gltf
