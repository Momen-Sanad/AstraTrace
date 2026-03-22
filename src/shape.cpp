#include "shape.hpp"
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <SDL3/SDL_log.h>

AABB Triangle::getBounds() const {
    AABB bounds;
    bounds.min = glm::min(v0.position, glm::min(v1.position, v2.position));
    bounds.max = glm::max(v0.position, glm::max(v1.position, v2.position));
    return bounds;
}

TriangleMesh::TriangleMesh(const std::span<Triangle>& triangles)
    : triangles(triangles.begin(), triangles.end()) {
    if(this->triangles.empty()) {
        bounds.min = glm::vec3(0.0f);
        bounds.max = glm::vec3(0.0f);
        return;
    }

    bounds = this->triangles[0].getBounds();
    for(std::size_t i = 1; i < this->triangles.size(); ++i) {
        AABB triangle_bounds = this->triangles[i].getBounds();
        bounds.min = glm::min(bounds.min, triangle_bounds.min);
        bounds.max = glm::max(bounds.max, triangle_bounds.max);
    }
}


bool Triangle::intersect(const Ray& ray, RayHit& hit) const {
    // Moller-Trumbore triangle intersection:
    // this avoids building/inverting a matrix for every triangle test.
    const float epsilon = 1e-8f;
    glm::vec3 edge1 = v1.position - v0.position;
    glm::vec3 edge2 = v2.position - v0.position;
    glm::vec3 p = glm::cross(ray.direction, edge2);
    float det = glm::dot(edge1, p);
    if(glm::abs(det) < epsilon) return false; // Parallel to the triangle plane.

    float inv_det = 1.0f / det;
    glm::vec3 t = ray.origin - v0.position;
    float w1 = glm::dot(t, p) * inv_det; // Barycentric weight for v1.
    if(w1 < 0.0f || w1 > 1.0f) return false;

    glm::vec3 q = glm::cross(t, edge1);
    float w2 = glm::dot(ray.direction, q) * inv_det; // Barycentric weight for v2.
    if(w2 < 0.0f || (w1 + w2) > 1.0f) return false;

    float hit_distance = glm::dot(edge2, q) * inv_det;
    if(hit_distance <= epsilon) return false; // Ignore behind/too-close self-intersections.

    hit.distance = hit_distance;
    hit.surface_coords = glm::vec2(w1, w2); // Barycentric coordinates.
    return true;
}

SurfaceData Triangle::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    // We will apply barycenteric interpolation to extract the surface data from the vertices.
    float w0 = 1.0f - hit.surface_coords.x - hit.surface_coords.y;
    float w1 = hit.surface_coords.x;
    float w2 = hit.surface_coords.y;
    
    SurfaceData surface;
    // Note: we don't normalize the vectors here since the scene object will do so after the transformation.
    surface.normal = v0.normal * w0 + v1.normal * w1 + v2.normal * w2;
    surface.tangent = v0.tangent * w0 + v1.tangent * w1 + v2.tangent * w2;
    surface.bitangent = v0.bitangent * w0 + v1.bitangent * w1 + v2.bitangent * w2;
    surface.uv = v0.uv * w0 + v1.uv * w1 + v2.uv * w2;
    // If the ray is in the direction of the interpolated normal, then we are exiting the mesh. Otherwise, we are entering the mesh.
    // This makes no sense when we have a lone triangle (since it has no volume), but we will always use triangles inside a triangle mesh, 
    // this will tell us whether we are entering or exiting the triangle mesh (not the triangle itself).
    if(glm::dot(ray.direction, surface.normal) > 0.0f) {
        surface.normal *= -1.0f;
        surface.hit_direction = HitDirection::EXITING;
    } else {
        surface.hit_direction = HitDirection::ENTERING;
    }
    return surface;
}

bool TriangleMesh::intersect(const Ray& ray, RayHit& hit) const {
    bool has_hit = false;
    // We loop over all the triangles and find the closest hit
    for(uint32_t idx = 0; idx < triangles.size(); idx++) {
        const Triangle& triangle = triangles[idx];
        RayHit triangle_hit;
        if(triangle.intersect(ray, triangle_hit)) {
            if(!has_hit || triangle_hit.distance < hit.distance) {
                hit = triangle_hit;
                hit.surface_id = idx; // We record the index of the triangle that was hit
                has_hit = true;
            }
        }
    }
    return has_hit;
}

SurfaceData TriangleMesh::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    // Get the surface data from the triangle identified the the surface id.
    return triangles[hit.surface_id].getSurfaceData(ray, hit);
}

bool Sphere::intersect(const Ray& ray, RayHit& hit) const {
    // The equation we are trying to solve is as follows:
    // | (o + td) - c |^2 = r^2
    // We can simplify it as follows:
    // | td + (o - c) |^2 = t^2 (d.d) + 2t(d.(o-c)) + (o-c).(o-c) = r^2
    // (d.d) t^2 + 2(d.(o-c))t + |o-c|^2 - r^2 = 0
    // This is a quadratic equation in t. We can solve it to get:
    // t = (-2(d.(o-c)) ± √(4(d.(o-c))^2 - 4(|o-c|^2 - r^2))) / (2(d.d))
    // t = (d.(c-o) / d.d) ± √((d.(c-o) / d.d)^2 - (|c-o|^2 - r^2)/(d.d)^2)
    glm::vec3 diff = center - ray.origin;
    float d2inv = 1.0f / glm::dot(ray.direction, ray.direction);
    float to = glm::dot(ray.direction, diff) * d2inv;
    float det = to * to - (glm::dot(diff, diff) - radius * radius) * d2inv;
    if(det < 0.0f) return false; // There is no real solutions, so no intersection.
    float d = glm::sqrt(det);
    float t = to - d; // First, we check the smaller solution (we set ± to be -).
    if(t > 1e-5f) { // Require a tiny positive distance to avoid self-intersections.
        hit.distance = t;
        glm::vec3 normal = ((ray.origin + t * ray.direction) - center) / radius;
        // The surface coordinates will be the x & y components of the normal.
        hit.surface_coords = normal;
        // The surface id will tell whether we the hit the back face (z > 0) or the front face (z < 0) of the sphere.
        hit.surface_id = normal.z > 0.0f ? 0 : 1;
        return true;
    }
    t = to + d; // If the smaller solution was behand the ray origin, we get the larger solution.
    if(t > 1e-5f) { // Require a tiny positive distance to avoid self-intersections.
        hit.distance = t;
        glm::vec3 normal = ((ray.origin + t * ray.direction) - center) / radius;
        // The surface coordinates will be the x & y components of the normal.
        hit.surface_coords = normal;
        // The surface id will tell whether we the hit the back face (z > 0) or the front face (z < 0) of the sphere.
        hit.surface_id = normal.z > 0.0f ? 0 : 1;
        return true;
    }
    return false; 
}

SurfaceData Sphere::getSurfaceData(const Ray& ray, const RayHit& hit) const {
    // First, we reconstruct the normal from the surface coordinates and id.
    float z = glm::sqrt(glm::max(0.0f, 1.0f - hit.surface_coords.x * hit.surface_coords.x - hit.surface_coords.y * hit.surface_coords.y));
    if(hit.surface_id != 0) z = -z;
    glm::vec3 normal = glm::vec3(hit.surface_coords, z);
    
    SurfaceData surface;
    surface.normal = normal;
    if(glm::abs(surface.normal.y) == 1.0f) {
        // As a special case, if the normal is up or down, then the tangent is right, and the bitangent is either front or back. 
        surface.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
        surface.bitangent = glm::vec3(0.0f, 0.0f, -surface.normal.y);
    } else {
        // We can get the tangent and bitangent using cross products. 
        surface.tangent = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), surface.normal));
        surface.bitangent = glm::cross(surface.normal, surface.tangent);
    }

    // We turn the normal into polar coordinates, and use the angles after scaling as texture coordinates.
    float u = std::atan2(normal.z, normal.x) / glm::radians(360.0f);
    if(u < 0.0f) u += 1.0f;
    float v = 0.5f * std::asin(normal.y) / glm::radians(90.0f) + 0.5f;
    surface.uv = glm::vec2(u, v);

    // If the ray is in the direction of the normal, then we are exiting the sphere. Otherwise, we are entering the sphere.
    if(glm::dot(ray.direction, surface.normal) > 0.0f) {
        surface.normal *= -1.0f;
        surface.hit_direction = HitDirection::EXITING;
    } else {
        surface.hit_direction = HitDirection::ENTERING;
    }

    return surface;
}

/////////////////////////////////////////////////////////////////////////////
// Helper functions for creating simple mesh and loading meshes from files //
/////////////////////////////////////////////////////////////////////////////

std::shared_ptr<TriangleMesh> createRectange(glm::vec3 center, glm::vec2 size, glm::quat rotation, glm::vec2 tiling) {
    glm::vec2 half = size * 0.5f;
    glm::vec3 normal = rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 tangent = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 bitangent = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    Vertex v0 = { center + rotation * glm::vec3(-half.x, -half.y, 0.0f), normal, tangent, bitangent, glm::vec2(0.0f, 0.0f) };
    Vertex v1 = { center + rotation * glm::vec3( half.x, -half.y, 0.0f), normal, tangent, bitangent, glm::vec2(tiling.x, 0.0f) };
    Vertex v2 = { center + rotation * glm::vec3(-half.x,  half.y, 0.0f), normal, tangent, bitangent, glm::vec2(0.0f, tiling.y) };
    Vertex v3 = { center + rotation * glm::vec3( half.x,  half.y, 0.0f), normal, tangent, bitangent, glm::vec2(tiling.x, tiling.y) };
    
    Triangle triangles[] = {
        Triangle(v0, v1, v3),
        Triangle(v3, v2, v0),
    };

    auto mesh = std::make_shared<TriangleMesh>(triangles);
    return mesh;
}

std::shared_ptr<TriangleMesh> createCuboid(glm::vec3 center, glm::vec3 size, glm::quat rotation) {
    glm::vec3 half = size * 0.5f;
    glm::vec3 v000 = center + rotation * glm::vec3(-half.x, -half.y,-half.z);
    glm::vec3 v001 = center + rotation * glm::vec3(-half.x, -half.y, half.z);
    glm::vec3 v010 = center + rotation * glm::vec3(-half.x,  half.y,-half.z);
    glm::vec3 v011 = center + rotation * glm::vec3(-half.x,  half.y, half.z);
    glm::vec3 v100 = center + rotation * glm::vec3( half.x, -half.y,-half.z);
    glm::vec3 v101 = center + rotation * glm::vec3( half.x, -half.y, half.z);
    glm::vec3 v110 = center + rotation * glm::vec3( half.x,  half.y,-half.z);
    glm::vec3 v111 = center + rotation * glm::vec3( half.x,  half.y, half.z);
    
    glm::vec3 nx0 = rotation * glm::vec3(-1.0f, 0.0f, 0.0f), nx1 = -nx0;
    glm::vec3 ny0 = rotation * glm::vec3(0.0f, -1.0f, 0.0f), ny1 = -ny0;
    glm::vec3 nz0 = rotation * glm::vec3(0.0f, 0.0f, 1.0f), nz1 = -nz0;
    
    glm::vec3 tx0 = rotation * glm::vec3(0.0f, 0.0f, 1.0f), tx1 = tx0;
    glm::vec3 ty0 = rotation * glm::vec3(1.0f, 0.0f, 0.0f), ty1 = ty0;
    glm::vec3 tz0 = rotation * glm::vec3(1.0f, 0.0f, 0.0f), tz1 = tz0;
    
    glm::vec3 bx0 = rotation * glm::vec3(0.0f, 1.0f, 0.0f), bx1 = bx0;
    glm::vec3 by0 = rotation * glm::vec3(0.0f, 0.0f, 1.0f), by1 = by0;
    glm::vec3 bz0 = rotation * glm::vec3(0.0f, 1.0f, 0.0f), bz1 = bz0;
    
    glm::vec2 uv00 = glm::vec2(0.0f, 0.0f);
    glm::vec2 uv01 = glm::vec2(0.0f, 1.0f);
    glm::vec2 uv10 = glm::vec2(1.0f, 0.0f);
    glm::vec2 uv11 = glm::vec2(1.0f, 1.0f);

    Triangle triangles[] = {
        // Left Face
        Triangle(Vertex{v000, nx0, tx0, bx0, uv00}, Vertex{v010, nx0, tx0, bx0, uv01}, Vertex{v011, nx0, tx0, bx0, uv11}),
        Triangle(Vertex{v000, nx0, tx0, bx0, uv00}, Vertex{v001, nx0, tx0, bx0, uv10}, Vertex{v011, nx0, tx0, bx0, uv11}),
        // Right Face
        Triangle(Vertex{v100, nx1, tx1, bx1, uv00}, Vertex{v110, nx1, tx1, bx1, uv01}, Vertex{v111, nx1, tx1, bx1, uv11}),
        Triangle(Vertex{v100, nx1, tx1, bx1, uv00}, Vertex{v101, nx1, tx1, bx1, uv10}, Vertex{v111, nx1, tx1, bx1, uv11}),
        // Bottom Face
        Triangle(Vertex{v000, ny0, ty0, by0, uv00}, Vertex{v100, ny0, ty0, by0, uv10}, Vertex{v101, ny0, ty0, by0, uv11}),
        Triangle(Vertex{v000, ny0, ty0, by0, uv00}, Vertex{v001, ny0, ty0, by0, uv01}, Vertex{v101, ny0, ty0, by0, uv11}),
        // Top Face
        Triangle(Vertex{v010, ny1, ty1, by1, uv00}, Vertex{v110, ny1, ty1, by1, uv10}, Vertex{v111, ny1, ty1, by1, uv11}),
        Triangle(Vertex{v010, ny1, ty1, by1, uv00}, Vertex{v011, ny1, ty1, by1, uv01}, Vertex{v111, ny1, ty1, by1, uv11}),
        // Front Face
        Triangle(Vertex{v000, nz0, tz0, bz0, uv00}, Vertex{v100, nz0, tz0, bz0, uv10}, Vertex{v110, nz0, tz0, bz0, uv11}),
        Triangle(Vertex{v000, nz0, tz0, bz0, uv00}, Vertex{v010, nz0, tz0, bz0, uv01}, Vertex{v110, nz0, tz0, bz0, uv11}),
        // Back Face
        Triangle(Vertex{v001, nz1, tz1, bz1, uv00}, Vertex{v101, nz1, tz1, bz1, uv10}, Vertex{v111, nz1, tz1, bz1, uv11}),
        Triangle(Vertex{v001, nz1, tz1, bz1, uv00}, Vertex{v011, nz1, tz1, bz1, uv01}, Vertex{v111, nz1, tz1, bz1, uv11}),
    };

    auto mesh = std::make_shared<TriangleMesh>(triangles);
    return mesh;
}

std::shared_ptr<TriangleMesh> loadMeshOBJ(const char* file_path) {
        tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, file_path)) {
        SDL_Log("Failed to load obj file \"%s\" due to error: %s", file_path, err.c_str());
        return nullptr;
    }

    std::vector<Triangle> triangles;

    for (const auto &shape : shapes) {
        Vertex vertices[3];
        int count = 0;
        for (const auto &index : shape.mesh.indices) {
            Vertex vertex = {};

            // Read the data for a vertex from the "attrib" object
            vertex.position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
            };


            vertex.uv = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
            };

            vertices[count++] = vertex;
            if(count == 3) {
                // Note that OBJ files don't include tangents or bitangents, so we need to compute them.
                // See https://learnopengl.com/Advanced-Lighting/Normal-Mapping for more details.
                glm::vec3 edge1 = vertices[1].position - vertices[0].position;
                glm::vec3 edge2 = vertices[2].position - vertices[0].position;
                glm::vec2 duv1 = vertices[1].uv - vertices[0].uv;
                glm::vec2 duv2 = vertices[2].uv - vertices[0].uv;
                float f = 1.0f / (duv1.x * duv2.y - duv2.x * duv1.y);

                glm::vec3 tangent = {
                    f * (duv2.y * edge1.x - duv1.y * edge2.x),
                    f * (duv2.y * edge1.y - duv1.y * edge2.y),
                    f * (duv2.y * edge1.z - duv1.y * edge2.z)
                };

                vertices[0].tangent = tangent;
                vertices[1].tangent = tangent;
                vertices[2].tangent = tangent;

                glm::vec3 bitangent  = {
                    f * (-duv2.x * edge1.x + duv1.x * edge2.x),
                    f * (-duv2.x * edge1.y + duv1.x * edge2.y),
                    f * (-duv2.x * edge1.z + duv1.x * edge2.z)
                };

                vertices[0].bitangent = bitangent;
                vertices[1].bitangent = bitangent;
                vertices[2].bitangent = bitangent;

                triangles.push_back(Triangle(vertices[0], vertices[1], vertices[2]));
                count = 0;
            }
        }
    }
    
    auto mesh = std::make_shared<TriangleMesh>(triangles);
    return mesh;
}

AABB Sphere::getBounds() const {
    AABB bounds;
    bounds.min = center - glm::vec3(radius);
    bounds.max = center + glm::vec3(radius);
    return bounds;
}
