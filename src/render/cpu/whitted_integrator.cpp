#include "render/cpu/whitted_integrator.hpp"

#include <array>
#include "render/common/brdf.hpp"
#include "render/common/normal_mapping.hpp"
#include "render/common/shadow_utils.hpp"
#include "scene/materials/materials.hpp"

namespace render::cpu {
namespace {

constexpr std::array<glm::vec3, 8> AREA_LIGHT_PREVIEW_SAMPLES = {
    glm::vec3(0.19f, 0.73f, 0.06f),
    glm::vec3(0.62f, 0.31f, 0.19f),
    glm::vec3(0.38f, 0.87f, 0.31f),
    glm::vec3(0.81f, 0.48f, 0.44f),
    glm::vec3(0.12f, 0.22f, 0.56f),
    glm::vec3(0.55f, 0.64f, 0.69f),
    glm::vec3(0.29f, 0.41f, 0.81f),
    glm::vec3(0.91f, 0.14f, 0.94f),
};
constexpr float INV_PI = 0.31830988618f;

float maxChannel(const Color& color) {
    return glm::max(color.r, glm::max(color.g, color.b));
}

glm::vec3 offsetShadowOrigin(
    const glm::vec3& point,
    const glm::vec3& normal,
    const glm::vec3& direction,
    float ray_epsilon
) {
    glm::vec3 offset_normal = glm::dot(normal, direction) >= 0.0f ? normal : -normal;
    return point + ray_epsilon * direction + (4.0f * ray_epsilon) * offset_normal;
}

Color estimatePBRLightContribution(
    const Scene& scene,
    const glm::vec3& position,
    const glm::vec3& normal,
    const glm::vec3& geometric_normal,
    const glm::vec3& view,
    const Color& albedo,
    const Color& f0,
    float roughness,
    const LightSample& sample,
    float ray_epsilon
) {
    if(maxChannel(sample.radiance) <= 0.0f || sample.pdf <= 0.0f || sample.distance <= 0.0f) {
        return Color(0.0f);
    }

    Color contribution = sample.radiance *
        computeLambertDiffuseAndGGXSpecular(albedo, f0, normal, sample.light_vector, view, roughness) /
        sample.pdf;
    if(!sample.delta) {
        contribution *= INV_PI;
    }
    if(maxChannel(contribution) <= 1.0f / 255.0f) return Color(0.0f);

    Color shadow = render::common::computeShadow(
        scene,
        {offsetShadowOrigin(position, geometric_normal, sample.light_vector, ray_epsilon), sample.light_vector},
        sample.distance - ray_epsilon
    );
    return contribution * shadow;
}

}

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
            glm::vec3 normal = computeGlobalNormal(surface, pbr->sampleNormal(surface));
            ColorA color_alpha = pbr->sampleBaseColor(surface);
            Color color = Color(color_alpha);
            float alpha = color_alpha.a;

            Color mr = pbr->sampleMetalRoughness(surface);
            float metalness = mr.r;
            float roughness = mr.g;
            float occlusion = pbr->sampleOcclusion(surface);

            Color albedo = color * (1.0f - metalness);
            Color F0 = glm::mix(Color(0.04f), color, metalness);
            glm::vec3 view = -ray.direction;
            glm::vec3 geometric_normal = object->getGeometricNormal(ray, hit);

            Color outgoing_radiance = Color(0.0f);
            if(alpha > 0.0f) {
                Color ambient_diffuse = scene.getAmbient() * albedo * occlusion;
                Color ambient_specular = scene.getAmbient()
                    * F0
                    * glm::mix(0.08f, 0.42f, metalness)
                    * (1.0f - 0.45f * glm::clamp(roughness, 0.0f, 1.0f));
                outgoing_radiance = ambient_diffuse + ambient_specular + pbr->sampleEmissive(surface);
                const auto& preview_lights = scene.getPathLights().empty()
                    ? scene.getLights()
                    : scene.getPathLights();
                for(const auto& light : preview_lights) {
                    if(!light) continue;
                    if(!light->isDelta()) {
                        Color light_sum(0.0f);
                        for(const glm::vec3& sample_u : AREA_LIGHT_PREVIEW_SAMPLES) {
                            light_sum += estimatePBRLightContribution(
                                scene,
                                position,
                                normal,
                                geometric_normal,
                                view,
                                albedo,
                                F0,
                                roughness,
                                light->sample(position, sample_u),
                                ray_epsilon
                            );
                        }
                        outgoing_radiance += light_sum / static_cast<float>(AREA_LIGHT_PREVIEW_SAMPLES.size());
                        continue;
                    }

                    LightEvaluation eval = light->evaluate(position);
                    Color light_contribution = eval.radiance * computeLambertDiffuseAndGGXSpecular(
                        albedo, F0, normal, eval.light_vector, view, roughness
                    );
                    if(glm::dot(light_contribution, Color(1.0f)) > (1.0f / 255.0f)) {
                        Color shadow = render::common::computeShadow(
                            scene,
                            {offsetShadowOrigin(position, geometric_normal, eval.light_vector, ray_epsilon), eval.light_vector},
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
            if(depth > 0) {
                glm::vec3 reflected = glm::reflect(ray.direction, normal);
                if(glm::dot(reflected, surface.normal) < 0.0f) reflected = glm::reflect(ray.direction, surface.normal);
                reflected = glm::normalize(reflected);

                Color F = computeFresnelSchlick(normal, view, F0);
                Color reflection_weight = F *
                    glm::mix(0.20f, 1.0f, metalness) *
                    glm::clamp(1.0f - 0.55f * roughness, 0.25f, 1.0f);
                if(maxChannel(reflection_weight) > 1.0f / 255.0f) {
                    Color reflected_radiance = traceRecursive(
                        scene,
                        {
                            offsetShadowOrigin(position, geometric_normal, reflected, ray_epsilon),
                            reflected
                        },
                        depth - 1
                    );
                    outgoing_radiance += reflection_weight * reflected_radiance;
                }
            }
            return outgoing_radiance;
        }

        if(auto glass = std::dynamic_pointer_cast<SmoothGlassMaterial>(material)) {
            glm::vec3 normal = computeGlobalNormal(surface, glass->sampleNormal(surface));
            glm::vec3 geometric_normal = object->getGeometricNormal(ray, hit);
            Color color = glass->sampleBaseColor(surface);
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
                if(depth <= 0) return scene.evaluateEnvironment(reflected);
                return traceRecursive(scene, {position + ray_epsilon * reflected, reflected}, depth - 1);
            }

            Color F = Color(F0);
            if(eta > 1.0f) F = computeFresnelSchlick(-normal, refracted, F);
            else F = computeFresnelSchlick(normal, reflected, F);

            if(depth <= 0) {
                return F * scene.evaluateEnvironment(reflected) +
                    color * (1.0f - F) * scene.evaluateEnvironment(refracted);
            }
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
                        {offsetShadowOrigin(position, geometric_normal, eval.light_vector, ray_epsilon), eval.light_vector},
                        eval.distance - ray_epsilon
                    );
                    surface_preview += light_contribution * shadow;
                }
            }

            return surface_preview + glass_result;
        }

        if(auto mirror = std::dynamic_pointer_cast<SmoothMirrorMaterial>(material)) {
            glm::vec3 normal = computeGlobalNormal(surface, mirror->sampleNormal(surface));
            Color F0 = mirror->sampleBaseColor(surface);
            Color F = computeFresnelSchlick(normal, -ray.direction, F0);

            glm::vec3 reflected = glm::reflect(ray.direction, normal);
            if(glm::dot(reflected, surface.normal) < 0.0f) reflected = glm::reflect(reflected, surface.normal);
            if(depth <= 0) return F * scene.evaluateEnvironment(reflected);
            return F * traceRecursive(scene, {position + ray_epsilon * reflected, reflected}, depth - 1);
        }

        return Color(0.0f);
    }

    return scene.evaluateEnvironment(ray.direction);
}

} // namespace render::cpu
