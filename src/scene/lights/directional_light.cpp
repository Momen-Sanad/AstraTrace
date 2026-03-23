#include "scene/lights/directional_light.hpp"

#include <limits>

LightEvaluation DirectionLight::evaluate(glm::vec3 point) const {
    (void)point;
    LightEvaluation eval;
    eval.light_vector = -direction;
    eval.distance = std::numeric_limits<float>::max();
    eval.radiance = color;
    return eval;
}
