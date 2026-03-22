#include "scene.hpp"
#include "PBR.hpp"
#include <SDL3/SDL_log.h>

namespace {

bool raySphereMayHit(
    const Ray& ray,
    const glm::vec3& center,
    float radius,
    float max_distance
) {
    if(radius <= 0.0f) return true;

    glm::vec3 oc = ray.origin - center;
    float a = glm::dot(ray.direction, ray.direction);
    float b = glm::dot(oc, ray.direction);
    float c = glm::dot(oc, oc) - radius * radius;
    float discriminant = b * b - a * c;
    if(discriminant < 0.0f) return false;

    float sqrt_discriminant = glm::sqrt(discriminant);
    float inv_a = 1.0f / a;
    float t0 = (-b - sqrt_discriminant) * inv_a;
    float t1 = (-b + sqrt_discriminant) * inv_a;
    if(t1 < 0.0f) return false;
    return t0 <= max_distance;
}

} // namespace

bool SceneObject::intersect(const Ray& ray, RayHit& hit) const {
    if(is_identity_transform) {
        return shape->intersect(ray, hit);
    }

    // First, we transform the ray to the local shape space.
    // Note that we don't renormalize the ray direction. This way, we don't need to apply 
    // any transformations to the hit distance returned by the shape's intersection function.
    Ray transformed_ray = {
        .origin = transform_inverse * glm::vec4(ray.origin, 1.0f),
        .direction = transform_inverse * glm::vec4(ray.direction, 0.0f)
    };
    // Then we compute the intersection against the shape.
    return shape->intersect(transformed_ray, hit);
}

bool SceneObject::mayIntersect(const Ray& ray, float max_distance) const {
    return raySphereMayHit(ray, bounds_center, bounds_radius, max_distance);
}

SurfaceData SceneObject::getSurfaceData(const Ray& ray, const RayHit& hit) const { 
    if(is_identity_transform) {
        return shape->getSurfaceData(ray, hit);
    }

    // First, we transform the ray to the local shape space.
    Ray transformed_ray = {
        .origin = transform_inverse * glm::vec4(ray.origin, 1.0f),
        .direction = transform_inverse * glm::vec4(ray.direction, 0.0f)
    };
    // Then we get the surface data from the shape.
    SurfaceData surface = shape->getSurfaceData(transformed_ray, hit);
    
    // Then we transform the normal, tangent and bitangent to the world space.
    // Note: remember to renormalize the normal, tangent and bitangent after transformation.
    surface.normal = glm::transpose(transform_inverse) * glm::vec4(surface.normal, 0.0f);
    surface.normal = glm::normalize(surface.normal);
    
    surface.tangent = transform * glm::vec4(surface.tangent, 0.0f);
    surface.tangent = glm::normalize(surface.tangent);
    
    surface.bitangent = transform * glm::vec4(surface.bitangent, 0.0f);
    surface.bitangent = glm::normalize(surface.bitangent);
    
    return surface;
}

void SceneObject::update() {
    if(!is_dirty) return; // If we are not dirty, no need to recompute anything.
    is_dirty = false;

    is_identity_transform =
        position == glm::vec3(0.0f) &&
        rotation == glm::quat(1.0f, 0.0f, 0.0f, 0.0f) &&
        scale == glm::vec3(1.0f);

    // First, we compute the transformation matrix (local to world space).
    if(is_identity_transform) {
        transform = glm::mat4(1.0f);
        transform_inverse = glm::mat4(1.0f);
    } else {
        transform = glm::translate(glm::mat4(1.0f), position) * 
                    glm::mat4_cast(rotation) * 
                    glm::scale(glm::mat4(1.0f), scale);
        // Then we compute the inverse transformation matrix (world to local space).
        transform_inverse = glm::inverse(transform);
    }

    // Update world-space bounding sphere for fast broad-phase culling.
    AABB local_bounds = shape->getBounds();
    glm::vec3 local_center = 0.5f * (local_bounds.min + local_bounds.max);
    float local_radius = glm::length(0.5f * (local_bounds.max - local_bounds.min));

    bounds_center = glm::vec3(transform * glm::vec4(local_center, 1.0f));

    float scale_x = glm::length(glm::vec3(transform[0]));
    float scale_y = glm::length(glm::vec3(transform[1]));
    float scale_z = glm::length(glm::vec3(transform[2]));
    float max_scale = glm::max(scale_x, glm::max(scale_y, scale_z));
    bounds_radius = local_radius * max_scale;
}

void Scene::update() {
    // we loop over the objects and update them.
    for(auto& object: objects) {
        object->update();
    }
}

bool Scene::anyHit(const Ray& ray, float max_distance) const {
    // We loop over all the objects and loop for a hit.
    for(auto& object: objects) {
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
    // We loop over all the objects and find the closest hit
    for(auto& object: objects) {
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

// This is a helper function for normal mapping thant transforms the sample taken from the normal map to a global normal vector.
glm::vec3 computeGlobalNormal(const SurfaceData& surface, const Color& normal_map_sample) {
    Color local = 2.0f * normal_map_sample - 1.0f; // First, we map the sample from the range [0,1] to the range [-1, 1].
    // Then we transform the local normal to a global normal.
    return glm::normalize(glm::vec3(
        local.x * surface.tangent +
        local.y * surface.bitangent +
        local.z * surface.normal
    ));
}

Color Scene::computeShadow(Ray ray, float max_distance) const {
    // We will take transparency into consideration while computing the shadow.
    const float ray_epsilon = 0.01f;
    Color shadow = Color(1.0f);
    while(shadow != Color(0.0f) && max_distance > 0.0f) { // When the shadow color becomes black (light is fully blocked) or we reached the light, we stop.
        RayHit hit;
        if(auto object = findClosestHit(ray, hit)) { // First, we look for the closest light blocker
            if(hit.distance > max_distance) break; // If the light blocker is further away than the light, we stop.
            auto material = object->getMaterial();
            if(auto pbr = std::dynamic_pointer_cast<PBRMaterial>(material)) {
                // If the material is PBR, we read the opacity (alpha) at the hit point and decrease the shadow color accordingly.
                SurfaceData surface = object->getSurfaceData(ray, hit);
                ColorA color_alpha = pbr->sampleBaseColor(surface.uv);
                shadow *= (1.0f - color_alpha.a);
            } else if(auto mirror = std::dynamic_pointer_cast<SmoothMirrorMaterial>(material)) {
                // We treat mirrors as fully opaque so we fully block the light.
                shadow = Color(0.0f);
            } else if(auto glass = std::dynamic_pointer_cast<SmoothGlassMaterial>(material)) {
                // Realistically, refractive materials could refract light from different directions onto the shaded point 
                // but this is hard to compute in Whitted-style ray tracers, so we just use a very simple (and unrealistic) assumption that
                // the light will not be refracted along the shadow ray. Therefore, we just multiply the shadow color by the surface color.
                SurfaceData surface = object->getSurfaceData(ray, hit);
                Color color = glass->sampleBaseColor(surface.uv);
                shadow *= color;
            }
            // After processing the hit, we move the ray forward to after the hit position, and subtract the hit distance from the max distance.
            ray.origin += ray.direction * (hit.distance + ray_epsilon);
            max_distance -= hit.distance + ray_epsilon;
        } else break; // If there is no hit, then there are no more blockers and we just return the current shadow color.
    }
    return shadow;
}

Color Scene::rayTrace(const Ray& ray, int max_depth) const {
    // This is the core of the Whitted-style ray tracer.
    const float ray_epsilon = 0.01f;
    RayHit hit;
    if(auto object = findClosestHit(ray, hit)) { // First, we look for the closest hit in the scene.
        glm::vec3 position = ray.origin + ray.direction * hit.distance;
        auto material = object->getMaterial();
        SurfaceData surface = object->getSurfaceData(ray, hit);
        if(auto pbr = std::dynamic_pointer_cast<PBRMaterial>(material)) {
            // If it is a generic PBR material, we will evaluate the local illumination at the hit point.
            // Important Note: We won't support reflections of this surface since it is computationally expensive to compute rough reflections in Whitted-style ray tracers.

            // First, we sample the local normal from the normal map and transform it to the world space. 
            glm::vec3 normal = computeGlobalNormal(surface, pbr->sampleNormal(surface.uv));

            // Then, we sample the surface color and opacity (alpha)
            ColorA color_alpha = pbr->sampleBaseColor(surface.uv);
            Color color = Color(color_alpha);
            float alpha = color_alpha.a;

            // Then, we sample the metalness and roughness.
            Color mr = pbr->sampleMetalRoughness(surface.uv);
            float metalness = mr.r;
            float roughness = mr.g;
            // Finally, we sample the ambient occlusion.
            float occlusion = pbr->sampleOcclusion(surface.uv);

            // Convert BaseColor/Metalness workflow to Albedo/Specular workflow
            Color albedo = color * (1.0f - metalness); // Subsurface albedo
            Color F0 = glm::mix(Color(0.04f), color, metalness); // Specular color
            
            // The view vector is the inverse of the ray direction
            glm::vec3 view = -ray.direction;
            
            Color outgoing_radiance = Color(0.0f); 
            if(alpha > 0.0f) {
                // Independently of the light contributions, there are two other sources of outgoing radiance:
                // - Ambient light: It is a cheap approximation of global illumination. 
                //                  It is light coming from everywhere and it only contribute to the diffuse part of the light.
                //                  The occlusion factor makes ambient light look darker in occluded areas.
                // - Emission: It is light emitted by the object itself (e.g., fire, tv screen). 
                outgoing_radiance = ambient * albedo * occlusion + pbr->sampleEmissive(surface.uv);
                
                // Next, we loop over every light source.
                for(auto light : lights) {
                    // For the current light source, we evalute the following for the given point: 
                    // the light vector, the attenuated light color and light distance. 
                    LightEvaluation eval = light->evaluate(position);
                    // Then we call a function to compute the outgoing radiance contribution of the light source.
                    // We use Lambert for the diffuse term and GGX Cook-Torrance for the specular term.
                    Color light_contribution = eval.radiance * computeLambertDiffuseAndGGXSpecular(albedo, F0, normal, eval.light_vector, view, roughness);
                    // If the light contribution is too low to have an effect on the final color, we skip it.
                    if(glm::dot(light_contribution, Color(1.0f)) > (1.0f / 255.0f)) {
                        // Before adding the light contribution, we compute the shadow color.
                        Color shadow = computeShadow({
                            position + ray_epsilon * eval.light_vector, eval.light_vector
                        }, eval.distance - ray_epsilon);
                        // Finally, we add the light contribution multiplied by the shadow color to the outgoing radiance.
                        outgoing_radiance += light_contribution * shadow;                        
                    }
                }
            }

            if(alpha < 1.0f && max_depth > 0) { // If the alpha is less than 1, we will need to blend the outgoing radiance with the radiance coming from behind the object.
                // So, we recursively trace the ray from behind the hit point, then blend it with the current outgoing radiance using alpha.
                return glm::mix(
                    rayTrace({ position + ray_epsilon * ray.direction, ray.direction }, max_depth - 1), 
                    outgoing_radiance, 
                    alpha);
            } else {
                // If the object is fully opaque, we just return the computed outgoing radiance.
                return outgoing_radiance;
            }

        } else if(auto glass = std::dynamic_pointer_cast<SmoothGlassMaterial>(material)) {
            // If it is a smooth glass material, we will fork a reflection and a refraction ray from the hit point, then blend their tracing results.

            // First, we sample the local normal from the normal map and transform it to the world space. 
            glm::vec3 normal = computeGlobalNormal(surface, glass->sampleNormal(surface.uv));

            // Then, we compute the surface color (we assume it is the color of the surface only, not the volume inside the object since this would need a volumetric renderer).
            Color color = glass->sampleBaseColor(surface.uv);

            // Then we comput the relative IOR (eta) based on whether the ray is entering or exiting the object.
            float eta = 
                surface.hit_direction == HitDirection::ENTERING ? 1.0f / glass->refractive_index :
                surface.hit_direction == HitDirection::EXITING ? glass->refractive_index : 
                1.0f;
            
            // Then, we compute the specular color based on the IOR (it is monochromatic for dieletrics).
            float F0 = (glass->refractive_index - 1) / (glass->refractive_index + 1);
            F0 = F0 * F0;

            // Then, we compute the reflection direction.
            glm::vec3 reflected = glm::reflect(ray.direction, normal);
            // Due to normal mapping, we may end up in a situation where the reflected ray is still pointed towards the surface.
            // In that case, we reflect it but the surface normal (from before the normal mapping) to make sure it is going away from the surface.
            if(glm::dot(reflected, surface.normal) < 0.0f) reflected = glm::reflect(reflected, surface.normal);
            
            // Then, we compute the refraction direction.
            glm::vec3 refracted = glm::refract(ray.direction, normal, eta);
            
            if(refracted == glm::vec3(0.0f))  { // If the refracted ray is 0, then we have a total internal reflection.
                // In this case, we just trace the refracted ray.
                if(max_depth <= 0) return background_color;
                return rayTrace({ position + ray_epsilon * reflected, reflected }, max_depth - 1);
            } else {
                // Based on the relative change in IOR, we choose the correct Frensel Schlick approximation to compute the reflected light ratio.
                Color F = Color(F0);
                if(eta > 1.0f) F = computeFresnelSchlick(-normal, refracted, F);
                else F = computeFresnelSchlick(normal, reflected, F);
                if(max_depth <= 0) return (color * (1.0f - F) + F) * background_color;
                // Finally, we compute the result of the reflection and refraction via ray tracing, then we blend them.
                Color reflection_result = rayTrace({ position + ray_epsilon * reflected, reflected }, max_depth - 1);
                Color refraction_result = rayTrace({ position + ray_epsilon * refracted, refracted }, max_depth - 1);
                return F * reflection_result + color * (1.0f - F) * refraction_result;
            }
        } else if(auto mirror = std::dynamic_pointer_cast<SmoothMirrorMaterial>(material)) {
            //TODO: Implement the smooth mirror material.
            // If it is a smooth mirror material, we will trace a reflection ray from the hit point, then return the result modulated by fresnel.

            // First, we sample the local normal from the normal map and transform it to the world space. 
            glm::vec3 normal = computeGlobalNormal(surface, mirror->sampleNormal(surface.uv));

            // Then, we sample the specular color and compute the Fresnel factor.
            Color F0 = mirror->sampleBaseColor(surface.uv);
            Color F = computeFresnelSchlick(normal, -ray.direction, F0);
            if(max_depth <= 0) return F * background_color;
            
            // Then, we compute the reflection direction.
            glm::vec3 reflected = glm::reflect(ray.direction, normal);
            // Due to normal mapping, we may end up in a situation where the reflected ray is still pointed towards the surface.
            // In that case, we reflect it but the surface normal (from before the normal mapping) to make sure it is going away from the surface.
            if(glm::dot(reflected, surface.normal) < 0.0f) reflected = glm::reflect(reflected, surface.normal);

            // Finally, we compute the result of the reflection via ray tracing, then return the result modulated by fresnel.
            return F * rayTrace({ position + ray_epsilon * reflected, reflected }, max_depth - 1);
        } else {
            // As a fallback, we will return black if no material was defined.
            return Color(0.0f);
        }
    } else {
        // If we hit nothing, we return the background color.
        return background_color;
    }
}

void Scene::printStats() {
    // Some useful statistics about the scene.
    SDL_Log("Scene Statistics:");
    SDL_Log("\t- Object Count: %llu", objects.size());
    SDL_Log("\t- Light Count: %llu", lights.size());
}
