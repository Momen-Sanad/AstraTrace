#include "scene/lights/spot_light.hpp"

LightEvaluation SpotLight::evaluate(glm::vec3 point) const {
    LightEvaluation eval;
    eval.light_vector = position - point;
    eval.distance = glm::length(eval.light_vector);
    eval.light_vector *= 1.0f / eval.distance;
    float cos_angle = -glm::dot(eval.light_vector, direction);
    float attenuation = glm::smoothstep(cos_angles.y, cos_angles.x, cos_angle) / (eval.distance * eval.distance);
    eval.radiance = color * attenuation;
    return eval;
}
