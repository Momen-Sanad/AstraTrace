#include "scene/world/scene.hpp"

#include <SDL3/SDL_log.h>

void Scene::clear() {
    objects.clear();
    lights.clear();
    background_color = Color(0.0f);
    ambient = Color(0.0f);
}

void Scene::update() {
    for(auto& object : objects) {
        object->update();
    }
}

bool Scene::anyHit(const Ray& ray, float max_distance) const {
    for(const auto& object : objects) {
        if(!object->mayIntersect(ray, max_distance)) continue;
        RayHit hit;
        hit.distance = max_distance;
        if(object->intersect(ray, hit) && hit.distance <= max_distance) return true;
    }
    return false;
}

std::shared_ptr<SceneObject> Scene::findClosestHit(const Ray& ray, RayHit& hit) const {
    std::shared_ptr<SceneObject> hit_object = nullptr;
    float closest_distance = std::numeric_limits<float>::max();
    for(const auto& object : objects) {
        if(!object->mayIntersect(ray, closest_distance)) continue;

        RayHit object_hit;
        object_hit.distance = closest_distance;
        if(object->intersect(ray, object_hit)) {
            if(!hit_object || object_hit.distance < closest_distance) {
                hit = object_hit;
                closest_distance = object_hit.distance;
                hit_object = object;
            }
        }
    }
    return hit_object;
}

void Scene::printStats() const {
    SDL_Log("Scene Statistics:");
    SDL_Log("\t- Object Count: %llu", objects.size());
    SDL_Log("\t- Light Count: %llu", lights.size());
}
