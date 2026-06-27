#include "scene/world/environment.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

#include <glm/gtc/constants.hpp>

namespace {

float luminance(Color color) {
    return glm::dot(color, Color(0.2126f, 0.7152f, 0.0722f));
}

glm::vec2 directionToUv(glm::vec3 direction) {
    direction = glm::normalize(direction);
    const float phi = std::atan2(direction.z, direction.x);
    const float theta = std::acos(glm::clamp(direction.y, -1.0f, 1.0f));
    return glm::vec2(
        phi / (2.0f * glm::pi<float>()) + 0.5f,
        1.0f - theta / glm::pi<float>()
    );
}

glm::vec3 uvToDirection(glm::vec2 uv) {
    const float theta = (1.0f - glm::clamp(uv.y, 0.0f, 1.0f)) * glm::pi<float>();
    const float phi = (uv.x - 0.5f) * 2.0f * glm::pi<float>();
    const float sin_theta = std::sin(theta);
    return glm::normalize(glm::vec3(
        std::cos(phi) * sin_theta,
        std::cos(theta),
        std::sin(phi) * sin_theta
    ));
}

Color sampleImageClamped(const std::shared_ptr<Image<Color>>& image, glm::vec2 uv) {
    if(!image || image->getWidth() <= 0 || image->getHeight() <= 0) return Color(0.0f);
    uv.x = glm::fract(uv.x);
    uv.y = glm::clamp(uv.y, 0.0f, 1.0f);
    return sampleImage(image, uv);
}

} // namespace

void EnvironmentMap::setConstant(Color color) {
    constant_color = glm::max(color, Color(0.0f));
    average_radiance = constant_color;
    image.reset();
    cdf.clear();
    pdf_by_pixel.clear();
    total_weight = 0.0f;
    image_strength = 1.0f;
}

void EnvironmentMap::setImage(std::shared_ptr<Image<Color>> source, float strength) {
    image = std::move(source);
    image_strength = glm::max(0.0f, strength);
    rebuildDistribution();
}

Color EnvironmentMap::evaluate(glm::vec3 direction) const {
    if(!image) return constant_color;
    return image_strength * sampleImageClamped(image, directionToUv(direction));
}

EnvironmentSample EnvironmentMap::sample(glm::vec3 u) const {
    EnvironmentSample sample;
    if(!image || cdf.empty() || total_weight <= 0.0f) {
        const float z = 1.0f - 2.0f * glm::clamp(u.x, 0.0f, 1.0f);
        const float r = std::sqrt(glm::max(0.0f, 1.0f - z * z));
        const float phi = 2.0f * glm::pi<float>() * glm::clamp(u.y, 0.0f, 1.0f);
        sample.direction = glm::vec3(r * std::cos(phi), z, r * std::sin(phi));
        sample.radiance = evaluate(sample.direction);
        sample.pdf = 1.0f / (4.0f * glm::pi<float>());
        return sample;
    }

    const float xi = glm::clamp(u.x, 0.0f, std::nextafter(1.0f, 0.0f));
    auto it = std::lower_bound(cdf.begin(), cdf.end(), xi);
    std::size_t index = static_cast<std::size_t>(std::distance(cdf.begin(), it));
    index = std::min(index, cdf.size() - 1u);

    const int width = image->getWidth();
    const int height = image->getHeight();
    const int x = static_cast<int>(index % static_cast<std::size_t>(width));
    const int y = static_cast<int>(index / static_cast<std::size_t>(width));

    const glm::vec2 uv(
        (static_cast<float>(x) + glm::clamp(u.y, 0.0f, std::nextafter(1.0f, 0.0f))) / static_cast<float>(width),
        (static_cast<float>(y) + glm::clamp(u.z, 0.0f, std::nextafter(1.0f, 0.0f))) / static_cast<float>(height)
    );

    sample.direction = uvToDirection(uv);
    sample.radiance = evaluate(sample.direction);
    sample.pdf = pdf_by_pixel[index];
    return sample;
}

float EnvironmentMap::pdf(glm::vec3 direction) const {
    if(!image || pdf_by_pixel.empty() || total_weight <= 0.0f) {
        return 1.0f / (4.0f * glm::pi<float>());
    }

    const int index = pixelIndexForDirection(direction);
    if(index < 0 || index >= static_cast<int>(pdf_by_pixel.size())) return 0.0f;
    return pdf_by_pixel[static_cast<std::size_t>(index)];
}

void EnvironmentMap::rebuildDistribution() {
    cdf.clear();
    pdf_by_pixel.clear();
    total_weight = 0.0f;
    average_radiance = Color(0.0f);
    if(!image || image->getWidth() <= 0 || image->getHeight() <= 0) {
        image.reset();
        average_radiance = constant_color;
        return;
    }

    const int width = image->getWidth();
    const int height = image->getHeight();
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    cdf.reserve(count);
    pdf_by_pixel.assign(count, 0.0f);

    const Color* pixels = image->getPixels();
    for(int y = 0; y < height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        const float theta = (1.0f - v) * glm::pi<float>();
        const float sin_theta = glm::max(0.0f, std::sin(theta));
        for(int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * width + x);
            const Color radiance = image_strength * glm::max(pixels[index], Color(0.0f));
            average_radiance += radiance;
            total_weight += glm::max(0.0f, luminance(radiance)) * sin_theta;
            cdf.push_back(total_weight);
        }
    }

    average_radiance /= static_cast<float>(count);
    if(total_weight <= 0.0f) {
        cdf.clear();
        return;
    }

    for(float& value : cdf) value /= total_weight;

    const float delta_theta = glm::pi<float>() / static_cast<float>(height);
    const float delta_phi = 2.0f * glm::pi<float>() / static_cast<float>(width);
    for(int y = 0; y < height; ++y) {
        const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
        const float theta = (1.0f - v) * glm::pi<float>();
        const float sin_theta = glm::max(1e-4f, std::sin(theta));
        for(int x = 0; x < width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * width + x);
            const float previous = index == 0 ? 0.0f : cdf[index - 1u];
            const float pixel_probability = glm::max(0.0f, cdf[index] - previous);
            pdf_by_pixel[index] = pixel_probability / glm::max(1e-8f, delta_theta * delta_phi * sin_theta);
        }
    }
}

int EnvironmentMap::pixelIndexForDirection(glm::vec3 direction) const {
    if(!image) return -1;
    const glm::vec2 uv = directionToUv(direction);
    const int width = image->getWidth();
    const int height = image->getHeight();
    const int x = glm::clamp(static_cast<int>(glm::fract(uv.x) * static_cast<float>(width)), 0, width - 1);
    const int y = glm::clamp(static_cast<int>(glm::clamp(uv.y, 0.0f, 1.0f) * static_cast<float>(height)), 0, height - 1);
    return y * width + x;
}
