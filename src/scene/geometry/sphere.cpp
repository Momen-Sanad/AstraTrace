#include "scene/geometry/sphere.hpp"

#include <cmath>
#include <glm/gtc/constants.hpp>

namespace {

glm::vec3 sampleUnitSphere(const glm::vec2& u) {
    float z = 1.0f - 2.0f * glm::clamp(u.x, 0.0f, 1.0f);
    float r = glm::sqrt(glm::max(0.0f, 1.0f - z * z));
    float phi = glm::two_pi<float>() * glm::clamp(u.y, 0.0f, 1.0f);
    return glm::vec3(r * glm::cos(phi), z, r * glm::sin(phi));
}

}

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

float Sphere::surfaceArea() const {
    return 4.0f * glm::pi<float>() * radius * radius;
}

glm::vec3 Sphere::geometricNormal(const Ray& ray, const RayHit& hit) const {
    glm::vec3 normal = glm::normalize((ray.origin + ray.direction * hit.distance) - center);
    if(glm::dot(ray.direction, normal) > 0.0f) normal *= -1.0f;
    return normal;
}

void Sphere::samplePoint(
    const glm::vec3& u,
    glm::vec3& position,
    glm::vec3& normal,
    glm::vec2& uv,
    float& pdf
) const {
    normal = sampleUnitSphere(glm::vec2(u));
    position = center + radius * normal;
    float tex_u = std::atan2(normal.z, normal.x) / glm::two_pi<float>();
    if(tex_u < 0.0f) tex_u += 1.0f;
    float tex_v = 0.5f * std::asin(normal.y) / glm::half_pi<float>() + 0.5f;
    uv = glm::vec2(tex_u, tex_v);
    pdf = 1.0f / surfaceArea();
}
