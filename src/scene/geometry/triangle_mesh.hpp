#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include "scene/geometry/triangle.hpp"

class TriangleMesh : public Shape {
public:
    explicit TriangleMesh(const std::span<Triangle>& triangles);

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override { return bounds; }
    float surfaceArea() const override { return total_area; }
    glm::vec3 geometricNormal(const Ray& ray, const RayHit& hit) const override;
    void samplePoint(
        const glm::vec3& u,
        glm::vec3& position,
        glm::vec3& normal,
        glm::vec2& uv,
        float& pdf
    ) const override;
    float computePDF(const Ray& ray, const RayHit& hit) const override;

private:
    struct TriangleRef {
        uint32_t triangle_index = 0;
        AABB bounds;
        glm::vec3 centroid = glm::vec3(0.0f);
    };

    struct Node {
        AABB bounds;
        int left = -1;
        int right = -1;
        int start = 0;
        int count = 0;
        bool isLeaf() const { return left < 0 && right < 0; }
    };

    int buildNode(int start, int count);

    std::vector<Triangle> triangles;
    std::vector<TriangleRef> triangle_refs;
    std::vector<Node> nodes;
    std::vector<float> triangle_area_cdf;
    float total_area = 0.0f;
    AABB bounds;
};
