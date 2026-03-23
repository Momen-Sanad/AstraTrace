#pragma once

#include <glm/glm.hpp>
#include "scene/lights/light_base.hpp"

class DirectionLight : public Light {
public:
    DirectionLight(glm::vec3 direction, Color color)
        : direction(glm::normalize(direction)), color(color) {}

    LightEvaluation evaluate(glm::vec3 point) const override;

    void setDirection(const glm::vec3& value) { direction = glm::normalize(value); }
    void setColor(const Color& value) { color = value; }

    const glm::vec3& getDirection() const { return direction; }
    const Color& getColor() const { return color; }

private:
    glm::vec3 direction;
    Color color;
};
