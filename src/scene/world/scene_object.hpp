#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <memory>
#include "scene/geometry/geometry.hpp"
#include "scene/materials/materials.hpp"

class SceneObject {
public:
    SceneObject(
        std::shared_ptr<Shape> shape,
        std::shared_ptr<Material> material,
        glm::vec3 position = glm::vec3(0.0f),
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3 scale = glm::vec3(1.0f)
    )
        : shape(shape), material(material), position(position), rotation(rotation), scale(scale) {}

    void setPosition(const glm::vec3& value) { if(position != value) { position = value; markDirty(); } }
    void setRotation(const glm::quat& value) { if(rotation != value) { rotation = value; markDirty(); } }
    void setScale(const glm::vec3& value) { if(scale != value) { scale = value; markDirty(); } }

    bool intersect(const Ray& ray, RayHit& hit) const;
    bool mayIntersect(const Ray& ray, float max_distance = std::numeric_limits<float>::max()) const;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const;
    std::shared_ptr<Material> getMaterial() const { return material; }

    void markDirty() { is_dirty = true; }
    void update();

private:
    std::shared_ptr<Shape> shape;
    std::shared_ptr<Material> material;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 transform;
    glm::mat4 transform_inverse;
    glm::vec3 bounds_center = glm::vec3(0.0f);
    float bounds_radius = 0.0f;
    bool is_identity_transform = true;
    bool is_dirty = true;
};
