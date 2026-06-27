#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "scene/world/scene_object.hpp"
#include "scene/world/environment.hpp"
#include "scene/lights/lights.hpp"

struct SceneStats {
    std::size_t object_count = 0;
    std::size_t punctual_light_count = 0;
    std::size_t path_light_count = 0;
    std::size_t top_level_bvh_node_count = 0;
    std::size_t top_level_bvh_leaf_count = 0;
    int top_level_bvh_max_depth = 0;
    std::uint64_t top_level_bvh_rebuild_count = 0;
    std::uint64_t top_level_bvh_refit_count = 0;
    double last_top_level_bvh_build_ms = 0.0;
    double last_top_level_bvh_refit_ms = 0.0;
};

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
        auto object = std::make_shared<SceneObject>(++next_object_id, shape, material, position, rotation, scale);
        objects.push_back(object);
        top_level_bvh_dirty = true;
        return object;
    }

    void addLight(const std::shared_ptr<Light>& light) { lights.push_back(light); }
    void clear();

    void setBackgroundColor(Color color);
    void setEnvironmentImage(std::shared_ptr<Image<Color>> image, float strength = 1.0f);
    void setAmbient(Color color) { ambient = color; }
    void setTopLevelBVHRefitEnabled(bool enabled) { top_level_bvh_refit_enabled = enabled; }

    Color getBackgroundColor() const { return background_color; }
    Color evaluateEnvironment(glm::vec3 direction) const { return environment.evaluate(direction); }
    EnvironmentSample sampleEnvironment(glm::vec3 u) const { return environment.sample(u); }
    float environmentPdf(glm::vec3 direction) const { return environment.pdf(direction); }
    bool hasImageEnvironment() const { return environment.hasImage(); }
    Color getAmbient() const { return ambient; }
    const std::vector<std::shared_ptr<Light>>& getLights() const { return lights; }
    const std::vector<std::shared_ptr<Light>>& getPathLights() const { return path_lights; }
    const std::vector<std::shared_ptr<SceneObject>>& getObjects() const { return objects; }

    void update();
    bool anyHit(const Ray& ray, float max_distance = std::numeric_limits<float>::max()) const;
    std::shared_ptr<SceneObject> findClosestHit(const Ray& ray, RayHit& hit) const;
    SceneStats getStats() const;
    void printStats() const;

private:
    struct ObjectRef {
        std::size_t object_index = 0;
        AABB bounds;
        glm::vec3 centroid = glm::vec3(0.0f);
    };

    struct TopLevelBVHNode {
        AABB bounds;
        int left = -1;
        int right = -1;
        int start = 0;
        int count = 0;
        bool isLeaf() const { return left < 0 && right < 0; }
    };

    void rebuildTopLevelBVH();
    void refitTopLevelBVH();
    AABB refitTopLevelBVHNode(int node_index);
    int buildTopLevelBVHNode(int start, int count, int depth);

    ObjectID next_object_id = 0;
    std::vector<std::shared_ptr<SceneObject>> objects;
    std::vector<std::shared_ptr<Light>> lights;
    std::vector<std::shared_ptr<Light>> path_lights;
    std::vector<ObjectRef> object_refs;
    std::vector<TopLevelBVHNode> top_level_bvh_nodes;
    SceneStats stats;
    Color background_color = Color(0.0f);
    EnvironmentMap environment;
    Color ambient = Color(0.0f);
    bool top_level_bvh_dirty = true;
    bool top_level_bvh_refit_enabled = false;
};
