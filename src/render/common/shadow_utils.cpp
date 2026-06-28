#include "render/common/shadow_utils.hpp"

#include "scene/materials/materials.hpp"

namespace render::common {

Color computeShadow(const Scene& scene, Ray ray, float max_distance) {
    const float ray_epsilon = 0.01f;
    Color shadow = Color(1.0f);
    while(shadow != Color(0.0f) && max_distance > 0.0f) {
        RayHit hit;
        if(auto object = scene.findClosestHit(ray, hit)) {
            if(hit.distance > max_distance) break;

            float endpoint_tolerance = glm::max(4.0f * ray_epsilon, 0.01f * max_distance);
            if(object->isEmissive() && max_distance - hit.distance <= endpoint_tolerance) {
                break;
            }

            auto material = object->getMaterial();
            if(material && !material->castsShadows()) {
                ray.origin += ray.direction * (hit.distance + ray_epsilon);
                max_distance -= hit.distance + ray_epsilon;
                continue;
            }

            if(auto pbr = std::dynamic_pointer_cast<PBRMaterial>(material)) {
                SurfaceData surface = object->getSurfaceData(ray, hit);
                ColorA color_alpha = pbr->sampleBaseColor(surface);
                shadow *= (1.0f - color_alpha.a);
            } else if(std::dynamic_pointer_cast<SmoothMirrorMaterial>(material)) {
                shadow = Color(0.0f);
            } else if(auto glass = std::dynamic_pointer_cast<SmoothGlassMaterial>(material)) {
                SurfaceData surface = object->getSurfaceData(ray, hit);
                Color color = glass->sampleBaseColor(surface);
                shadow *= color;
            }

            ray.origin += ray.direction * (hit.distance + ray_epsilon);
            max_distance -= hit.distance + ray_epsilon;
        } else {
            break;
        }
    }
    return shadow;
}

} // namespace render::common
