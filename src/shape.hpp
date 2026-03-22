#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <span>
#include <vector>
#include "ray.hpp"

// An enum defining the direction of the ray hit relative to the shape.
enum struct HitDirection {
    NONE, // This is used when the shape has no volume (e.g., a single plane in space) so we can neither enter or exit it.
    ENTERING, // This is used when the ray is entering the shape volume.
    EXITING, // This is used when the ray is exiting the shape volume.
};

// The surface data at a given hitpoint.
struct SurfaceData {
    HitDirection hit_direction; // Are we entering or exiting the shape volume?
    glm::vec3 normal, tangent, bitangent; // The interpolated normal, tangent & bitangent at the hit point.
    glm::vec2 uv; // The interpolated texture coordinates at the hit point. 
};

struct AABB {
    glm::vec3 min = glm::vec3(0.0f);
    glm::vec3 max = glm::vec3(0.0f);
};

// A base class for all shapes.
class Shape {
public:
    // Compute the intersection with the shape. If there is a hit, it returns true and set the hit data into the hit argument. Otherwise, it returns false.
    virtual bool intersect(const Ray& ray, RayHit& hit) const = 0;
    // Given a ray and its hit data, it returns the surface data at the hit point.
    virtual SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const = 0;
    // Return local-space axis-aligned bounds for fast broad-phase culling.
    virtual AABB getBounds() const = 0;
};

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal, tangent, bitangent;
    glm::vec2 uv;
};

// A triangle shape.
class Triangle : public Shape {
public:
    Triangle(Vertex v0, Vertex v1, Vertex v2): v0(v0), v1(v1), v2(v2) {}

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override;
    
private:
    // The three vertices of the triangle.
    Vertex v0, v1, v2;
};

// Since it is not efficient to define an object in the scene for each individual triangle, 
// we define a shape called "TriangleMesh" which consists of many triangles and a BVH to accelerate intersection tests with the triangles.
// Using triangle meshes, we can create complex shapes out of many triangles and control them (e.g., move them, change their material) as a single object.
// We can also create multiple objects with this complex shape without repeating the same mesh data, thus saving memory.
class TriangleMesh : public Shape {
public:
    TriangleMesh(const std::span<Triangle>& triangles);

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override { return bounds; }

private:
    std::vector<Triangle> triangles; // The triangles in the mesh.
    AABB bounds;
};

// A sphere shape. While we could just use triangle meshes to approximate spheres, 
// this shape will be a 100% accurate sphere and will be more memory and computationally efficient.
class Sphere : public Shape {
public:
    Sphere(glm::vec3 center, float radius): center(center), radius(radius) {}

    bool intersect(const Ray& ray, RayHit& hit) const override;
    SurfaceData getSurfaceData(const Ray& ray, const RayHit& hit) const override;
    AABB getBounds() const override;
    
private:
    glm::vec3 center;
    float radius;
};

// Helper functions to create basic shapes or load mesh data from files.

// Create a rectangle with the given "size" around the given "center", then rotate it by the given "rotation".
// "tiling" defines how many times the texture should be repeated on the the triangle surface on both the x and y directions.  
std::shared_ptr<TriangleMesh> createRectange(glm::vec3 center, glm::vec2 size, glm::quat rotation, glm::vec2 tiling);

// Create a cuboid with the given "size" around the given "center", then rotate it by the given "rotation".
std::shared_ptr<TriangleMesh> createCuboid(glm::vec3 center, glm::vec3 size, glm::quat rotation);

// Load a Wavefront OBJ file from the given file path.
std::shared_ptr<TriangleMesh> loadMeshOBJ(const char* file_path);
