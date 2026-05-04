#pragma once

#include "render/renderer.hpp"

namespace render::cpu {

struct PathResult {
    ObjectID object_id = 0;
    glm::vec3 hit_position = glm::vec3(0.0f);
    glm::vec3 geometric_normal = glm::vec3(0.0f);
    glm::vec3 shading_normal = glm::vec3(0.0f);
    Color subsurface_albedo = Color(0.0f);
    Color specular_color = Color(0.0f);
    Color emission = Color(0.0f);
    Color direct_diffuse = Color(0.0f);
    Color indirect_diffuse = Color(0.0f);
    Color specular = Color(0.0f);

    Color radiance() const {
        return emission + direct_diffuse + indirect_diffuse + specular;
    }
};

class PathIntegrator {
public:
    PathResult trace(
        const Scene& scene,
        const Camera& camera,
        glm::ivec2 pixel,
        glm::vec2 jitter,
        glm::ivec2 resolution,
        uint32_t sample_index,
        const PathRenderSettings& settings
    ) const;

private:
};

} // namespace render::cpu
