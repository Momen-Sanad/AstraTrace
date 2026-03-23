#pragma once

#include <glm/glm.hpp>
#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/materials/material_base.hpp"

class PBRMaterial : public Material {
public:
    std::shared_ptr<Image<ColorA>> base_color = nullptr;
    std::shared_ptr<Image<Color>> emissive = nullptr;
    std::shared_ptr<Image<Color>> normal = nullptr;
    std::shared_ptr<Image<float>> occlusion = nullptr;
    std::shared_ptr<Image<Color>> metal_roughness = nullptr;
    ColorA tint = ColorA(1.0f);
    Color emissive_power = Color(0.0f);

    inline ColorA sampleBaseColor(glm::vec2 uv) const {
        if(base_color) return tint * sampleImage(base_color, uv);
        return tint;
    }

    inline Color sampleEmissive(glm::vec2 uv) const {
        if(emissive) return emissive_power * sampleImage(emissive, uv);
        return emissive_power;
    }

    inline Color sampleNormal(glm::vec2 uv) const {
        if(normal) return sampleImage(normal, uv);
        return Color(0.5f, 0.5f, 1.0f);
    }

    inline float sampleOcclusion(glm::vec2 uv) const {
        if(occlusion) return sampleImage(occlusion, uv);
        return 1.0f;
    }

    inline Color sampleMetalRoughness(glm::vec2 uv) const {
        if(metal_roughness) return sampleImage(metal_roughness, uv);
        return Color(0.0f, 1.0f, 0.0f);
    }
};
