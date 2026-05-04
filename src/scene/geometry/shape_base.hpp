#pragma once

#include <glm/glm.hpp>
#include "core/ray.hpp"

enum struct HitDirection {
    NONE,
    ENTERING,
    EXITING,
};

struct SurfaceData {
    HitDirection hit_direction;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    glm::vec2 uv;
};

struct AABB {
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
};

class Shape {
public:
    virtual ~Shape() = default;
    virtual bool intersect(const Ray& ray, RayHit& hit) const = 0;
    virtual SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const = 0;
    virtual AABB getBounds() const = 0;

    virtual float surfaceArea() const { return 0.0f; }
    virtual glm::vec3 geometricNormal(const Ray& ray, const RayHit& hit) const {
        return getSurfaceData(ray, hit).normal;
    }
    virtual void samplePoint(
        const glm::vec3& u,
        glm::vec3& position,
        glm::vec3& normal,
        glm::vec2& uv,
        float& pdf
    ) const {
        (void)u;
        position = glm::vec3(0.0f);
        normal = glm::vec3(0.0f, 1.0f, 0.0f);
        uv = glm::vec2(0.0f);
        pdf = 0.0f;
    }
    virtual void sampleDirection(
        const glm::vec3& u,
        const glm::vec3& point,
        glm::vec3& direction,
        float& distance,
        glm::vec2& uv,
        float& pdf
    ) const;
    virtual float computePDF(const Ray& ray, const RayHit& hit) const;
};

inline void Shape::sampleDirection(
    const glm::vec3& u,
    const glm::vec3& point,
    glm::vec3& direction,
    float& distance,
    glm::vec2& uv,
    float& pdf
) const {
    glm::vec3 position;
    glm::vec3 normal;
    samplePoint(u, position, normal, uv, pdf);

    direction = position - point;
    float distance_squared = glm::dot(direction, direction);
    if(distance_squared <= 1e-8f || pdf <= 0.0f) {
        direction = glm::vec3(0.0f, 1.0f, 0.0f);
        distance = 0.0f;
        pdf = 0.0f;
        return;
    }

    distance = glm::sqrt(distance_squared);
    direction *= 1.0f / distance;
    float cos_light = glm::abs(glm::dot(normal, -direction));
    if(cos_light <= 1e-5f) {
        pdf = 0.0f;
        return;
    }
    pdf *= distance_squared / cos_light;
}

inline float Shape::computePDF(const Ray& ray, const RayHit& hit) const {
    glm::vec3 point = ray.origin + ray.direction * hit.distance;
    glm::vec3 normal = geometricNormal(ray, hit);
    float area = surfaceArea();
    float cos_light = glm::abs(glm::dot(normal, -ray.direction));
    if(area <= 0.0f || cos_light <= 1e-5f) return 0.0f;
    return (hit.distance * hit.distance) / (area * cos_light);
}
