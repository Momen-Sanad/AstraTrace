#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include "scene/geometry/triangle_mesh.hpp"

std::shared_ptr<TriangleMesh> createRectange(
    glm::vec3 center,
    glm::vec2 size,
    glm::quat rotation,
    glm::vec2 tiling
);

std::shared_ptr<TriangleMesh> createCuboid(
    glm::vec3 center,
    glm::vec3 size,
    glm::quat rotation
);

std::shared_ptr<TriangleMesh> loadMeshOBJ(const char* file_path);
