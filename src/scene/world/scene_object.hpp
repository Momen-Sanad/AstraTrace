#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cstdint>
#include <limits>
#include <memory>
#include "scene/geometry/geometry.hpp"
#include "scene/lights/light_base.hpp"
#include "scene/materials/materials.hpp"

using ObjectID = uint32_t;

class SceneObject : public Light {
public:
    SceneObject(
        ObjectID id,
        std::shared_ptr<Shape> shape,
        std::shared_ptr<Material> material,
        glm::vec3 position = glm::vec3(0.0f),
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3 scale = glm::vec3(1.0f)
    )
        : id(id), shape(shape), material(material), position(position), rotation(rotation), scale(scale) {}

    void setPosition(const glm::vec3& value) { if(position != value) { position = value; markDirty(); } }
    void setRotation(const glm::quat& value) { if(rotation != value) { rotation = value; markDirty(); } }
    void setScale(const glm::vec3& value) { if(scale != value) { scale = value; markDirty(); } }

    bool intersect(const Ray& ray, RayHit& hit) const;
    bool mayIntersect(const Ray& ray, float max_distance = std::numeric_limits<float>::max()) const;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const;
    glm::vec3 getGeometricNormal(const Ray& ray, const RayHit& hit) const;
    std::shared_ptr<Material> getMaterial() const { return material; }
    ObjectID getID() const { return id; }
    AABB getBounds() const { return world_bounds; }
    bool isEmissive() const;

    void samplePoint(
        const glm::vec3& u,
        glm::vec3& point,
        glm::vec3& normal,
        glm::vec2& uv,
        float& pdf
    ) const;
    void sampleDirection(
        const glm::vec3& u,
        const glm::vec3& point,
        glm::vec3& direction,
        float& distance,
        glm::vec2& uv,
        float& pdf
    ) const;
    float computePDF(const Ray& ray, const RayHit& hit) const;

    LightEvaluation evaluate(glm::vec3 point) const override;
    LightSample sample(glm::vec3 point, const glm::vec3& u) const override;
    float pdf(const Ray& ray, const RayHit& hit) const override;
    float power() const override;
    float estimatePowerAt(glm::vec3 point, glm::vec3 normal) const override;
    bool isDelta() const override { return false; }

    void markDirty() { is_dirty = true; }
    void update();

private:
    ObjectID id = 0;
    std::shared_ptr<Shape> shape;
    std::shared_ptr<Material> material;
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::mat4 transform;
    glm::mat4 transform_inverse;
    AABB world_bounds;
    glm::vec3 bounds_center = glm::vec3(0.0f);
    float bounds_radius = 0.0f;
    bool is_identity_transform = true;
    bool is_dirty = true;
};
