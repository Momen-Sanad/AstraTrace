#pragma once

#include "scene/geometry/shape_base.hpp"

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    glm::vec2 uv;
};

class Triangle : public Shape {
public:
    Triangle(Vertex v0, Vertex v1, Vertex v2)
        : v0(v0), v1(v1), v2(v2) {}

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override;

private:
    Vertex v0;
    Vertex v1;
    Vertex v2;
};
