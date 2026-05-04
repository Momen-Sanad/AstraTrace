#pragma once

#include <memory>
#include <glm/glm.hpp>
#include "core/color.hpp"
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

    virtual float sampleCoverage(glm::vec2 uv) const {
        (void)uv;
        return 1.0f;
    }

    virtual Color getAverageEmissivePower() const {
        return Color(0.0f);
    }
};
