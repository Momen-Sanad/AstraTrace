#pragma once

#include <span>
#include <vector>
#include "scene/geometry/triangle.hpp"

class TriangleMesh : public Shape {
public:
    explicit TriangleMesh(const std::span<Triangle>& triangles);

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override { return bounds; }

private:
    std::vector<Triangle> triangles;
    AABB bounds;
};
