#pragma once

#include <glm/glm.hpp>
#include "core/color.hpp"
#include "core/ray.hpp"

struct LightEvaluation {
    glm::vec3 light_vector;
    float distance;
    Color radiance;
};

struct LightSample {
    glm::vec3 light_vector = glm::vec3(0.0f, 1.0f, 0.0f);
    float distance = 0.0f;
    Color radiance = Color(0.0f);
    float pdf = 0.0f;
    bool delta = true;
};

class Light {
public:
    virtual ~Light() = default;
    virtual LightEvaluation evaluate(glm::vec3 point) const = 0;
    virtual LightSample sample(glm::vec3 point, const glm::vec3& u) const {
        (void)u;
        LightEvaluation eval = evaluate(point);
        return {
            .light_vector = eval.light_vector,
            .distance = eval.distance,
            .radiance = eval.radiance,
            .pdf = 0.0f,
            .delta = true
        };
    }
    virtual float pdf(const Ray& ray, const RayHit& hit) const {
        (void)ray;
        (void)hit;
        return 0.0f;
    }
    virtual float power() const = 0;
    virtual float estimatePowerAt(glm::vec3 point, glm::vec3 normal) const {
        LightEvaluation eval = evaluate(point);
        return glm::max(0.0f, glm::dot(eval.light_vector, normal)) *
            glm::max(eval.radiance.r, glm::max(eval.radiance.g, eval.radiance.b));
    }
    virtual bool isDelta() const { return true; }
};
