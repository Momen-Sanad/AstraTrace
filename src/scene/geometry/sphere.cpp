#include "scene/geometry/sphere.hpp"

bool Sphere::intersect(const Ray& ray, RayHit& hit) const {
    glm::vec3 diff = center - ray.origin;
    float d2inv = 1.0f / glm::dot(ray.direction, ray.direction);
    float to = glm::dot(ray.direction, diff) * d2inv;
    float det = to * to - (glm::dot(diff, diff) - radius * radius) * d2inv;
    if(det < 0.0f) return false;
    float d = glm::sqrt(det);
    float t = to - d;
    if(t > 1e-5f) {
        hit.distance = t;
        glm::vec3 normal = ((ray.origin + t * ray.direction) - center) / radius;
        hit.surface_coords = normal;
        hit.surface_id = normal.z > 0.0f ? 0 : 1;
        return true;
    }
    t = to + d;
    if(t > 1e-5f) {
        hit.distance = t;
        glm::vec3 normal = ((ray.origin + t * ray.direction) - center) / radius;
        hit.surface_coords = normal;
        hit.surface_id = normal.z > 0.0f ? 0 : 1;
        return true;
    }
    return false;
}

SurfaceData Sphere::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    float z = glm::sqrt(glm::max(0.0f, 1.0f - hit.surface_coords.x * hit.surface_coords.x - hit.surface_coords.y * hit.surface_coords.y));
    if(hit.surface_id != 0) z = -z;
    glm::vec3 normal = glm::vec3(hit.surface_coords, z);

    SurfaceData surface;
    surface.normal = normal;
    if(glm::abs(surface.normal.y) == 1.0f) {
        surface.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        surface.bitangent = glm::vec3(0.0f, 0.0f, -surface.normal.y);
    } else {
        surface.tangent = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), surface.normal));
        surface.bitangent = glm::cross(surface.normal, surface.tangent);
    }

    float u = std::atan2(normal.z, normal.x) / glm::radians(360.0f);
    if(u < 0.0f) u += 1.0f;
    float v = 0.5f * std::asin(normal.y) / glm::radians(90.0f) + 0.5f;
    surface.uv = glm::vec2(u, v);

    if(glm::dot(ray.direction, surface.normal) > 0.0f) {
        surface.normal *= -1.0f;
        surface.hit_direction = HitDirection::EXITING;
    } else {
        surface.hit_direction = HitDirection::ENTERING;
    }

    return surface;
}

AABB Sphere::getBounds() const {
    AABB bounds;
    bounds.min = center - glm::vec3(radius);
    bounds.max = center + glm::vec3(radius);
    return bounds;
}
