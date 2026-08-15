#include "scene/world/scene_object.hpp"

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
        if(!shape->intersect(ray, hit)) return false;
        hit.object_distance = hit.distance;
        return true;
    }

    Ray transformed_ray = {
        .origin = transform_inverse * glm::vec4(ray.origin, 1.0f),
        .direction = transform_inverse * glm::vec4(ray.direction, 0.0f)
    };
    RayHit local_hit = hit;
    if(!shape->intersect(transformed_ray, local_hit)) return false;

    glm::vec3 local_point = transformed_ray.origin + transformed_ray.direction * local_hit.distance;
    glm::vec3 world_point = glm::vec3(transform * glm::vec4(local_point, 1.0f));
    glm::vec3 world_delta = world_point - ray.origin;
    float direction_length2 = glm::max(glm::dot(ray.direction, ray.direction), 1e-8f);

    hit = local_hit;
    hit.object_distance = local_hit.distance;
    hit.distance = glm::dot(world_delta, ray.direction) / direction_length2;
    return hit.distance > 0.0f;
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

    RayHit local_hit = hit;
    local_hit.distance = hit.object_distance;
    SurfaceData surface = shape->getSurfaceData(transformed_ray, local_hit);
    glm::mat3 normal_transform = glm::transpose(glm::mat3(transform_inverse));
    glm::mat3 tangent_transform = glm::mat3(transform);
    surface.normal = glm::normalize(normal_transform * surface.normal);
    surface.tangent = glm::normalize(tangent_transform * surface.tangent);
    surface.bitangent = glm::normalize(tangent_transform * surface.bitangent);
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
    RayHit local_hit = hit;
    local_hit.distance = hit.object_distance;
    glm::vec3 normal = shape->geometricNormal(transformed_ray, local_hit);
    glm::mat3 normal_transform = glm::transpose(glm::mat3(transform_inverse));
    normal = glm::normalize(normal_transform * normal);
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
    glm::vec2 uv1;
    samplePoint(u, point, normal, uv, uv1, pdf);
}

void SceneObject::samplePoint(
    const glm::vec3& u,
    glm::vec3& point,
    glm::vec3& normal,
    glm::vec2& uv,
    glm::vec2& uv1,
    float& pdf
) const {
    shape->samplePoint(u, point, normal, uv, uv1, pdf);
    if(is_identity_transform) return;

    point = glm::vec3(transform * glm::vec4(point, 1.0f));
    glm::mat3 normal_transform = glm::transpose(glm::mat3(transform_inverse));
    normal = glm::normalize(normal_transform * normal);
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
    glm::vec2 uv1;
    float emitter_cosine = 1.0f;
    sampleDirectionDetailed(u, point, direction, distance, uv, uv1, pdf, emitter_cosine);
}

void SceneObject::sampleDirectionDetailed(
    const glm::vec3& u,
    const glm::vec3& point,
    glm::vec3& direction,
    float& distance,
    glm::vec2& uv,
    glm::vec2& uv1,
    float& pdf,
    float& emitter_cosine
) const {
    emitter_cosine = 0.0f;
    glm::vec3 sampled_point;
    glm::vec3 sampled_normal;
    samplePoint(u, sampled_point, sampled_normal, uv, uv1, pdf);
    direction = sampled_point - point;
    float distance_squared = glm::dot(direction, direction);
    if(distance_squared <= 1e-8f || pdf <= 0.0f) {
        distance = 0.0f;
        pdf = 0.0f;
        return;
    }
    distance = glm::sqrt(distance_squared);
    direction *= 1.0f / distance;
    float cos_light = glm::dot(sampled_normal, -direction);
    if(!material || material->emitsDoubleSided()) {
        cos_light = glm::abs(cos_light);
    }
    emitter_cosine = glm::max(0.0f, cos_light);
    pdf = cos_light > 1e-5f ? pdf * distance_squared / cos_light : 0.0f;
}

float SceneObject::computePDF(const Ray& ray, const RayHit& hit) const {
    if(is_identity_transform) {
        if(material && !material->emitsDoubleSided()) {
            SurfaceData surface = shape->getSurfaceData(ray, hit);
            if(surface.hit_direction == HitDirection::EXITING) return 0.0f;
        }
        return shape->computePDF(ray, hit);
    }

    float area = shape->surfaceArea() * area_scale;
    if(area <= 0.0f) return 0.0f;
    if(material && !material->emitsDoubleSided()) {
        SurfaceData surface = getSurfaceData(ray, hit);
        if(surface.hit_direction == HitDirection::EXITING) return 0.0f;
    }
    glm::vec3 normal = getGeometricNormal(ray, hit);
    float cos_light = glm::abs(glm::dot(normal, -ray.direction));
    if(cos_light <= 1e-5f) return 0.0f;
    return (hit.distance * hit.distance) / (area * cos_light);
}

LightEvaluation SceneObject::evaluate(glm::vec3 point) const {
    glm::vec3 direction;
    float distance = 0.0f;
    glm::vec2 uv;
    glm::vec2 uv1;
    float sample_pdf = 0.0f;
    float emitter_cosine = 1.0f;
    sampleDirectionDetailed(glm::vec3(0.5f), point, direction, distance, uv, uv1, sample_pdf, emitter_cosine);
    return {
        .light_vector = direction,
        .distance = distance,
        .radiance = sample_pdf > 0.0f ? material->sampleEmissive(uv, uv1) / sample_pdf : Color(0.0f)
    };
}

LightSample SceneObject::sample(glm::vec3 point, const glm::vec3& u) const {
    glm::vec3 direction;
    float distance = 0.0f;
    glm::vec2 uv;
    glm::vec2 uv1;
    float sample_pdf = 0.0f;
    float emitter_cosine = 1.0f;
    sampleDirectionDetailed(u, point, direction, distance, uv, uv1, sample_pdf, emitter_cosine);
    return {
        .light_vector = direction,
        .distance = distance,
        .radiance = sample_pdf > 0.0f ? material->sampleEmissive(uv, uv1) : Color(0.0f),
        .pdf = sample_pdf,
        .delta = false,
        .emitter_cosine = emitter_cosine
    };
}

float SceneObject::pdf(const Ray& ray, const RayHit& hit) const {
    return computePDF(ray, hit);
}

float SceneObject::power() const {
    if(!material || !shape) return 0.0f;
    return maxChannel(material->getAverageEmissivePower()) * shape->surfaceArea() * area_scale * 3.1415926535f;
}

float SceneObject::estimatePowerAt(glm::vec3 point, glm::vec3 normal) const {
    float dist2 = glm::dot(bounds_center - point, bounds_center - point);
    dist2 = glm::max(dist2, 1e-4f);
    glm::vec3 to_light = glm::normalize(bounds_center - point);
    return power() * glm::max(0.0f, glm::dot(normal, to_light)) / dist2;
}

bool SceneObject::update() {
    if(!is_dirty) return false;
    is_dirty = false;

    is_identity_transform =
        position == glm::vec3(0.0f) &&
        rotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f) &&
        scale == glm::vec3(1.0f);

    if(is_identity_transform) {
        transform = glm::mat4(1.0f);
        transform_inverse = glm::mat4(1.0f);
        area_scale = 1.0f;
    } else {
        transform = glm::translate(glm::mat4(1.0f), position)
            * glm::mat4_cast(rotation)
            * glm::scale(glm::mat4(1.0f), scale);
        transform_inverse = glm::inverse(transform);

        float scale_x = glm::length(glm::vec3(transform[0]));
        float scale_y = glm::length(glm::vec3(transform[1]));
        float scale_z = glm::length(glm::vec3(transform[2]));
        area_scale = glm::max(1e-6f, (scale_x * scale_y + scale_x * scale_z + scale_y * scale_z) / 3.0f);
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
    return true;
}
