#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <memory>
#include <vector>
#include "scene/world/scene_object.hpp"
#include "scene/lights/lights.hpp"

class Scene {
public:
    Scene() = default;

    std::shared_ptr<SceneObject> createObject(
        std::shared_ptr<Shape> shape,
        std::shared_ptr<Material> material,
        glm::vec3 position = glm::vec3(0.0f),
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        glm::vec3 scale = glm::vec3(1.0f)
    ) {
        auto object = std::make_shared<SceneObject>(shape, material, position, rotation, scale);
        objects.push_back(object);
        return object;
    }

    void addLight(const std::shared_ptr<Light>& light) { lights.push_back(light); }

    void setBackgroundColor(Color color) { background_color = color; }
    void setAmbient(Color color) { ambient = color; }

    Color getBackgroundColor() const { return background_color; }
    Color getAmbient() const { return ambient; }
    const std::vector<std::shared_ptr<Light>>& getLights() const { return lights; }

    void update();
    bool anyHit(const Ray& ray, float max_distance = std::numeric_limits<float>::max()) const;
    std::shared_ptr<SceneObject> findClosestHit(const Ray& ray, RayHit& hit) const;
    void printStats() const;

private:
    std::vector<std::shared_ptr<SceneObject>> objects;
    std::vector<std::shared_ptr<Light>> lights;
    Color background_color = Color(0.0f);
    Color ambient = Color(0.0f);
};
