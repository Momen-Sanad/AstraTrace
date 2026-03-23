#include "scene/lights/point_light.hpp"

LightEvaluation PointLight::evaluate(glm::vec3 point) const {
    LightEvaluation eval;
    eval.light_vector = position - point;
    eval.distance = glm::length(eval.light_vector);
    eval.light_vector *= 1.0f / eval.distance;
    float attenuation = 1.0f / (eval.distance * eval.distance);
    eval.radiance = color * attenuation;
    return eval;
}
