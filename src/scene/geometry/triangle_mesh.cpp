#include "scene/geometry/triangle_mesh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

void expandBounds(AABB& dst, const AABB& src) {
    dst.min = glm::min(dst.min, src.min);
    dst.max = glm::max(dst.max, src.max);
}

void expandBounds(AABB& dst, const glm::vec3& point) {
    dst.min = glm::min(dst.min, point);
    dst.max = glm::max(dst.max, point);
}

bool intersectAABB(const AABB& box, const Ray& ray, float t_min, float t_max) {
    for(int axis = 0; axis < 3; ++axis) {
        const float dir = ray.direction[axis];
        if(std::abs(dir) < 1e-8f) {
            if(ray.origin[axis] < box.min[axis] || ray.origin[axis] > box.max[axis]) {
                return false;
            }
            continue;
        }

        const float inv_d = 1.0f / dir;
        float t0 = (box.min[axis] - ray.origin[axis]) * inv_d;
        float t1 = (box.max[axis] - ray.origin[axis]) * inv_d;
        if(inv_d < 0.0f) std::swap(t0, t1);
        t_min = glm::max(t_min, t0);
        t_max = glm::min(t_max, t1);
        if(t_max < t_min) return false;
    }
    return true;
}

} // namespace

TriangleMesh::TriangleMesh(const std::span<Triangle>& triangles)
    : triangles(triangles.begin(), triangles.end()) {
    if(this->triangles.empty()) {
        bounds.min = glm::vec3(0.0f);
        bounds.max = glm::vec3(0.0f);
        return;
    }

    bounds.min = glm::vec3(std::numeric_limits<float>::max());
    bounds.max = glm::vec3(std::numeric_limits<float>::lowest());

    triangle_refs.resize(this->triangles.size());
    for(std::size_t i = 0; i < this->triangles.size(); ++i) {
        AABB triangle_bounds = this->triangles[i].getBounds();
        triangle_refs[i].triangle_index = static_cast<uint32_t>(i);
        triangle_refs[i].bounds = triangle_bounds;
        triangle_refs[i].centroid = 0.5f * (triangle_bounds.min + triangle_bounds.max);
        expandBounds(bounds, triangle_bounds);
    }

    nodes.reserve(triangle_refs.size() * 2);
    buildNode(0, static_cast<int>(triangle_refs.size()));
}

int TriangleMesh::buildNode(int start, int count) {
    Node node;
    node.bounds.min = glm::vec3(std::numeric_limits<float>::max());
    node.bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for(int i = 0; i < count; ++i) {
        expandBounds(node.bounds, triangle_refs[static_cast<std::size_t>(start + i)].bounds);
    }

    const int node_index = static_cast<int>(nodes.size());
    nodes.push_back(node);

    if(count <= 4) {
        nodes[static_cast<std::size_t>(node_index)].start = start;
        nodes[static_cast<std::size_t>(node_index)].count = count;
        return node_index;
    }

    AABB centroid_bounds;
    centroid_bounds.min = glm::vec3(std::numeric_limits<float>::max());
    centroid_bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for(int i = 0; i < count; ++i) {
        expandBounds(centroid_bounds, triangle_refs[static_cast<std::size_t>(start + i)].centroid);
    }

    const glm::vec3 extent = centroid_bounds.max - centroid_bounds.min;
    int axis = 0;
    if(extent.y > extent.x && extent.y > extent.z) axis = 1;
    else if(extent.z > extent.x) axis = 2;

    const int mid = start + count / 2;
    std::nth_element(
        triangle_refs.begin() + start,
        triangle_refs.begin() + mid,
        triangle_refs.begin() + start + count,
        [axis](const TriangleRef& a, const TriangleRef& b) {
            return a.centroid[axis] < b.centroid[axis];
        }
    );

    const int left = buildNode(start, mid - start);
    const int right = buildNode(mid, count - (mid - start));
    nodes[static_cast<std::size_t>(node_index)].left = left;
    nodes[static_cast<std::size_t>(node_index)].right = right;
    return node_index;
}

bool TriangleMesh::intersect(const Ray& ray, RayHit& hit) const {
    if(triangles.empty() || nodes.empty()) return false;

    const float ray_epsilon = 1e-5f;
    bool has_hit = false;
    float closest_distance = hit.distance;
    RayHit best_hit = hit;

    std::vector<int> stack;
    stack.reserve(nodes.size());
    stack.push_back(0);

    while(!stack.empty()) {
        const int node_index = stack.back();
        stack.pop_back();
        const Node& node = nodes[static_cast<std::size_t>(node_index)];

        if(!intersectAABB(node.bounds, ray, ray_epsilon, closest_distance)) {
            continue;
        }

        if(node.isLeaf()) {
            for(int i = 0; i < node.count; ++i) {
                const TriangleRef& tri_ref = triangle_refs[static_cast<std::size_t>(node.start + i)];
                const Triangle& triangle = triangles[tri_ref.triangle_index];
                RayHit triangle_hit{};
                if(triangle.intersect(ray, triangle_hit) && triangle_hit.distance < closest_distance) {
                    closest_distance = triangle_hit.distance;
                    best_hit = triangle_hit;
                    best_hit.surface_id = tri_ref.triangle_index;
                    has_hit = true;
                }
            }
            continue;
        }

        if(node.left >= 0) stack.push_back(node.left);
        if(node.right >= 0) stack.push_back(node.right);
    }

    if(has_hit) {
        hit = best_hit;
    }
    return has_hit;
}

SurfaceData TriangleMesh::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    if(hit.surface_id >= triangles.size()) {
        SurfaceData fallback{};
        fallback.hit_direction = HitDirection::NONE;
        fallback.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        fallback.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        fallback.bitangent = glm::vec3(0.0f, 0.0f, 1.0f);
        fallback.uv = glm::vec2(0.0f);
        return fallback;
    }
    return triangles[hit.surface_id].getSurfaceData(ray, hit);
}

