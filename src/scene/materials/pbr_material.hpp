#pragma once

#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/materials/material_base.hpp"

namespace scene_material_detail {

inline float luminanceValue(const Color& color) {
    return glm::max(color.r, glm::max(color.g, color.b));
}

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

inline Color fresnelSchlick(float cos_theta, const Color& f0) {
    return glm::mix(f0, Color(1.0f), glm::pow(glm::max(0.0f, 1.0f - glm::clamp(cos_theta, 0.0f, 1.0f)), 5.0f));
}

inline float ggxD(float n_dot_h, float alpha) {
    if(n_dot_h <= 0.0f) return 0.0f;
    float a2 = alpha * alpha;
    float d = n_dot_h * n_dot_h * (a2 - 1.0f) + 1.0f;
    return a2 / glm::max(1e-7f, glm::pi<float>() * d * d);
}

inline float smithG1(float n_dot_v, float alpha) {
    n_dot_v = glm::max(n_dot_v, 0.0f);
    float a = alpha;
    float k = (a * a) * 0.5f;
    return n_dot_v / glm::max(1e-6f, n_dot_v * (1.0f - k) + k);
}

inline float smithG2(float n_dot_l, float n_dot_v, float alpha) {
    return smithG1(n_dot_l, alpha) * smithG1(n_dot_v, alpha);
}

inline glm::vec3 sampleGGX(const glm::vec2& u, const glm::vec3& normal, float alpha) {
    float a2 = alpha * alpha;
    float phi = glm::two_pi<float>() * glm::clamp(u.x, 0.0f, 1.0f);
    float cos_theta = glm::sqrt((1.0f - u.y) / glm::max(1e-6f, 1.0f + (a2 - 1.0f) * u.y));
    float sin_theta = glm::sqrt(glm::max(0.0f, 1.0f - cos_theta * cos_theta));
    glm::vec3 tangent = makeTangent(normal);
    glm::vec3 bitangent = glm::cross(normal, tangent);
    return glm::normalize(
        sin_theta * glm::cos(phi) * tangent +
        sin_theta * glm::sin(phi) * bitangent +
        cos_theta * normal
    );
}

}

class PBRBRDF final : public BSDF {
public:
    glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
    Color subsurface_color = Color(0.0f);
    Color specular_color = Color(0.04f);
    Color emission = Color(0.0f);
    float alpha = 1.0f;
    float coverage = 1.0f;

    BSDFSample sample(const glm::vec3& u, const glm::vec3& view) const override {
        using namespace scene_material_detail;
        BSDFSample result;

        float n_dot_v = glm::dot(normal, view);
        if(n_dot_v <= 0.0f) return result;

        float diffuse_value = luminanceValue(subsurface_color);
        float specular_value = luminanceValue(fresnelSchlick(n_dot_v, specular_color));
        float specular_probability = specular_value / glm::max(1e-4f, diffuse_value + specular_value);
        specular_probability = glm::clamp(specular_probability, 0.05f, 0.95f);

        if(u.z < specular_probability) {
            result.lobe = LobeType::Specular;
            if(alpha < 1e-3f) {
                result.direction = glm::reflect(-view, normal);
                result.throughput.specular = fresnelSchlick(glm::dot(result.direction, normal), specular_color) / specular_probability;
                result.pdf = 0.0f;
                result.is_delta = true;
                return result;
            }
            glm::vec3 half_vector = sampleGGX(glm::vec2(u), normal, glm::max(alpha, 1e-3f));
            result.direction = glm::reflect(-view, half_vector);
        } else {
            result.lobe = LobeType::Diffuse;
            result.direction = cosineHemisphere(glm::vec2(u), normal);
        }

        result.pdf = pdf(result.direction, view);
        if(result.pdf > 0.0f) {
            DiffuseSpecular value = evaluate(result.direction, view);
            result.throughput.diffuse = value.diffuse / result.pdf;
            result.throughput.specular = value.specular / result.pdf;
        }
        return result;
    }

    DiffuseSpecular evaluate(const glm::vec3& light, const glm::vec3& view) const override {
        using namespace scene_material_detail;
        DiffuseSpecular value;
        float n_dot_l = glm::dot(normal, light);
        float n_dot_v = glm::dot(normal, view);
        if(n_dot_l <= 0.0f || n_dot_v <= 0.0f) return value;

        glm::vec3 half_vector = glm::normalize(light + view);
        float n_dot_h = glm::max(0.0f, glm::dot(normal, half_vector));
        float h_dot_v = glm::max(0.0f, glm::dot(half_vector, view));
        Color f = fresnelSchlick(h_dot_v, specular_color);
        float d = ggxD(n_dot_h, glm::max(alpha, 1e-3f));
        float g = smithG2(n_dot_l, n_dot_v, glm::max(alpha, 1e-3f));

        value.diffuse = subsurface_color * (Color(1.0f) - f) * (n_dot_l / glm::pi<float>());
        value.specular = f * (d * g / glm::max(1e-5f, 4.0f * n_dot_v));
        return value;
    }

    float pdf(const glm::vec3& light, const glm::vec3& view) const override {
        using namespace scene_material_detail;
        float n_dot_l = glm::dot(normal, light);
        float n_dot_v = glm::dot(normal, view);
        if(n_dot_l <= 0.0f || n_dot_v <= 0.0f) return 0.0f;

        float diffuse_value = luminanceValue(subsurface_color);
        float specular_value = luminanceValue(fresnelSchlick(n_dot_v, specular_color));
        float specular_probability = specular_value / glm::max(1e-4f, diffuse_value + specular_value);
        specular_probability = glm::clamp(specular_probability, 0.05f, 0.95f);

        glm::vec3 half_vector = glm::normalize(light + view);
        float n_dot_h = glm::max(0.0f, glm::dot(normal, half_vector));
        float h_dot_v = glm::max(1e-5f, glm::dot(half_vector, view));
        float specular_pdf = ggxD(n_dot_h, glm::max(alpha, 1e-3f)) * n_dot_h / (4.0f * h_dot_v);
        float diffuse_pdf = n_dot_l / glm::pi<float>();
        return specular_probability * specular_pdf + (1.0f - specular_probability) * diffuse_pdf;
    }

    Color getSubsurfaceAlbedo() const override { return subsurface_color; }
    Color getSpecularColor() const override { return specular_color; }
    Color getEmission(const glm::vec3& view) const override { (void)view; return emission; }
    float getCoverage(const glm::vec3& view) const override { (void)view; return coverage; }
    glm::vec3 getNormal() const override { return normal; }
    void regularize() override { alpha = glm::max(alpha, 0.12f); }
};

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

    std::unique_ptr<BSDF> sampleBSDF(const SurfaceData& surface) const override {
        auto brdf = std::make_unique<PBRBRDF>();
        Color local = 2.0f * sampleNormal(surface.uv) - 1.0f;
        brdf->normal = glm::normalize(glm::vec3(
            local.x * surface.tangent +
            local.y * surface.bitangent +
            local.z * surface.normal
        ));

        ColorA base = sampleBaseColor(surface.uv);
        Color mr = sampleMetalRoughness(surface.uv);
        float metalness = glm::clamp(mr.r, 0.0f, 1.0f);
        float roughness = glm::clamp(mr.g, 0.0f, 1.0f);
        Color base_rgb(base);
        brdf->subsurface_color = base_rgb * (1.0f - metalness);
        brdf->specular_color = glm::mix(Color(0.04f), base_rgb, metalness);
        brdf->alpha = glm::max(roughness * roughness, 1e-4f);
        brdf->coverage = glm::clamp(base.a, 0.0f, 1.0f);
        brdf->emission = sampleEmissive(surface.uv);
        return brdf;
    }

    float sampleCoverage(glm::vec2 uv) const override {
        return glm::clamp(sampleBaseColor(uv).a, 0.0f, 1.0f);
    }

    Color getAverageEmissivePower() const override {
        if(!emissive) return emissive_power;
        const Color* pixels = emissive->getPixels();
        Color average(0.0f);
        int count = emissive->getWidth() * emissive->getHeight();
        for(int i = 0; i < count; ++i) average += pixels[i];
        if(count > 0) average /= static_cast<float>(count);
        return emissive_power * average;
    }
};
