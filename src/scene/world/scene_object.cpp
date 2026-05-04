#include "scene/world/scene_object.hpp"
// median heuristic in low-level bvh
// bvh top-level too

#include <algorithm>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

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

void expandBounds(AABB& bounds, const glm::vec3& point) {
    bounds.min = glm::min(bounds.min, point);
    bounds.max = glm::max(bounds.max, point);
}

float maxChannel(const Color& color) {
    return glm::max(color.r, glm::max(color.g, color.b));
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

glm::vec3 SceneObject::getGeometricNormal(const Ray& ray, const RayHit& hit) const {
    if(is_identity_transform) {
        return shape->geometricNormal(ray, hit);
    }

    Ray transformed_ray = {
        .origin = transform_inverse * glm::vec4(ray.origin, 1.0f),
        .direction = transform_inverse * glm::vec4(ray.direction, 0.0f)
    };
    glm::vec3 normal = shape->geometricNormal(transformed_ray, hit);
    normal = glm::normalize(glm::transpose(transform_inverse) * glm::vec4(normal, 0.0f));
    if(glm::dot(ray.direction, normal) > 0.0f) normal *= -1.0f;
    return normal;
}

bool SceneObject::isEmissive() const {
    return material && maxChannel(material->getAverageEmissivePower()) > 0.0f && shape && shape->surfaceArea() > 0.0f;
}

void SceneObject::samplePoint(
    const glm::vec3& u,
    glm::vec3& point,
    glm::vec3& normal,
    glm::vec2& uv,
    float& pdf
) const {
    shape->samplePoint(u, point, normal, uv, pdf);
    if(is_identity_transform) return;

    point = glm::vec3(transform * glm::vec4(point, 1.0f));
    normal = glm::normalize(glm::transpose(transform_inverse) * glm::vec4(normal, 0.0f));

    float scale_x = glm::length(glm::vec3(transform[0]));
    float scale_y = glm::length(glm::vec3(transform[1]));
    float scale_z = glm::length(glm::vec3(transform[2]));
    float area_scale = glm::max(1e-6f, (scale_x * scale_y + scale_x * scale_z + scale_y * scale_z) / 3.0f);
    pdf /= area_scale;
}

void SceneObject::sampleDirection(
    const glm::vec3& u,
    const glm::vec3& point,
    glm::vec3& direction,
    float& distance,
    glm::vec2& uv,
    float& pdf
) const {
    if(is_identity_transform) {
        shape->sampleDirection(u, point, direction, distance, uv, pdf);
        return;
    }

    glm::vec3 sampled_point;
    glm::vec3 sampled_normal;
    samplePoint(u, sampled_point, sampled_normal, uv, pdf);
    direction = sampled_point - point;
    float distance_squared = glm::dot(direction, direction);
    if(distance_squared <= 1e-8f || pdf <= 0.0f) {
        distance = 0.0f;
        pdf = 0.0f;
        return;
    }
    distance = glm::sqrt(distance_squared);
    direction *= 1.0f / distance;
    float cos_light = glm::abs(glm::dot(sampled_normal, -direction));
    pdf = cos_light > 1e-5f ? pdf * distance_squared / cos_light : 0.0f;
}

float SceneObject::computePDF(const Ray& ray, const RayHit& hit) const {
    if(is_identity_transform) return shape->computePDF(ray, hit);

    float area = shape->surfaceArea();
    if(area <= 0.0f) return 0.0f;
    glm::vec3 normal = getGeometricNormal(ray, hit);
    float cos_light = glm::abs(glm::dot(normal, -ray.direction));
    if(cos_light <= 1e-5f) return 0.0f;
    return (hit.distance * hit.distance) / (area * cos_light);
}

LightEvaluation SceneObject::evaluate(glm::vec3 point) const {
    glm::vec3 direction;
    float distance = 0.0f;
    glm::vec2 uv;
    float sample_pdf = 0.0f;
    sampleDirection(glm::vec3(0.5f), point, direction, distance, uv, sample_pdf);
    return {
        .light_vector = direction,
        .distance = distance,
        .radiance = sample_pdf > 0.0f ? material->sampleEmissive(uv) / sample_pdf : Color(0.0f)
    };
}

LightSample SceneObject::sample(glm::vec3 point, const glm::vec3& u) const {
    glm::vec3 direction;
    float distance = 0.0f;
    glm::vec2 uv;
    float sample_pdf = 0.0f;
    sampleDirection(u, point, direction, distance, uv, sample_pdf);
    return {
        .light_vector = direction,
        .distance = distance,
        .radiance = sample_pdf > 0.0f ? material->sampleEmissive(uv) : Color(0.0f),
        .pdf = sample_pdf,
        .delta = false
    };
}

float SceneObject::pdf(const Ray& ray, const RayHit& hit) const {
    return computePDF(ray, hit);
}

float SceneObject::power() const {
    if(!material || !shape) return 0.0f;
    return maxChannel(material->getAverageEmissivePower()) * shape->surfaceArea() * 3.1415926535f;
}

float SceneObject::estimatePowerAt(glm::vec3 point, glm::vec3 normal) const {
    float dist2 = glm::dot(bounds_center - point, bounds_center - point);
    dist2 = glm::max(dist2, 1e-4f);
    glm::vec3 to_light = glm::normalize(bounds_center - point);
    return power() * glm::max(0.0f, glm::dot(normal, to_light)) / dist2;
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

    world_bounds.min = glm::vec3(std::numeric_limits<float>::max());
    world_bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for(int x = 0; x < 2; ++x) {
        for(int y = 0; y < 2; ++y) {
            for(int z = 0; z < 2; ++z) {
                glm::vec3 corner(
                    x ? local_bounds.max.x : local_bounds.min.x,
                    y ? local_bounds.max.y : local_bounds.min.y,
                    z ? local_bounds.max.z : local_bounds.min.z
                );
                expandBounds(world_bounds, glm::vec3(transform * glm::vec4(corner, 1.0f)));
            }
        }
    }

    bounds_center = glm::vec3(transform * glm::vec4(local_center, 1.0f));
    float scale_x = glm::length(glm::vec3(transform[0]));
    float scale_y = glm::length(glm::vec3(transform[1]));
    float scale_z = glm::length(glm::vec3(transform[2]));
    float max_scale = glm::max(scale_x, glm::max(scale_y, scale_z));
    bounds_radius = local_radius * max_scale;
}
