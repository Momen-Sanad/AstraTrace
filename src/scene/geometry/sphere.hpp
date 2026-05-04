#pragma once

#include "scene/geometry/shape_base.hpp"

class Sphere : public Shape {
public:
    Sphere(glm::vec3 center, float radius)
        : center(center), radius(radius) {}

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override;
    float surfaceArea() const override;
    glm::vec3 geometricNormal(const Ray& ray, const RayHit& hit) const override;
    void samplePoint(
        const glm::vec3& u,
        glm::vec3& position,
        glm::vec3& normal,
        glm::vec2& uv,
        float& pdf
    ) const override;

private:
    glm::vec3 center;
    float radius;
};
