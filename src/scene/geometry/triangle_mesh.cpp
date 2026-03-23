#include "scene/geometry/triangle_mesh.hpp"

TriangleMesh::TriangleMesh(const std::span<Triangle>& triangles)
    : triangles(triangles.begin(), triangles.end()) {
    if(this->triangles.empty()) {
        bounds.min = glm::vec3(0.0f);
        bounds.max = glm::vec3(0.0f);
        return;
    }

    bounds = this->triangles[0].getBounds();
    for(std::size_t i = 1; i < this->triangles.size(); ++i) {
        AABB triangle_bounds = this->triangles[i].getBounds();
        bounds.min = glm::min(bounds.min, triangle_bounds.min);
        bounds.max = glm::max(bounds.max, triangle_bounds.max);
    }
}

bool TriangleMesh::intersect(const Ray& ray, RayHit& hit) const {
    bool has_hit = false;
    for(uint32_t idx = 0; idx < triangles.size(); idx++) {
        const Triangle& triangle = triangles[idx];
        RayHit triangle_hit;
        if(triangle.intersect(ray, triangle_hit)) {
            if(!has_hit || triangle_hit.distance < hit.distance) {
                hit = triangle_hit;
                hit.surface_id = idx;
                has_hit = true;
            }
        }
    }
    return has_hit;
}

SurfaceData TriangleMesh::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    return triangles[hit.surface_id].getSurfaceData(ray, hit);
}
