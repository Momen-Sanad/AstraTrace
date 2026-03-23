#pragma once

#include <glm/glm.hpp>
#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/materials/material_base.hpp"

class SmoothMirrorMaterial : public Material {
public:
    std::shared_ptr<Image<Color>> base_color = nullptr;
    std::shared_ptr<Image<Color>> normal = nullptr;
    Color tint = Color(1.0f);

    inline Color sampleBaseColor(glm::vec2 uv) const {
        if(base_color) return tint * sampleImage(base_color, uv);
        return tint;
    }

    inline Color sampleNormal(glm::vec2 uv) const {
        if(normal) return sampleImage(normal, uv);
        return Color(0.5f, 0.5f, 1.0f);
    }
};
