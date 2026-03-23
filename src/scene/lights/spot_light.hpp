#pragma once

#include <glm/glm.hpp>
#include "scene/lights/light_base.hpp"

class SpotLight : public Light {
public:
    SpotLight(glm::vec3 position, glm::vec3 direction, float inner_cone_angle, float outer_cone_angle, Color color)
        : position(position),
          direction(glm::normalize(direction)),
          cos_angles(glm::cos(inner_cone_angle), glm::cos(outer_cone_angle)),
          color(color) {}

    LightEvaluation evaluate(glm::vec3 point) const override;

    void setDirection(const glm::vec3& value) { direction = glm::normalize(value); }
    void setPosition(const glm::vec3& value) { position = value; }
    void setColor(const Color& value) { color = value; }
    void setInnerConeAngle(float angle) { cos_angles.x = glm::cos(angle); }
    void setOuterConeAngle(float angle) { cos_angles.y = glm::cos(angle); }

    const glm::vec3& getDirection() const { return direction; }
    const glm::vec3& getPosition() const { return position; }
    const Color& getColor() const { return color; }
    float getInnerConeAngle() const { return glm::acos(cos_angles.x); }
    float getOuterConeAngle() const { return glm::acos(cos_angles.y); }
    float getCosInnerConeAngle() const { return cos_angles.x; }
    float getCosOuterConeAngle() const { return cos_angles.y; }

private:
    glm::vec3 position;
    glm::vec3 direction;
    glm::vec2 cos_angles;
    Color color;
};
