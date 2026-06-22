#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>

#include "core/ray.hpp"
#include "io/gltf/gltf_loader.hpp"
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"

namespace {

struct TraversalResult {
    double milliseconds = 0.0;
    std::size_t hit_count = 0;
    double distance_checksum = 0.0;
};

std::shared_ptr<SceneObject> findClosestLinear(const Scene& scene, const Ray& ray, RayHit& hit) {
    std::shared_ptr<SceneObject> hit_object = nullptr;
    float closest_distance = std::numeric_limits<float>::max();

    for(const auto& object : scene.getObjects()) {
        if(!object->mayIntersect(ray, closest_distance)) continue;

        RayHit object_hit;
        object_hit.distance = closest_distance;
        if(object->intersect(ray, object_hit) && object_hit.distance < closest_distance) {
            hit = object_hit;
            closest_distance = object_hit.distance;
            hit_object = object;
        }
    }
    return hit_object;
}

std::vector<Ray> generateRays(const Camera& camera, int ray_count) {
    int side = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(ray_count))));
    std::vector<Ray> rays;
    rays.reserve(static_cast<std::size_t>(ray_count));
    for(int y = 0; y < side && static_cast<int>(rays.size()) < ray_count; ++y) {
        for(int x = 0; x < side && static_cast<int>(rays.size()) < ray_count; ++x) {
            glm::vec2 uv(
                (static_cast<float>(x) + 0.5f) / static_cast<float>(side),
                (static_cast<float>(y) + 0.5f) / static_cast<float>(side)
            );
            rays.push_back(camera.generateRay(uv));
        }
    }
    return rays;
}

template<typename TraceFn>
TraversalResult timeTraversal(const std::vector<Ray>& rays, TraceFn trace) {
    TraversalResult result;
    const auto start = std::chrono::steady_clock::now();
    for(const Ray& ray : rays) {
        RayHit hit;
        std::shared_ptr<SceneObject> object = trace(ray, hit);
        if(object) {
            ++result.hit_count;
            result.distance_checksum += static_cast<double>(hit.distance);
        }
    }
    const auto end = std::chrono::steady_clock::now();
    result.milliseconds = std::chrono::duration<double, std::milli>(end - start).count();
    return result;
}

int parseRayCount(int argc, char** argv) {
    if(argc < 3) return 65536;
    int value = std::atoi(argv[2]);
    return value > 0 ? value : 65536;
}

} // namespace

int main(int argc, char** argv) {
    std::string scene_path = argc > 1 ? argv[1] : "scenes/sponza.glb";
    int ray_count = parseRayCount(argc, argv);

    Scene scene;
    Camera camera;
    camera.setHalfSize(glm::radians(60.0f), 16.0f / 9.0f);

    GltfSceneLoadResult load_result = io::gltf::loadSceneFromGLTF(
        scene_path,
        scene,
        camera,
        16.0f / 9.0f
    );
    if(!load_result.success) {
        std::cerr << "BVH benchmark failed to load scene: " << load_result.error << "\n";
        return 1;
    }

    scene.update();
    SceneStats stats = scene.getStats();
    std::vector<Ray> rays = generateRays(camera, ray_count);

    TraversalResult linear = timeTraversal(rays, [&](const Ray& ray, RayHit& hit) {
        return findClosestLinear(scene, ray, hit);
    });
    TraversalResult tlas = timeTraversal(rays, [&](const Ray& ray, RayHit& hit) {
        return scene.findClosestHit(ray, hit);
    });

    double speedup = tlas.milliseconds > 0.0 ? linear.milliseconds / tlas.milliseconds : 0.0;
    std::cout
        << "BVH Benchmark\n"
        << "Scene: " << scene_path << "\n"
        << "Rays: " << rays.size() << "\n"
        << "Objects: " << stats.object_count << "\n"
        << "TLAS Nodes: " << stats.top_level_bvh_node_count << "\n"
        << "TLAS Leaves: " << stats.top_level_bvh_leaf_count << "\n"
        << "TLAS Max Depth: " << stats.top_level_bvh_max_depth << "\n"
        << "TLAS Build: " << stats.last_top_level_bvh_build_ms << " ms\n"
        << "Linear: " << linear.milliseconds << " ms, hits=" << linear.hit_count << "\n"
        << "TLAS: " << tlas.milliseconds << " ms, hits=" << tlas.hit_count << "\n"
        << "Speedup: " << speedup << "x\n"
        << "Checksums: linear=" << linear.distance_checksum << ", tlas=" << tlas.distance_checksum << "\n";

    return 0;
}
