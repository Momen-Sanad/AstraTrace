#pragma once

#include <memory>
#include <vector>

#include <glm/glm.hpp>

#include "core/color.hpp"
#include "core/image.hpp"

struct EnvironmentSample {
    glm::vec3 direction = glm::vec3(0.0f, 1.0f, 0.0f);
    Color radiance = Color(0.0f);
    float pdf = 0.0f;
};

class EnvironmentMap {
public:
    void setConstant(Color color);
    void setImage(std::shared_ptr<Image<Color>> image, float strength = 1.0f);

    bool hasImage() const { return static_cast<bool>(image); }
    Color constantColor() const { return constant_color; }
    Color averageRadiance() const { return average_radiance; }
    Color evaluate(glm::vec3 direction) const;
    EnvironmentSample sample(glm::vec3 u) const;
    float pdf(glm::vec3 direction) const;

private:
    void rebuildDistribution();
    int pixelIndexForDirection(glm::vec3 direction) const;

    Color constant_color = Color(0.0f);
    Color average_radiance = Color(0.0f);
    std::shared_ptr<Image<Color>> image;
    std::vector<float> cdf;
    std::vector<float> pdf_by_pixel;
    float image_strength = 1.0f;
    float total_weight = 0.0f;
};
