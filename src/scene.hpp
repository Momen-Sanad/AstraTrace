#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <vector>
#include "shape.hpp"
#include "light.hpp"
#include "material.hpp"

// A scene object defines a drawable object in the scene.
// It contains the shape to be drawn, its transformation and the material used to draw it.
class SceneObject {
public:
    SceneObject(
        std::shared_ptr<Shape> shape, 
        std::shared_ptr<Material> material,
        glm::vec3 position = glm::vec3(0.0f),
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3 scale = glm::vec3(1.0f)
    ) : shape(shape), material(material), position(position), rotation(rotation), scale(scale) {}

    // When changing the position, rotation or scale, we need to mark the object as dirty so that we update its transforms.
    void setPosition(const glm::vec3& position) { if(this->position != position) { this->position = position; markDirty(); } }
    void setRotation(const glm::quat& rotation) { if(this->rotation != rotation) { this->rotation = rotation; markDirty(); } }
    void setScale(const glm::vec3& scale) { if(this->scale != scale) { this->scale = scale; markDirty(); } }
    
    // Check for intersection against the object's shape.
    bool intersect(const Ray& ray, RayHit& hit) const;
    // Fast broad-phase test using a cached world-space bounding sphere.
    bool mayIntersect(const Ray& ray, float max_distance = std::numeric_limits<float>::max()) const;
    // Get the surface data at the hit point.
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const;
    // Get the object's material.
    std::shared_ptr<Material> getMaterial() const { return material; }

    // Mark that this object has been moved, rotated, or scaled and needs to be updated.
    void markDirty() { is_dirty = true; }
    // This will be called by the scene so that we update our transformation matrices too.
    void update();
private:
    // The shape and material of the object.
    std::shared_ptr<Shape> shape;
    std::shared_ptr<Material> material;
    
    // The position, rotation and scale of the object.
    glm::vec3 position = glm::vec3(0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    
    // We cache the transformation matrices so that we don't have to compute them every frame.
    // We also cache the inverse matrix since it is used frequently.
    glm::mat4 transform, transform_inverse;
    glm::vec3 bounds_center = glm::vec3(0.0f);
    float bounds_radius = 0.0f;
    bool is_identity_transform = true;

    // When is_dirty is true, then "transform", "transform_inverse" & "bounds" are out-of-date and need to be recomputed.
    bool is_dirty = true;
};

// A scene defines a collection of objects and a TLAS to accelerate intersections against them.
// It also contains a collection of lights, the background color and the ambient light intensity.
class Scene {
public:
    Scene() = default;

    // Create an object with the given shape, material and transformation, 
    // add it to the scene, then return a pointer to it.
    std::shared_ptr<SceneObject> createObject(
        std::shared_ptr<Shape> shape, 
        std::shared_ptr<Material> material,
        glm::vec3 position = glm::vec3(0.0f),
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3 scale = glm::vec3(1.0f)
    ) {
        auto object = std::make_shared<SceneObject>(
            shape, material, position, rotation, scale
        );
        objects.push_back(object);
        return object;
    }

    // Add a light to the scene.
    void addLight(const std::shared_ptr<Light>& light) {
        lights.push_back(light);
    }
    
    // Some setters.
    void setBackgroundColor(Color color) { background_color = color; }
    void setAmbient(Color color) { ambient = color; }

    // If the TLAS is dirty, we update it and recursively, it updates dirty objects.
    void update();

    // Check if the ray hits any object in the scene.
    bool anyHit(const Ray& ray, float max_distance = std::numeric_limits<float>::max()) const;
    // Finds the closest hit in the scene and returns the hit object.
    std::shared_ptr<SceneObject> findClosestHit(const Ray& ray, RayHit& hit) const;
    // Compute the shadow color for a given shadow ray.
    Color computeShadow(Ray ray, float max_distance = std::numeric_limits<float>::max()) const;

    // Apply Whitted-style ray tracing and return the radiance returning to the camera.
    // max_depth specifies the maximum recursion depth.
    Color rayTrace(const Ray& ray, int max_depth) const;

    // Print some statistics about the scene.
    void printStats();

private:
    std::vector<std::shared_ptr<SceneObject>> objects;
    std::vector<std::shared_ptr<Light>> lights;
    Color background_color, ambient;
};
