#include "render/cpu/path_integrator.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <vector>
#include <glm/gtc/constants.hpp>
#include "render/common/shadow_utils.hpp"
#include "render/cpu/alias_table.hpp"
#include "render/cpu/sampler.hpp"

namespace render::cpu {
namespace {

constexpr float RAY_EPSILON = 0.002f;

float maxChannel(const Color& color) {
    return glm::max(color.r, glm::max(color.g, color.b));
}

Color safeColor(Color color) {
    if(!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b)) return Color(0.0f);
    return glm::max(color, Color(0.0f));
}

glm::vec3 makeTangent(const glm::vec3& normal) {
    glm::vec3 up = glm::abs(normal.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
    return glm::normalize(glm::cross(up, normal));
}

glm::vec3 uniformHemisphere(const glm::vec2& u, const glm::vec3& normal) {
    float z = glm::clamp(u.x, 0.0f, 1.0f);
    float r = glm::sqrt(glm::max(0.0f, 1.0f - z * z));
    float phi = glm::two_pi<float>() * glm::clamp(u.y, 0.0f, 1.0f);
    glm::vec3 tangent = makeTangent(normal);
    glm::vec3 bitangent = glm::cross(normal, tangent);
    return glm::normalize(r * glm::cos(phi) * tangent + r * glm::sin(phi) * bitangent + z * normal);
}

float lightWeight(
    const std::shared_ptr<Light>& light,
    const glm::vec3& point,
    const glm::vec3& normal,
    PathLightSamplerMode mode
) {
    if(!light) return 0.0f;
    switch(mode) {
    case PathLightSamplerMode::Power:
        return glm::max(0.0f, light->power());
    case PathLightSamplerMode::PartialBRDF:
    case PathLightSamplerMode::Tree:
        return glm::max(0.0f, light->estimatePowerAt(point, normal));
    case PathLightSamplerMode::Uniform:
    default:
        return 1.0f;
    }
}

std::shared_ptr<Light> sampleLight(
    const Scene& scene,
    Sampler& sampler,
    const glm::vec3& point,
    const glm::vec3& normal,
    PathLightSamplerMode mode,
    float& probability
) {
    const auto& lights = scene.getPathLights();
    probability = 0.0f;
    if(lights.empty()) return nullptr;

    if(mode == PathLightSamplerMode::Uniform) {
        std::size_t index = glm::min(
            static_cast<std::size_t>(sampler.next() * lights.size()),
            lights.size() - 1
        );
        probability = 1.0f / static_cast<float>(lights.size());
        return lights[index];
    }

    std::vector<float> weights(lights.size(), 0.0f);
    for(std::size_t i = 0; i < lights.size(); ++i) {
        weights[i] = lightWeight(lights[i], point, normal, mode);
    }

    AliasTable table(weights);
    if(table.empty()) return nullptr;

    std::size_t index = table.sample(sampler.next(), sampler.next(), probability);
    if(probability > 0.0f) {
        return lights[index];
    }

    if(probability <= 0.0f) {
        std::size_t index = glm::min(
            static_cast<std::size_t>(sampler.next() * lights.size()),
            lights.size() - 1
        );
        probability = 1.0f / static_cast<float>(lights.size());
        return lights[index];
    }

    return nullptr;
}

float lightProbability(
    const Scene& scene,
    const std::shared_ptr<Light>& target,
    const glm::vec3& point,
    const glm::vec3& normal,
    PathLightSamplerMode mode
) {
    const auto& lights = scene.getPathLights();
    if(!target || lights.empty()) return 0.0f;
    if(mode == PathLightSamplerMode::Uniform) {
        return 1.0f / static_cast<float>(lights.size());
    }

    float total_weight = 0.0f;
    float target_weight = 0.0f;
    for(const auto& light : lights) {
        float weight = lightWeight(light, point, normal, mode);
        total_weight += weight;
        if(light.get() == target.get()) target_weight = weight;
    }
    if(total_weight <= 0.0f) return 1.0f / static_cast<float>(lights.size());
    return target_weight / total_weight;
}

void addContribution(
    PathResult& result,
    Color radiance,
    LobeType lobe,
    bool first_bounce,
    bool direct
) {
    radiance = safeColor(radiance);
    if(maxChannel(radiance) <= 0.0f) return;

    if(lobe == LobeType::Specular || lobe == LobeType::Transmission) {
        result.specular += radiance;
        return;
    }
    if(first_bounce && direct) {
        result.direct_diffuse += radiance;
    } else {
        result.indirect_diffuse += radiance;
    }
}

Color directLighting(
    const Scene& scene,
    const glm::vec3& point,
    const glm::vec3& normal,
    const glm::vec3& view,
    const BSDF& bsdf,
    Sampler& sampler,
    const PathRenderSettings& settings
) {
    float light_pick_pdf = 0.0f;
    std::shared_ptr<Light> light = sampleLight(
        scene,
        sampler,
        point,
        normal,
        settings.light_sampler,
        light_pick_pdf
    );
    if(!light || light_pick_pdf <= 0.0f) return Color(0.0f);

    LightSample sample = light->sample(point, sampler.next3());
    if(maxChannel(sample.radiance) <= 0.0f) return Color(0.0f);
    if(sample.distance <= 0.0f && !sample.delta) return Color(0.0f);

    DiffuseSpecular bsdf_value = bsdf.evaluate(sample.light_vector, view);
    Color f = bsdf_value.sum();
    if(maxChannel(f) <= 0.0f) return Color(0.0f);

    float light_pdf = sample.delta ? light_pick_pdf : light_pick_pdf * sample.pdf;
    if(light_pdf <= 0.0f && !sample.delta) return Color(0.0f);

    float max_distance = sample.delta ? sample.distance : sample.distance - RAY_EPSILON;
    Color visibility = render::common::computeShadow(
        scene,
        {point + RAY_EPSILON * sample.light_vector, sample.light_vector},
        max_distance
    );
    if(maxChannel(visibility) <= 0.0f) return Color(0.0f);

    float mis_weight = 1.0f;
    if(settings.enable_mis && !sample.delta) {
        float bsdf_pdf = bsdf.pdf(sample.light_vector, view);
        float lp = light_pdf * light_pdf;
        float bp = bsdf_pdf * bsdf_pdf;
        mis_weight = lp / glm::max(1e-6f, lp + bp);
    }

    if(sample.delta) {
        return visibility * sample.radiance * f / light_pick_pdf;
    }
    return visibility * sample.radiance * f * (mis_weight / light_pdf);
}

Color directSkyLighting(
    const Scene& scene,
    const glm::vec3& point,
    const glm::vec3& normal,
    const glm::vec3& view,
    const BSDF& bsdf,
    Sampler& sampler,
    const PathRenderSettings& settings
) {
    Color sky = scene.getBackgroundColor();
    if(maxChannel(sky) <= 0.0f) return Color(0.0f);

    glm::vec3 direction = uniformHemisphere(sampler.next2(), normal);
    constexpr float sky_pdf = 1.0f / (2.0f * glm::pi<float>());
    DiffuseSpecular bsdf_value = bsdf.evaluate(direction, view);
    Color f = bsdf_value.sum();
    if(maxChannel(f) <= 0.0f) return Color(0.0f);

    Color visibility = render::common::computeShadow(
        scene,
        {point + RAY_EPSILON * direction, direction},
        std::numeric_limits<float>::max()
    );
    if(maxChannel(visibility) <= 0.0f) return Color(0.0f);

    float mis_weight = 1.0f;
    if(settings.enable_mis) {
        float bsdf_pdf = bsdf.pdf(direction, view);
        float sp = sky_pdf * sky_pdf;
        float bp = bsdf_pdf * bsdf_pdf;
        mis_weight = sp / glm::max(1e-6f, sp + bp);
    }
    return visibility * sky * f * (mis_weight / sky_pdf);
}

} // namespace

PathResult PathIntegrator::trace(
    const Scene& scene,
    const Camera& camera,
    glm::ivec2 pixel,
    glm::vec2 jitter,
    glm::ivec2 resolution,
    uint32_t sample_index,
    const PathRenderSettings& settings
) const {
    PathResult result;
    Sampler sampler(settings.sampler, pixel, sample_index);
    glm::vec2 uv = (glm::vec2(pixel) + jitter) / glm::vec2(resolution);
    Ray ray = camera.generateRay(uv);

    Color throughput(1.0f);
    LobeType previous_lobe = LobeType::Diffuse;
    glm::vec3 previous_path_point = ray.origin;
    glm::vec3 previous_path_normal = camera.getForward();
    float previous_bsdf_pdf = 0.0f;
    bool has_primary = false;
    bool any_non_delta = false;

    for(int bounce = 0; bounce < settings.max_bounces; ++bounce) {
        RayHit hit;
        std::shared_ptr<SceneObject> object = scene.findClosestHit(ray, hit);
        if(!object) {
            Color sky = scene.getBackgroundColor();
            float mis_weight = 1.0f;
            if(
                settings.enable_nee &&
                settings.enable_mis &&
                bounce > 0 &&
                previous_lobe != LobeType::Specular &&
                previous_lobe != LobeType::Transmission
            ) {
                constexpr float sky_pdf = 1.0f / (2.0f * glm::pi<float>());
                float bp = previous_bsdf_pdf * previous_bsdf_pdf;
                float sp = sky_pdf * sky_pdf;
                if(bp + sp > 0.0f) mis_weight = bp / (bp + sp);
            }
            addContribution(result, throughput * sky * mis_weight, previous_lobe, bounce == 0, false);
            break;
        }

        glm::vec3 point = ray.origin + ray.direction * hit.distance;
        SurfaceData surface = object->getSurfaceData(ray, hit);
        std::shared_ptr<Material> material = object->getMaterial();
        if(!material) break;

        std::unique_ptr<BSDF> bsdf = material->sampleBSDF(surface);
        if(!bsdf) break;

        glm::vec3 view = -ray.direction;
        if(!has_primary) {
            has_primary = true;
            result.object_id = object->getID();
            result.hit_position = point;
            result.geometric_normal = object->getGeometricNormal(ray, hit);
            result.shading_normal = bsdf->getNormal();
            result.subsurface_albedo = bsdf->getSubsurfaceAlbedo();
            result.specular_color = bsdf->getSpecularColor();
            result.emission = bsdf->getEmission(view);
        }

        Color emission = bsdf->getEmission(view);
        if(maxChannel(emission) > 0.0f) {
            Color emitted = throughput * emission;
            if(bounce == 0) {
                result.emission += emitted;
            } else {
                float mis_weight = 1.0f;
                if(settings.enable_mis && previous_lobe != LobeType::Specular && previous_lobe != LobeType::Transmission) {
                    auto hit_light = std::static_pointer_cast<Light>(object);
                    float light_pick_pdf = lightProbability(
                        scene,
                        hit_light,
                        previous_path_point,
                        previous_path_normal,
                        settings.light_sampler
                    );
                    float light_pdf = light_pick_pdf * object->pdf(ray, hit);
                    float bp = previous_bsdf_pdf * previous_bsdf_pdf;
                    float lp = light_pdf * light_pdf;
                    if(lp + bp > 0.0f) mis_weight = bp / (bp + lp);
                }
                addContribution(result, emitted * mis_weight, previous_lobe, false, false);
            }
        }

        float coverage = bsdf->getCoverage(view);
        if(coverage < 1.0f && sampler.next() > coverage) {
            ray.origin = point + RAY_EPSILON * ray.direction;
            continue;
        }

        if(settings.enable_nee && !bsdf->isDelta()) {
            Color direct = throughput * directLighting(scene, point, bsdf->getNormal(), view, *bsdf, sampler, settings);
            direct += throughput * directSkyLighting(scene, point, bsdf->getNormal(), view, *bsdf, sampler, settings);
            addContribution(result, direct, previous_lobe, bounce == 0, true);
        }

        if(settings.enable_path_regularization && any_non_delta) {
            bsdf->regularize();
        }

        BSDFSample sample = bsdf->sample(sampler.next3(), view);
        Color bounce_throughput = sample.throughput.sum();
        if(sample.lobe == LobeType::Specular || sample.lobe == LobeType::Transmission) {
            bounce_throughput = sample.throughput.specular;
        } else if(sample.lobe == LobeType::Diffuse) {
            bounce_throughput = sample.throughput.diffuse + sample.throughput.specular;
        }

        if(
            maxChannel(bounce_throughput) <= 0.0f ||
            !std::isfinite(bounce_throughput.r) ||
            !std::isfinite(bounce_throughput.g) ||
            !std::isfinite(bounce_throughput.b)
        ) {
            break;
        }

        throughput *= bounce_throughput;
        previous_lobe = sample.lobe;
        previous_path_point = point;
        previous_path_normal = bsdf->getNormal();
        previous_bsdf_pdf = sample.pdf;
        any_non_delta = any_non_delta || !sample.is_delta;

        if(settings.enable_russian_roulette && bounce >= 2) {
            float continue_probability = glm::clamp(maxChannel(throughput), 0.05f, 0.95f);
            if(sampler.next() > continue_probability) break;
            throughput /= continue_probability;
        }

        ray.origin = point + RAY_EPSILON * sample.direction;
        ray.direction = glm::normalize(sample.direction);
    }

    return result;
}

} // namespace render::cpu
