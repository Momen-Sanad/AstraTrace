#include "scene/world/scene_object.hpp"
// median heuristic in low-level bvh
// bvh top-level too

namespace {

bool raySphereMayHit(
    const Ray& ray,
    const glm::vec3& center,
    float radius,
    float max_distance
) {
    if(radius <= 0.0f) return true;

    glm::vec3 oc = ray.origin - center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - a * c;
    if(discriminant < 0.0f) return false;

    float sqrt_discriminant = glm::sqrt(discriminant);
    float inv_a = 1.0f / a;
    float t0 = (-b - sqrt_discriminant) * inv_a;
    float t1 = (-b + sqrt_discriminant) * inv_a;
    if(t1 < 0.0f) return false;
    return t0 <= max_distance;
}

}

bool SceneObject::intersect(const Ray& ray, RayHit& hit) const {
    if(is_identity_transform) {
        return shape->intersect(ray, hit);
    }

    Ray transformed_ray = {
        .origin = transform_inverse * glm::vec4(ray.origin, 1.0f),
        .direction = transform_inverse * glm::vec4(ray.direction, 0.0f)
    };
    return shape->intersect(transformed_ray, hit);
}

bool SceneObject::mayIntersect(const Ray& ray, float max_distance) const {
    return raySphereMayHit(ray, bounds_center, bounds_radius, max_distance);
}

SurfaceData SceneObject::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    if(is_identity_transform) {
        return shape->getSurfaceData(ray, hit);
    }

    Ray transformed_ray = {
        .origin = transform_inverse * glm::vec4(ray.origin, 1.0f),
        .direction = transform_inverse * glm::vec4(ray.direction, 0.0f)
    };

    SurfaceData surface = shape->getSurfaceData(transformed_ray, hit);
    surface.normal = glm::normalize(glm::transpose(transform_inverse) * glm::vec4(surface.normal, 0.0f));
    surface.tangent = glm::normalize(transform * glm::vec4(surface.tangent, 0.0f));
    surface.bitangent = glm::normalize(transform * glm::vec4(surface.bitangent, 0.0f));
    return surface;
}

void SceneObject::update() {
    if(!is_dirty) return;
    is_dirty = false;

    is_identity_transform =
        position == glm::vec3(0.0f) &&
        rotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f) &&
        scale == glm::vec3(1.0f);

    if(is_identity_transform) {
        transform = glm::mat4(1.0f);
        transform_inverse = glm::mat4(1.0f);
    } else {
        transform = glm::translate(glm::mat4(1.0f), position)
            * glm::mat4_cast(rotation)
            * glm::scale(glm::mat4(1.0f), scale);
        transform_inverse = glm::inverse(transform);
    }

    AABB local_bounds = shape->getBounds();
    glm::vec3 local_center = 0.5f * (local_bounds.min + local_bounds.max);
    float local_radius = glm::length(0.5f * (local_bounds.max - local_bounds.min));

    bounds_center = glm::vec3(transform * glm::vec4(local_center, 1.0f));
    float scale_x = glm::length(glm::vec3(transform[0]));
    float scale_y = glm::length(glm::vec3(transform[1]));
    float scale_z = glm::length(glm::vec3(transform[2]));
    float max_scale = glm::max(scale_x, glm::max(scale_y, scale_z));
    bounds_radius = local_radius * max_scale;
}
