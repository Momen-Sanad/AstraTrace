#pragma once

#include <glm/glm.hpp>

// A ray structure defining the ray origin and direction.
struct Ray {
    glm::vec3 origin, direction;
};

// A structure to store the result of a ray intersection with a surface.
// We stopped storing normals in the hit structure since we will compute many hit that will be discarded when we find a closer hit.
// So there is no reason to compute the hit normal (and now we will need the tangent, bitangent and texture coordinates too) for each hit.
// Instead, we will use the surface coords and id to find these values when we need them.
struct RayHit {
    float distance; // The distance to the ray hit position.
    glm::vec2 surface_coords; // Where we hit the surface as coordinate defined by the surface itself (e.g., barycentric coordinates).
    uint32_t surface_id; // An id for the surface we hit (e.g., the triangle index in the triangle mesh).
};