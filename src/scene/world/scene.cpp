#include "scene/world/scene.hpp"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <limits>
#include <SDL3/SDL_log.h>

namespace {

void expandBounds(AABB& dst, const AABB& src) {
    dst.min = glm::min(dst.min, src.min);
    dst.max = glm::max(dst.max, src.max);
}

bool intersectAABB(const AABB& box, const Ray& ray, float t_min, float t_max, float* t_entry = nullptr) {
    for(int axis = 0; axis < 3; ++axis) {
        const float dir = ray.direction[axis];
        if(std::abs(dir) < 1e-8f) {
            if(ray.origin[axis] < box.min[axis] || ray.origin[axis] > box.max[axis]) {
                return false;
            }
            continue;
        }

        const float inv_d = 1.0f / dir;
        float t0 = (box.min[axis] - ray.origin[axis]) * inv_d;
        float t1 = (box.max[axis] - ray.origin[axis]) * inv_d;
        if(inv_d < 0.0f) std::swap(t0, t1);
        t_min = glm::max(t_min, t0);
        t_max = glm::min(t_max, t1);
        if(t_max < t_min) return false;
    }
    if(t_entry) *t_entry = t_min;
    return true;
}

} // namespace

void Scene::clear() {
    next_object_id = 0;
    objects.clear();
    lights.clear();
    path_lights.clear();
    object_refs.clear();
    top_level_bvh_nodes.clear();
    stats = {};
    background_color = Color(0.0f);
    environment.setConstant(background_color);
    ambient = Color(0.0f);
    top_level_bvh_dirty = true;
    top_level_bvh_refit_enabled = false;
}

void Scene::setBackgroundColor(Color color) {
    background_color = glm::max(color, Color(0.0f));
    environment.setConstant(background_color);
}

void Scene::setEnvironmentImage(std::shared_ptr<Image<Color>> image, float strength) {
    environment.setImage(std::move(image), strength);
    background_color = environment.averageRadiance();
}

void Scene::update() {
    path_lights.clear();
    path_lights.reserve(lights.size() + objects.size());
    for(const auto& light : lights) {
        path_lights.push_back(light);
    }

    bool object_bounds_changed = false;
    for(auto& object : objects) {
        object_bounds_changed = object->update() || object_bounds_changed;
        if(object->isEmissive()) {
            path_lights.push_back(object);
        }
    }

    if(top_level_bvh_dirty || top_level_bvh_nodes.empty() || object_refs.size() != objects.size()) {
        rebuildTopLevelBVH();
    } else if(object_bounds_changed) {
        if(top_level_bvh_refit_enabled) {
            refitTopLevelBVH();
        } else {
            rebuildTopLevelBVH();
        }
    }
}

bool Scene::anyHit(const Ray& ray, float max_distance) const {
    if(top_level_bvh_nodes.empty()) {
        for(const auto& object : objects) {
            if(!object->mayIntersect(ray, max_distance)) continue;
            RayHit hit;
            hit.distance = max_distance;
            if(object->intersect(ray, hit) && hit.distance <= max_distance) return true;
        }
        return false;
    }

    std::vector<int> stack;
    stack.reserve(top_level_bvh_nodes.size());
    stack.push_back(0);

    while(!stack.empty()) {
        int node_index = stack.back();
        stack.pop_back();
        const TopLevelBVHNode& node = top_level_bvh_nodes[static_cast<std::size_t>(node_index)];
        if(!intersectAABB(node.bounds, ray, 1e-5f, max_distance)) continue;

        if(node.isLeaf()) {
            for(int i = 0; i < node.count; ++i) {
                const ObjectRef& ref = object_refs[static_cast<std::size_t>(node.start + i)];
                const auto& object = objects[ref.object_index];
                if(!object->mayIntersect(ray, max_distance)) continue;

                RayHit hit;
                hit.distance = max_distance;
                if(object->intersect(ray, hit) && hit.distance <= max_distance) return true;
            }
            continue;
        }

        float left_t = std::numeric_limits<float>::max();
        float right_t = std::numeric_limits<float>::max();
        bool hit_left = node.left >= 0 && intersectAABB(
            top_level_bvh_nodes[static_cast<std::size_t>(node.left)].bounds,
            ray,
            1e-5f,
            max_distance,
            &left_t
        );
        bool hit_right = node.right >= 0 && intersectAABB(
            top_level_bvh_nodes[static_cast<std::size_t>(node.right)].bounds,
            ray,
            1e-5f,
            max_distance,
            &right_t
        );
        if(hit_left && hit_right) {
            if(left_t < right_t) {
                stack.push_back(node.right);
                stack.push_back(node.left);
            } else {
                stack.push_back(node.left);
                stack.push_back(node.right);
            }
        } else if(hit_left) {
            stack.push_back(node.left);
        } else if(hit_right) {
            stack.push_back(node.right);
        }
    }

    return false;
}

std::shared_ptr<SceneObject> Scene::findClosestHit(const Ray& ray, RayHit& hit) const {
    std::shared_ptr<SceneObject> hit_object = nullptr;
    float closest_distance = std::numeric_limits<float>::max();

    if(top_level_bvh_nodes.empty()) {
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

    std::vector<int> stack;
    stack.reserve(top_level_bvh_nodes.size());
    stack.push_back(0);

    while(!stack.empty()) {
        int node_index = stack.back();
        stack.pop_back();
        const TopLevelBVHNode& node = top_level_bvh_nodes[static_cast<std::size_t>(node_index)];
        if(!intersectAABB(node.bounds, ray, 1e-5f, closest_distance)) continue;

        if(node.isLeaf()) {
            for(int i = 0; i < node.count; ++i) {
                const ObjectRef& ref = object_refs[static_cast<std::size_t>(node.start + i)];
                const auto& object = objects[ref.object_index];
                if(!object->mayIntersect(ray, closest_distance)) continue;

                RayHit object_hit;
                object_hit.distance = closest_distance;
                if(object->intersect(ray, object_hit) && object_hit.distance < closest_distance) {
                    hit = object_hit;
                    closest_distance = object_hit.distance;
                    hit_object = object;
                }
            }
            continue;
        }

        float left_t = std::numeric_limits<float>::max();
        float right_t = std::numeric_limits<float>::max();
        bool hit_left = node.left >= 0 && intersectAABB(
            top_level_bvh_nodes[static_cast<std::size_t>(node.left)].bounds,
            ray,
            1e-5f,
            closest_distance,
            &left_t
        );
        bool hit_right = node.right >= 0 && intersectAABB(
            top_level_bvh_nodes[static_cast<std::size_t>(node.right)].bounds,
            ray,
            1e-5f,
            closest_distance,
            &right_t
        );
        if(hit_left && hit_right) {
            if(left_t < right_t) {
                stack.push_back(node.right);
                stack.push_back(node.left);
            } else {
                stack.push_back(node.left);
                stack.push_back(node.right);
            }
        } else if(hit_left) {
            stack.push_back(node.left);
        } else if(hit_right) {
            stack.push_back(node.right);
        }
    }

    return hit_object;
}

SceneStats Scene::getStats() const {
    SceneStats snapshot = stats;
    snapshot.object_count = objects.size();
    snapshot.punctual_light_count = lights.size();
    snapshot.path_light_count = path_lights.size();
    snapshot.top_level_bvh_node_count = top_level_bvh_nodes.size();
    return snapshot;
}

void Scene::printStats() const {
    SceneStats snapshot = getStats();
    SDL_Log("Scene Statistics:");
    SDL_Log("\t- Object Count: %llu", static_cast<unsigned long long>(snapshot.object_count));
    SDL_Log("\t- Light Count: %llu", static_cast<unsigned long long>(snapshot.punctual_light_count));
    SDL_Log("\t- Path Light Count: %llu", static_cast<unsigned long long>(snapshot.path_light_count));
    SDL_Log("\t- Top-Level BVH Nodes: %llu", static_cast<unsigned long long>(snapshot.top_level_bvh_node_count));
    SDL_Log("\t- Top-Level BVH Leaves: %llu", static_cast<unsigned long long>(snapshot.top_level_bvh_leaf_count));
    SDL_Log("\t- Top-Level BVH Max Depth: %d", snapshot.top_level_bvh_max_depth);
    SDL_Log("\t- Top-Level BVH Rebuilds: %llu", static_cast<unsigned long long>(snapshot.top_level_bvh_rebuild_count));
    SDL_Log("\t- Top-Level BVH Refits: %llu", static_cast<unsigned long long>(snapshot.top_level_bvh_refit_count));
    SDL_Log("\t- Last Top-Level BVH Build: %.3f ms", snapshot.last_top_level_bvh_build_ms);
    SDL_Log("\t- Last Top-Level BVH Refit: %.3f ms", snapshot.last_top_level_bvh_refit_ms);
}

void Scene::rebuildTopLevelBVH() {
    const auto start_time = std::chrono::steady_clock::now();
    object_refs.clear();
    top_level_bvh_nodes.clear();
    top_level_bvh_dirty = false;
    stats.top_level_bvh_leaf_count = 0;
    stats.top_level_bvh_max_depth = 0;

    if(objects.empty()) {
        const auto end_time = std::chrono::steady_clock::now();
        stats.last_top_level_bvh_build_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        ++stats.top_level_bvh_rebuild_count;
        return;
    }

    object_refs.reserve(objects.size());
    for(std::size_t i = 0; i < objects.size(); ++i) {
        AABB bounds = objects[i]->getBounds();
        object_refs.push_back({
            .object_index = i,
            .bounds = bounds,
            .centroid = 0.5f * (bounds.min + bounds.max)
        });
    }

    top_level_bvh_nodes.reserve(object_refs.size() * 2);
    buildTopLevelBVHNode(0, static_cast<int>(object_refs.size()), 1);
    const auto end_time = std::chrono::steady_clock::now();
    stats.last_top_level_bvh_build_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    ++stats.top_level_bvh_rebuild_count;
}

void Scene::refitTopLevelBVH() {
    if(top_level_bvh_nodes.empty() || object_refs.size() != objects.size()) {
        rebuildTopLevelBVH();
        return;
    }

    const auto start_time = std::chrono::steady_clock::now();
    for(ObjectRef& ref : object_refs) {
        if(ref.object_index >= objects.size()) {
            rebuildTopLevelBVH();
            return;
        }
        ref.bounds = objects[ref.object_index]->getBounds();
        ref.centroid = 0.5f * (ref.bounds.min + ref.bounds.max);
    }

    refitTopLevelBVHNode(0);
    const auto end_time = std::chrono::steady_clock::now();
    stats.last_top_level_bvh_refit_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    ++stats.top_level_bvh_refit_count;
}

AABB Scene::refitTopLevelBVHNode(int node_index) {
    TopLevelBVHNode& node = top_level_bvh_nodes[static_cast<std::size_t>(node_index)];
    AABB bounds;
    bounds.min = glm::vec3(std::numeric_limits<float>::max());
    bounds.max = glm::vec3(std::numeric_limits<float>::lowest());

    if(node.isLeaf()) {
        for(int i = 0; i < node.count; ++i) {
            expandBounds(bounds, object_refs[static_cast<std::size_t>(node.start + i)].bounds);
        }
    } else {
        if(node.left >= 0) expandBounds(bounds, refitTopLevelBVHNode(node.left));
        if(node.right >= 0) expandBounds(bounds, refitTopLevelBVHNode(node.right));
    }

    node.bounds = bounds;
    return bounds;
}

int Scene::buildTopLevelBVHNode(int start, int count, int depth) {
    TopLevelBVHNode node;
    node.bounds.min = glm::vec3(std::numeric_limits<float>::max());
    node.bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for(int i = 0; i < count; ++i) {
        expandBounds(node.bounds, object_refs[static_cast<std::size_t>(start + i)].bounds);
    }

    int node_index = static_cast<int>(top_level_bvh_nodes.size());
    top_level_bvh_nodes.push_back(node);

    if(count <= 4) {
        top_level_bvh_nodes[static_cast<std::size_t>(node_index)].start = start;
        top_level_bvh_nodes[static_cast<std::size_t>(node_index)].count = count;
        ++stats.top_level_bvh_leaf_count;
        stats.top_level_bvh_max_depth = glm::max(stats.top_level_bvh_max_depth, depth);
        return node_index;
    }

    AABB centroid_bounds;
    centroid_bounds.min = glm::vec3(std::numeric_limits<float>::max());
    centroid_bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for(int i = 0; i < count; ++i) {
        const glm::vec3& centroid = object_refs[static_cast<std::size_t>(start + i)].centroid;
        centroid_bounds.min = glm::min(centroid_bounds.min, centroid);
        centroid_bounds.max = glm::max(centroid_bounds.max, centroid);
    }

    const glm::vec3 extent = centroid_bounds.max - centroid_bounds.min;
    int axis = 0;
    if(extent.y > extent.x && extent.y > extent.z) axis = 1;
    else if(extent.z > extent.x) axis = 2;

    const int mid = start + count / 2;
    std::nth_element(
        object_refs.begin() + start,
        object_refs.begin() + mid,
        object_refs.begin() + start + count,
        [axis](const ObjectRef& a, const ObjectRef& b) {
            return a.centroid[axis] < b.centroid[axis];
        }
    );

    int left = buildTopLevelBVHNode(start, mid - start, depth + 1);
    int right = buildTopLevelBVHNode(mid, count - (mid - start), depth + 1);
    top_level_bvh_nodes[static_cast<std::size_t>(node_index)].left = left;
    top_level_bvh_nodes[static_cast<std::size_t>(node_index)].right = right;
    return node_index;
}
