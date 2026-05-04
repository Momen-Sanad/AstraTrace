#pragma once

#include <glm/glm.hpp>
#include "scene/lights/light_base.hpp"

class PointLight : public Light {
public:
    PointLight(glm::vec3 position, Color color)
        : position(position), color(color) {}

    LightEvaluation evaluate(glm::vec3 point) const override;
    float power() const override { return 4.0f * 3.1415926535f * glm::max(color.r, glm::max(color.g, color.b)); }

    void setPosition(const glm::vec3& value) { position = value; }
    void setColor(const Color& value) { color = value; }

    const glm::vec3& getPosition() const { return position; }
    const Color& getColor() const { return color; }

private:
    glm::vec3 position;
    Color color;
};
