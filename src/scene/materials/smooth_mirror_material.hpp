#pragma once

#include <glm/glm.hpp>
#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/materials/material_base.hpp"

class SmoothMirrorBSDF final : public BSDF {
public:
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    Color color = Color(1.0f);

    BSDFSample sample(const glm::vec3& u, const glm::vec3& view) const override {
        (void)u;
        BSDFSample result;
        result.direction = glm::reflect(-view, normal);
        result.throughput.specular = color;
        result.pdf = 0.0f;
        result.lobe = LobeType::Specular;
        result.is_delta = true;
        return result;
    }

    DiffuseSpecular evaluate(const glm::vec3& light, const glm::vec3& view) const override {
        (void)light;
        (void)view;
        return {};
    }

    float pdf(const glm::vec3& light, const glm::vec3& view) const override {
        (void)light;
        (void)view;
        return 0.0f;
    }

    Color getSpecularColor() const override { return color; }
    glm::vec3 getNormal() const override { return normal; }
    bool isDelta() const override { return true; }
};

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

    std::unique_ptr<BSDF> sampleBSDF(const SurfaceData& surface) const override {
        auto bsdf = std::make_unique<SmoothMirrorBSDF>();
        Color local = 2.0f * sampleNormal(surface.uv) - 1.0f;
        bsdf->normal = glm::normalize(glm::vec3(
            local.x * surface.tangent +
            local.y * surface.bitangent +
            local.z * surface.normal
        ));
        bsdf->color = sampleBaseColor(surface.uv);
        return bsdf;
    }
};
