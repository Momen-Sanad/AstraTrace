#include "render/cpu/whitted_integrator.hpp"

#include "render/common/brdf.hpp"
#include "render/common/normal_mapping.hpp"
#include "render/common/shadow_utils.hpp"
#include "scene/materials/materials.hpp"

namespace render::cpu {

Color WhittedIntegrator::trace(const Scene& scene, const Ray& ray) const {
    return traceRecursive(scene, ray, max_depth);
}

Color WhittedIntegrator::traceRecursive(const Scene& scene, const Ray& ray, int depth) const {
    const float ray_epsilon = 0.01f;
    RayHit hit;
    if(auto object = scene.findClosestHit(ray, hit)) {
        glm::vec3 position = ray.origin + ray.direction * hit.distance;
        auto material = object->getMaterial();
        SurfaceData surface = object->getSurfaceData(ray, hit);

        if(auto pbr = std::dynamic_pointer_cast<PBRMaterial>(material)) {
            glm::vec3 normal = computeGlobalNormal(surface, pbr->sampleNormal(surface.uv));
            ColorA color_alpha = pbr->sampleBaseColor(surface.uv);
            Color color = Color(color_alpha);
            float alpha = color_alpha.a;

            Color mr = pbr->sampleMetalRoughness(surface.uv);
            float metalness = mr.r;
            float roughness = mr.g;
            float occlusion = pbr->sampleOcclusion(surface.uv);

            Color albedo = color * (1.0f - metalness);
            Color F0 = glm::mix(Color(0.04f), color, metalness);
            glm::vec3 view = -ray.direction;

            Color outgoing_radiance = Color(0.0f);
            if(alpha > 0.0f) {
                Color ambient_diffuse = scene.getAmbient() * albedo * occlusion;
                Color ambient_specular = scene.getAmbient()
                    * F0
                    * glm::mix(0.08f, 0.42f, metalness)
                    * (1.0f - 0.45f * glm::clamp(roughness, 0.0f, 1.0f));
                outgoing_radiance = ambient_diffuse + ambient_specular + pbr->sampleEmissive(surface.uv);
                const auto& preview_lights = scene.getPathLights().empty()
                    ? scene.getLights()
                    : scene.getPathLights();
                for(const auto& light : preview_lights) {
                    LightEvaluation eval = light->evaluate(position);
                    Color light_contribution = eval.radiance * computeLambertDiffuseAndGGXSpecular(
                        albedo, F0, normal, eval.light_vector, view, roughness
                    );
                    if(glm::dot(light_contribution, Color(1.0f)) > (1.0f / 255.0f)) {
                        Color shadow = render::common::computeShadow(
                            scene,
                            {position + ray_epsilon * eval.light_vector, eval.light_vector},
                            eval.distance - ray_epsilon
                        );
                        outgoing_radiance += light_contribution * shadow;
                    }
                }
            }

            if(alpha < 1.0f && depth > 0) {
                return glm::mix(
                    traceRecursive(scene, {position + ray_epsilon * ray.direction, ray.direction}, depth - 1),
                    outgoing_radiance,
                    alpha
                );
            }
            return outgoing_radiance;
        }

        if(auto glass = std::dynamic_pointer_cast<SmoothGlassMaterial>(material)) {
            glm::vec3 normal = computeGlobalNormal(surface, glass->sampleNormal(surface.uv));
            Color color = glass->sampleBaseColor(surface.uv);
            float surface_detail = glm::clamp(glass->surface_detail_strength, 0.0f, 1.0f);
            float eta =
                surface.hit_direction == HitDirection::ENTERING ? 1.0f / glass->refractive_index :
                surface.hit_direction == HitDirection::EXITING ? glass->refractive_index :
                1.0f;

            float F0 = (glass->refractive_index - 1) / (glass->refractive_index + 1);
            F0 = F0 * F0;

            glm::vec3 reflected = glm::reflect(ray.direction, normal);
            if(glm::dot(reflected, surface.normal) < 0.0f) reflected = glm::reflect(reflected, surface.normal);
            glm::vec3 refracted = glm::refract(ray.direction, normal, eta);

            if(refracted == glm::vec3(0.0f)) {
                if(depth <= 0) return scene.getBackgroundColor();
                return traceRecursive(scene, {position + ray_epsilon * reflected, reflected}, depth - 1);
            }

            Color F = Color(F0);
            if(eta > 1.0f) F = computeFresnelSchlick(-normal, refracted, F);
            else F = computeFresnelSchlick(normal, reflected, F);

            if(depth <= 0) return (color * (1.0f - F) + F) * scene.getBackgroundColor();
            Color reflection_result = traceRecursive(scene, {position + ray_epsilon * reflected, reflected}, depth - 1);
            Color refraction_result = traceRecursive(scene, {position + ray_epsilon * refracted, refracted}, depth - 1);
            float clear_weight = surface_detail > 0.0f
                ? glm::clamp(1.0f - 0.70f * surface_detail, 0.25f, 1.0f)
                : 1.0f;
            Color glass_result = clear_weight * (F * reflection_result + color * (1.0f - F) * refraction_result);
            if(surface_detail <= 0.0f) return glass_result;

            Color surface_preview = scene.getAmbient() * color * surface_detail;
            const auto& preview_lights = scene.getPathLights().empty()
                ? scene.getLights()
                : scene.getPathLights();
            for(const auto& light : preview_lights) {
                LightEvaluation eval = light->evaluate(position);
                float n_dot_l = glm::max(0.0f, glm::dot(normal, eval.light_vector));
                if(n_dot_l <= 0.0f) continue;

                Color light_contribution = eval.radiance * color * (surface_detail * n_dot_l);
                if(glm::dot(light_contribution, Color(1.0f)) > (1.0f / 255.0f)) {
                    Color shadow = render::common::computeShadow(
                        scene,
                        {position + ray_epsilon * eval.light_vector, eval.light_vector},
                        eval.distance - ray_epsilon
                    );
                    surface_preview += light_contribution * shadow;
                }
            }

            return surface_preview + glass_result;
        }

        if(auto mirror = std::dynamic_pointer_cast<SmoothMirrorMaterial>(material)) {
            glm::vec3 normal = computeGlobalNormal(surface, mirror->sampleNormal(surface.uv));
            Color F0 = mirror->sampleBaseColor(surface.uv);
            Color F = computeFresnelSchlick(normal, -ray.direction, F0);
            if(depth <= 0) return F * scene.getBackgroundColor();

            glm::vec3 reflected = glm::reflect(ray.direction, normal);
            if(glm::dot(reflected, surface.normal) < 0.0f) reflected = glm::reflect(reflected, surface.normal);
            return F * traceRecursive(scene, {position + ray_epsilon * reflected, reflected}, depth - 1);
        }

        return Color(0.0f);
    }

    return scene.getBackgroundColor();
}

} // namespace render::cpu
