#pragma once

#include <glm/glm.hpp>
#include "color.hpp"

// This will contain parameters of the light at a given point p in space.
struct LightEvaluation {
    glm::vec3 light_vector; // The normalized vector going from p to the light source.
    float distance; // The distance from p to the light source.
    Color radiance; // The amount of radiance emitted from the light source and reaching p from the direction light_vector.
};

// The base class for all lights. It provides an interface to evaluate the light parameters at a given point.
class Light {
public:
    virtual LightEvaluation evaluate(glm::vec3 point) const = 0;
};

// A directional light is a light that comes from an infinite distance and has no falloff. It is defined by a direction and a color.
class DirectionLight : public Light {
public:
    DirectionLight(glm::vec3 direction, Color color) : direction(glm::normalize(direction)), color(color) {}
    LightEvaluation evaluate(glm::vec3 point) const override;

    // Define Setters
    void setDirection(const glm::vec3& direction) { this->direction = glm::normalize(direction); }
    void setColor(const Color& color) { this->color = color; }
    // Define Getters
    const glm::vec3& getDirection() const { return direction; }
    const Color& getColor() const { return color; }
private:
    glm::vec3 direction; // The direction of the light in world space.
    Color color; // The radiance of the light arriving from the light direction at any point.
};

// A point light is a light that comes from a single point and has distance falloff. It is defined by a position and a color.
class PointLight : public Light {
public:
    PointLight(glm::vec3 position, Color color) : position(position), color(color) {}
    LightEvaluation evaluate(glm::vec3 point) const override;

    // Define Setters
    void setPosition(const glm::vec3& position) { this->position = position; }
    void setColor(const Color& color) { this->color = color; }
    // Define Getters
    const glm::vec3& getPosition() const { return position; }
    const Color& getColor() const { return color; }
private:
    glm::vec3 position; // The position of the light in world space.
    Color color; // The radiance of the light arriving from the light direction to any point at distance 1 from the light.
};

// A spot light is a light that comes from a single point and has distance falloff and cone falloff. It is defined by a position, direction, cone angles and a color.
class SpotLight : public Light {
public:
    SpotLight(glm::vec3 position, glm::vec3 direction, float inner_cone_angle, float outer_cone_angle, Color color) : 
        position(position), direction(glm::normalize(direction)), 
        cos_angles(glm::cos(inner_cone_angle), glm::cos(outer_cone_angle)), color(color) {}

    LightEvaluation evaluate(glm::vec3 point) const override;

    // Define Setters
    void setDirection(const glm::vec3& direction) { this->direction = glm::normalize(direction); }
    void setPosition(const glm::vec3& position) { this->position = position; }
    void setColor(const Color& color) { this->color = color; }
    void setInnerConeAngle(float angle) { cos_angles.x = glm::cos(angle); }
    void setOuterConeAngle(float angle) { cos_angles.y = glm::cos(angle); }
    // Define Getters
    const glm::vec3& getDirection() const { return direction; }
    const glm::vec3& getPosition() const { return position; }
    const Color& getColor() const { return color; }
    float getInnerConeAngle() const { return glm::acos(cos_angles.x); }
    float getOuterConeAngle() const { return glm::acos(cos_angles.y); }
    float getCosInnerConeAngle() const { return cos_angles.x; }
    float getCosOuterConeAngle() const { return cos_angles.y; }

private:
    glm::vec3 position, direction; // The position and cone direction of the light in world space.
    glm::vec2 cos_angles; // The cosine of the inner (in x) and outer (in y) cone angles.
    Color color; // The radiance of the light arriving from the light direction to any point at distance 1 from the light.
};