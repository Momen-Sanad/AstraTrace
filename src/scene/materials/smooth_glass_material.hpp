#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/materials/material_base.hpp"

namespace smooth_glass_detail {

inline glm::vec3 makeTangent(const glm::vec3& normal) {
    glm::vec3 up = glm::abs(normal.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(glm::cross(up, normal));
}

inline glm::vec3 cosineHemisphere(const glm::vec2& u, const glm::vec3& normal) {
    float r = glm::sqrt(glm::clamp(u.x, 0.0f, 1.0f));
    float phi = glm::two_pi<float>() * glm::clamp(u.y, 0.0f, 1.0f);
    float x = r * glm::cos(phi);
    float y = r * glm::sin(phi);
    float z = glm::sqrt(glm::max(0.0f, 1.0f - x * x - y * y));
    glm::vec3 tangent = makeTangent(normal);
    glm::vec3 bitangent = glm::cross(normal, tangent);
    return glm::normalize(x * tangent + y * bitangent + z * normal);
}

inline float detailProbability(float strength) {
    if(strength <= 0.0f) return 0.0f;
    return glm::clamp(0.25f + 0.55f * strength, 0.25f, 0.70f);
}

}

class SmoothGlassBSDF final : public BSDF {
public:
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 base_normal = glm::vec3(0.0f, 1.0f, 0.0f);
    Color color = Color(1.0f);
    float surface_detail_strength = 0.0f;
    float refractive_index = 1.5f;
    bool entering = true;

    BSDFSample sample(const glm::vec3& u, const glm::vec3& view) const override {
        BSDFSample result;
        float detail_probability = smooth_glass_detail::detailProbability(surface_detail_strength);
        float clear_weight = surface_detail_strength > 0.0f
            ? glm::clamp(1.0f - 0.70f * surface_detail_strength, 0.25f, 1.0f)
            : 1.0f;
        float clear_probability = glm::max(1e-3f, 1.0f - detail_probability);
        if(detail_probability > 0.0f && u.z < detail_probability) {
            result.direction = smooth_glass_detail::cosineHemisphere(glm::vec2(u), normal);
            result.lobe = LobeType::Diffuse;
            result.pdf = pdf(result.direction, view);
            if(result.pdf > 0.0f) {
                DiffuseSpecular value = evaluate(result.direction, view);
                result.throughput.diffuse = value.diffuse / result.pdf;
            }
            return result;
        }

        float eta = entering ? 1.0f / refractive_index : refractive_index;
        float f0 = (refractive_index - 1.0f) / (refractive_index + 1.0f);
        f0 *= f0;
        float cos_theta = glm::clamp(glm::dot(view, normal), 0.0f, 1.0f);
        float fresnel = f0 + (1.0f - f0) * glm::pow(1.0f - cos_theta, 5.0f);
        glm::vec3 reflected = glm::reflect(-view, normal);
        if(glm::dot(reflected, base_normal) < 0.0f) reflected = glm::reflect(-view, base_normal);
        glm::vec3 refracted = glm::refract(-view, normal, eta);

        if(refracted == glm::vec3(0.0f) || u.x < fresnel) {
            result.direction = reflected;
            result.throughput.specular = Color(clear_weight / clear_probability);
            result.lobe = LobeType::Specular;
        } else {
            result.direction = refracted;
            result.throughput.specular = (clear_weight / clear_probability) * color;
            result.lobe = LobeType::Transmission;
        }
        result.pdf = 0.0f;
        result.is_delta = true;
        return result;
    }

    DiffuseSpecular evaluate(const glm::vec3& light, const glm::vec3& view) const override {
        DiffuseSpecular value;
        if(surface_detail_strength <= 0.0f) return value;

        // Preview layer for rough/textured transmission assets that this CPU renderer cannot fully scatter.
        float n_dot_l = glm::dot(normal, light);
        float n_dot_v = glm::dot(normal, view);
        if(n_dot_l <= 0.0f || n_dot_v <= 0.0f) return value;

        value.diffuse = surface_detail_strength * color * (n_dot_l / glm::pi<float>());
        return value;
    }

    float pdf(const glm::vec3& light, const glm::vec3& view) const override {
        float detail_probability = smooth_glass_detail::detailProbability(surface_detail_strength);
        if(detail_probability <= 0.0f) return 0.0f;
        float n_dot_l = glm::dot(normal, light);
        float n_dot_v = glm::dot(normal, view);
        if(n_dot_l <= 0.0f || n_dot_v <= 0.0f) return 0.0f;
        return detail_probability * n_dot_l / glm::pi<float>();
    }

    Color getSubsurfaceAlbedo() const override {
        return surface_detail_strength * color;
    }

    Color getSpecularColor() const override {
        if(surface_detail_strength > 0.0f) return Color(0.0f);
        return Color(1.0f);
    }

    bool isDelta() const override {
        return surface_detail_strength <= 0.0f;
    }

    void regularize() override {
        surface_detail_strength = glm::max(surface_detail_strength, 0.15f);
    }

    glm::vec3 getNormal() const override { return normal; }
};

class SmoothGlassMaterial : public Material {
public:
    std::shared_ptr<Image<Color>> base_color = nullptr;
    std::shared_ptr<Image<Color>> normal = nullptr;
    TextureMapping base_color_mapping;
    TextureMapping normal_mapping;
    Color tint = Color(1.0f);
    float surface_detail_strength = 0.0f;
    float refractive_index = 1.5f;

    inline Color sampleBaseColor(glm::vec2 uv) const {
        if(base_color) return tint * sampleImage(base_color, uv);
        return tint;
    }

    inline Color sampleBaseColor(const SurfaceData& surface) const {
        if(base_color) return tint * sampleMappedImage(base_color, base_color_mapping, surface);
        return tint;
    }

    inline Color sampleNormal(glm::vec2 uv) const {
        if(normal) return sampleImage(normal, uv);
        return Color(0.5f, 0.5f, 1.0f);
    }

    inline Color sampleNormal(const SurfaceData& surface) const {
        if(normal) return sampleMappedImage(normal, normal_mapping, surface);
        return Color(0.5f, 0.5f, 1.0f);
    }

    std::unique_ptr<BSDF> sampleBSDF(const SurfaceData& surface) const override {
        auto bsdf = std::make_unique<SmoothGlassBSDF>();
        Color local = 2.0f * sampleNormal(surface) - 1.0f;
        bsdf->normal = glm::normalize(glm::vec3(
            local.x * surface.tangent +
            local.y * surface.bitangent +
            local.z * surface.normal
        ));
        bsdf->base_normal = surface.normal;
        bsdf->color = sampleBaseColor(surface);
        bsdf->surface_detail_strength = surface_detail_strength;
        bsdf->refractive_index = refractive_index;
        bsdf->entering = surface.hit_direction == HitDirection::ENTERING;
        return bsdf;
    }
};
