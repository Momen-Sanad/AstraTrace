#pragma once

#include "scene/geometry/shape_base.hpp"

class Sphere : public Shape {
public:
    Sphere(glm::vec3 center, float radius)
        : center(center), radius(radius) {}

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override;

private:
    glm::vec3 center;
    float radius;
};
