#pragma once

#include <glm/glm.hpp>
#include "core/color.hpp"
#include "scene/geometry/shape_base.hpp"

inline glm::vec3 computeGlobalNormal(const SurfaceData& surface, const Color& normal_map_sample) {
    Color local = 2.0f * normal_map_sample - 1.0f;
    return glm::normalize(glm::vec3(
        local.x * surface.tangent +
        local.y * surface.bitangent +
        local.z * surface.normal
    ));
}
