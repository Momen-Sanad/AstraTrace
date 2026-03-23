#pragma once

#include <glm/glm.hpp>
#include "core/ray.hpp"

enum struct HitDirection {
    NONE,
    ENTERING,
    EXITING,
};

struct SurfaceData {
    HitDirection hit_direction;
    glm::vec3 normal;
    glm::vec3 tangent;
    glm::vec3 bitangent;
    glm::vec2 uv;
};

struct AABB {
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
};

class Shape {
public:
    virtual ~Shape() = default;
    virtual bool intersect(const Ray& ray, RayHit& hit) const = 0;
    virtual SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const = 0;
    virtual AABB getBounds() const = 0;
};
