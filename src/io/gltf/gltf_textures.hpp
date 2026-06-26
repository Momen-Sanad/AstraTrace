#pragma once

#include <memory>
#include <string>

#include <tiny_gltf.h>

#include "core/color.hpp"
#include "core/image.hpp"
#include "io/gltf/gltf_utils.hpp"
#include "scene/materials/material_base.hpp"

namespace io::gltf {

const tinygltf::Image* getTextureImage(const tinygltf::Model& model, int texture_index);
float readImageChannel(const tinygltf::Image& image, int x, int y, int channel);

std::shared_ptr<Image<ColorA>> buildBaseColorImage(
    const tinygltf::Image& image,
    bool linearize,
    const std::string& alpha_mode,
    float alpha_cutoff
);
std::shared_ptr<Image<ColorA>> buildReadableGuideImage(
    const tinygltf::Image& image,
    const std::string& alpha_mode,
    float alpha_cutoff
);
std::shared_ptr<Image<Color>> buildGuideEmissionImage(const tinygltf::Image& image);
std::shared_ptr<Image<Color>> buildColorImage(const tinygltf::Image& image, bool linearize);
std::shared_ptr<Image<Color>> buildNormalImage(const tinygltf::Image& image, float scale);
std::shared_ptr<Image<float>> buildOcclusionImage(const tinygltf::Image& image, float strength);
std::shared_ptr<Image<Color>> buildMetalRoughnessImage(
    const tinygltf::Image& image,
    float metallic_factor,
    float roughness_factor
);
std::shared_ptr<Image<Color>> buildTransmissionDetailImage(const tinygltf::Image& image);
std::shared_ptr<Image<Color>> makeSolidColorImage(const Color& color);

bool materialNameLooksLikeGuide(const std::string& name);
bool looksLikeDarkTransparentAnnotationTexture(const tinygltf::Image& image);

template<typename TextureInfoT>
TextureMapping getTextureMapping(const TextureInfoT& info, std::string& warnings) {
    TextureMapping mapping;
    mapping.texcoord = info.texCoord == 1 ? 1 : 0;
    if(info.texCoord > 1) {
        appendUniqueLine(
            warnings,
            "glTF texture requested TEXCOORD_n where n > 1. Falling back to TEXCOORD_0."
        );
    }

    auto ext_it = info.extensions.find("KHR_texture_transform");
    if(ext_it == info.extensions.end() || !ext_it->second.IsObject()) return mapping;

    const tinygltf::Value& ext = ext_it->second;
    if(ext.Has("texCoord") && ext.Get("texCoord").IsInt()) {
        int texcoord = ext.Get("texCoord").Get<int>();
        mapping.texcoord = texcoord == 1 ? 1 : 0;
        if(texcoord > 1) {
            appendUniqueLine(
                warnings,
                "KHR_texture_transform requested TEXCOORD_n where n > 1. Falling back to TEXCOORD_0."
            );
        }
    }
    if(ext.Has("offset") && ext.Get("offset").IsArray() && ext.Get("offset").ArrayLen() >= 2) {
        const tinygltf::Value& offset = ext.Get("offset");
        if(offset.Get(0).IsNumber() && offset.Get(1).IsNumber()) {
            mapping.offset = glm::vec2(
                static_cast<float>(offset.Get(0).GetNumberAsDouble()),
                static_cast<float>(offset.Get(1).GetNumberAsDouble())
            );
        }
    }
    if(ext.Has("scale") && ext.Get("scale").IsArray() && ext.Get("scale").ArrayLen() >= 2) {
        const tinygltf::Value& scale = ext.Get("scale");
        if(scale.Get(0).IsNumber() && scale.Get(1).IsNumber()) {
            mapping.scale = glm::vec2(
                static_cast<float>(scale.Get(0).GetNumberAsDouble()),
                static_cast<float>(scale.Get(1).GetNumberAsDouble())
            );
        }
    }
    if(ext.Has("rotation") && ext.Get("rotation").IsNumber()) {
        mapping.rotation = static_cast<float>(ext.Get("rotation").GetNumberAsDouble());
    }
    return mapping;
}

} // namespace io::gltf
