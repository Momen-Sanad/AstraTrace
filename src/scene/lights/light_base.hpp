#pragma once

#include <glm/glm.hpp>
#include "core/color.hpp"

struct LightEvaluation {
    glm::vec3 light_vector;
    float distance;
    Color radiance;
};

class Light {
public:
    virtual ~Light() = default;
    virtual LightEvaluation evaluate(glm::vec3 point) const = 0;
};
