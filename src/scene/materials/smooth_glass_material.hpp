#pragma once

#include <glm/glm.hpp>
#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/materials/material_base.hpp"

class SmoothGlassBSDF final : public BSDF {
public:
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    Color color = Color(1.0f);
    float refractive_index = 1.5f;
    bool entering = true;

    BSDFSample sample(const glm::vec3& u, const glm::vec3& view) const override {
        BSDFSample result;
        float eta = entering ? 1.0f / refractive_index : refractive_index;
        float f0 = (refractive_index - 1.0f) / (refractive_index + 1.0f);
        f0 *= f0;
        float cos_theta = glm::clamp(glm::dot(view, normal), 0.0f, 1.0f);
        float fresnel = f0 + (1.0f - f0) * glm::pow(1.0f - cos_theta, 5.0f);
        glm::vec3 reflected = glm::reflect(-view, normal);
        glm::vec3 refracted = glm::refract(-view, normal, eta);

        if(refracted == glm::vec3(0.0f) || u.x < fresnel) {
            result.direction = reflected;
            result.throughput.specular = Color(1.0f);
            result.lobe = LobeType::Specular;
        } else {
            result.direction = refracted;
            result.throughput.specular = color;
            result.lobe = LobeType::Transmission;
        }
        result.pdf = 0.0f;
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

    Color getSpecularColor() const override { return Color(1.0f); }
    glm::vec3 getNormal() const override { return normal; }
    bool isDelta() const override { return true; }
};

class SmoothGlassMaterial : public Material {
public:
    std::shared_ptr<Image<Color>> base_color = nullptr;
    std::shared_ptr<Image<Color>> normal = nullptr;
    Color tint = Color(1.0f);
    float refractive_index = 1.5f;

    inline Color sampleBaseColor(glm::vec2 uv) const {
        if(base_color) return tint * sampleImage(base_color, uv);
        return tint;
    }

    inline Color sampleNormal(glm::vec2 uv) const {
        if(normal) return sampleImage(normal, uv);
        return Color(0.5f, 0.5f, 1.0f);
    }

    std::unique_ptr<BSDF> sampleBSDF(const SurfaceData& surface) const override {
        auto bsdf = std::make_unique<SmoothGlassBSDF>();
        Color local = 2.0f * sampleNormal(surface.uv) - 1.0f;
        bsdf->normal = glm::normalize(glm::vec3(
            local.x * surface.tangent +
            local.y * surface.bitangent +
            local.z * surface.normal
        ));
        bsdf->color = sampleBaseColor(surface.uv);
        bsdf->refractive_index = refractive_index;
        bsdf->entering = surface.hit_direction == HitDirection::ENTERING;
        return bsdf;
    }
};
