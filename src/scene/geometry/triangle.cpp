#include "scene/geometry/triangle.hpp"

AABB Triangle::getBounds() const {
    AABB bounds;
    bounds.min = glm::min(v0.position, glm::min(v1.position, v2.position));
    bounds.max = glm::max(v0.position, glm::max(v1.position, v2.position));
    return bounds;
}

float Triangle::surfaceArea() const {
    return 0.5f * glm::length(glm::cross(v1.position - v0.position, v2.position - v0.position));
}

glm::vec3 Triangle::geometricNormal(const Ray& ray, const RayHit& hit) const {
    (void)hit;
    glm::vec3 normal = glm::normalize(glm::cross(v1.position - v0.position, v2.position - v0.position));
    if(glm::dot(ray.direction, normal) > 0.0f) normal *= -1.0f;
    return normal;
}

void Triangle::samplePoint(
    const glm::vec3& u,
    glm::vec3& position,
    glm::vec3& normal,
    glm::vec2& uv,
    float& pdf
) const {
    float su0 = glm::sqrt(glm::clamp(u.x, 0.0f, 1.0f));
    float w0 = 1.0f - su0;
    float w1 = su0 * (1.0f - glm::clamp(u.y, 0.0f, 1.0f));
    float w2 = su0 * glm::clamp(u.y, 0.0f, 1.0f);
    position = w0 * v0.position + w1 * v1.position + w2 * v2.position;
    normal = glm::normalize(glm::cross(v1.position - v0.position, v2.position - v0.position));
    uv = w0 * v0.uv + w1 * v1.uv + w2 * v2.uv;
    float area = surfaceArea();
    pdf = area > 0.0f ? 1.0f / area : 0.0f;
}

bool Triangle::intersect(const Ray& ray, RayHit& hit) const {
    const float epsilon = 1e-8f;
    glm::vec3 edge1 = v1.position - v0.position;
    glm::vec3 edge2 = v2.position - v0.position;
    glm::vec3 p = glm::cross(ray.direction, edge2);
    float det = glm::dot(edge1, p);
    if(glm::abs(det) < epsilon) return false;

    float inv_det = 1.0f / det;
    glm::vec3 t = ray.origin - v0.position;
    float w1 = glm::dot(t, p) * inv_det;
    if(w1 < 0.0f || w1 > 1.0f) return false;

    glm::vec3 q = glm::cross(t, edge1);
    float w2 = glm::dot(ray.direction, q) * inv_det;
    if(w2 < 0.0f || (w1 + w2) > 1.0f) return false;

    float hit_distance = glm::dot(edge2, q) * inv_det;
    if(hit_distance <= epsilon) return false;

    hit.distance = hit_distance;
    hit.surface_coords = glm::vec2(w1, w2);
    return true;
}

SurfaceData Triangle::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    float w0 = 1.0f - hit.surface_coords.x - hit.surface_coords.y;
    float w1 = hit.surface_coords.x;
    float w2 = hit.surface_coords.y;

    SurfaceData surface;
    surface.normal = v0.normal * w0 + v1.normal * w1 + v2.normal * w2;
    surface.tangent = v0.tangent * w0 + v1.tangent * w1 + v2.tangent * w2;
    surface.bitangent = v0.bitangent * w0 + v1.bitangent * w1 + v2.bitangent * w2;
    surface.uv = v0.uv * w0 + v1.uv * w1 + v2.uv * w2;
    if(glm::dot(ray.direction, surface.normal) > 0.0f) {
        surface.normal *= -1.0f;
        surface.hit_direction = HitDirection::EXITING;
    } else {
        surface.hit_direction = HitDirection::ENTERING;
    }
    return surface;
}
