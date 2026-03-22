#include "light.hpp"

LightEvaluation DirectionLight::evaluate(glm::vec3 point) const {
    LightEvaluation eval;
    eval.light_vector = -direction; // For directional lights, the light vector is constant for all points.
    eval.distance = std::numeric_limits<float>::max(); // The light is at an infinite distance away.
    eval.radiance = color; // For directional lights, the radiance is constant for all points.
    return eval;
}

LightEvaluation PointLight::evaluate(glm::vec3 point) const {
    LightEvaluation eval;
    // Compute the vector and distance from the point to the light position.
    eval.light_vector = position - point;
    eval.distance = glm::length(eval.light_vector);
    eval.light_vector *= 1.0f / eval.distance;
    // Apply distance attenuation (falloff) because the further the point is from the light position, the less radiance it receives.
    float attenuation = 1.0f / (eval.distance * eval.distance);
    eval.radiance = color * attenuation;
    return eval;
}

LightEvaluation SpotLight::evaluate(glm::vec3 point) const {
    LightEvaluation eval;
    // Compute the vector and distance from the point to the light position.
    eval.light_vector = position - point;
    eval.distance = glm::length(eval.light_vector);
    eval.light_vector *= 1.0f / eval.distance;
    float cos_angle = -glm::dot(eval.light_vector, direction);
    // Apply distance and code attenuation (falloff) because the further the point is from the light position and cone direction, 
    // the less radiance it receives. We will use the smoothstep function for the cone falloff like in three.js.
    float attenuation = glm::smoothstep(cos_angles.y, cos_angles.x, cos_angle) / (eval.distance * eval.distance);
    eval.radiance = color * attenuation;
    return eval;
}