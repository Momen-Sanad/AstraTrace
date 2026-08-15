#pragma once

#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/geometry/shape_base.hpp"

enum struct LobeType {
    None,
    Diffuse,
    Specular,
    Transmission,
};

struct DiffuseSpecular {
    Color diffuse = Color(0.0f);
    Color specular = Color(0.0f);

    Color sum() const { return diffuse + specular; }
};

struct BSDFSample {
    glm::vec3 direction = glm::vec3(0.0f);
    DiffuseSpecular throughput;
    float pdf = 0.0f;
    LobeType lobe = LobeType::None;
    bool is_delta = false;
};

struct TextureMapping {
    int texcoord = 0;
    glm::vec2 offset = glm::vec2(0.0f);
    glm::vec2 scale = glm::vec2(1.0f);
    float rotation = 0.0f;

    glm::vec2 apply(glm::vec2 uv) const {
        uv *= scale;
        if(rotation != 0.0f) {
            float c = glm::cos(rotation);
            float s = glm::sin(rotation);
            uv = glm::vec2(c * uv.x - s * uv.y, s * uv.x + c * uv.y);
        }
        return uv + offset;
    }

    glm::vec2 select(const SurfaceData& surface) const {
        return texcoord == 1 ? surface.uv1 : surface.uv;
    }

    glm::vec2 select(glm::vec2 uv0, glm::vec2 uv1) const {
        return texcoord == 1 ? uv1 : uv0;
    }
};

template<typename T>
T sampleMappedImage(const std::shared_ptr<Image<T>>& image, const TextureMapping& mapping, const SurfaceData& surface) {
    return sampleImage(image, mapping.apply(mapping.select(surface)));
}

template<typename T>
T sampleMappedImage(
    const std::shared_ptr<Image<T>>& image,
    const TextureMapping& mapping,
    glm::vec2 uv0,
    glm::vec2 uv1
) {
    return sampleImage(image, mapping.apply(mapping.select(uv0, uv1)));
}

class BSDF {
public:
    virtual ~BSDF() = default;

    virtual BSDFSample sample(const glm::vec3& u, const glm::vec3& view) const = 0;
    virtual DiffuseSpecular evaluate(const glm::vec3& light, const glm::vec3& view) const = 0;
    virtual float pdf(const glm::vec3& light, const glm::vec3& view) const = 0;

    virtual Color getSubsurfaceAlbedo() const { return Color(0.0f); }
    virtual Color getSpecularColor() const { return Color(0.0f); }
    virtual Color getEmission(const glm::vec3& view) const { (void)view; return Color(0.0f); }
    virtual float getCoverage(const glm::vec3& view) const { (void)view; return 1.0f; }
    virtual glm::vec3 getNormal() const { return glm::vec3(0.0f); }
    virtual bool isDelta() const { return false; }
    virtual void regularize() {}
};

class Material {
public:
    virtual ~Material() = default;

    virtual std::unique_ptr<BSDF> sampleBSDF(const SurfaceData& surface) const {
        (void)surface;
        return nullptr;
    }

    virtual Color sampleEmissive(glm::vec2 uv) const {
        (void)uv;
        return Color(0.0f);
    }

    virtual Color sampleEmissive(glm::vec2 uv0, glm::vec2 uv1) const {
        (void)uv1;
        return sampleEmissive(uv0);
    }

    virtual float sampleCoverage(glm::vec2 uv) const {
        (void)uv;
        return 1.0f;
    }

    virtual Color getAverageEmissivePower() const {
        return Color(0.0f);
    }

    virtual bool castsShadows() const {
        return true;
    }

    virtual bool isDoubleSided() const {
        return true;
    }

    virtual bool emitsDoubleSided() const {
        return false;
    }
};
