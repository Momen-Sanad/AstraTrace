#pragma once

#include <memory>
#include <unordered_map>
#include "core/color.hpp"
#include "core/image.hpp"
#include <tiny_gltf.h>

namespace io::gltf {

class GltfTextureCache {
public:
    std::shared_ptr<Image<Color>> getColorTexture(const tinygltf::Model& model, int texture_index, bool linearize);
    std::shared_ptr<Image<ColorA>> getColorATexture(const tinygltf::Model& model, int texture_index, bool linearize);
    std::shared_ptr<Image<float>> getScalarTexture(const tinygltf::Model& model, int texture_index, bool linearize);

private:
    std::unordered_map<int, std::shared_ptr<Image<Color>>> color_cache;
    std::unordered_map<int, std::shared_ptr<Image<ColorA>>> color_a_cache;
    std::unordered_map<int, std::shared_ptr<Image<float>>> scalar_cache;
};

} // namespace io::gltf
