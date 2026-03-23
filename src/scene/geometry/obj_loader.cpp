#include "scene/geometry/obj_loader.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <SDL3/SDL_log.h>

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
    return std::make_shared<TriangleMesh>(triangles);
}

std::shared_ptr<TriangleMesh> createCuboid(glm::vec3 center, glm::vec3 size, glm::quat rotation) {
    glm::vec3 half = size * 0.5f;
    glm::vec3 v000 = center + rotation * glm::vec3(-half.x, -half.y, -half.z);
    glm::vec3 v001 = center + rotation * glm::vec3(-half.x, -half.y,  half.z);
    glm::vec3 v010 = center + rotation * glm::vec3(-half.x,  half.y, -half.z);
    glm::vec3 v011 = center + rotation * glm::vec3(-half.x,  half.y,  half.z);
    glm::vec3 v100 = center + rotation * glm::vec3( half.x, -half.y, -half.z);
    glm::vec3 v101 = center + rotation * glm::vec3( half.x, -half.y,  half.z);
    glm::vec3 v110 = center + rotation * glm::vec3( half.x,  half.y, -half.z);
    glm::vec3 v111 = center + rotation * glm::vec3( half.x,  half.y,  half.z);

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
        Triangle(Vertex{v000, nx0, tx0, bx0, uv00}, Vertex{v010, nx0, tx0, bx0, uv01}, Vertex{v011, nx0, tx0, bx0, uv11}),
        Triangle(Vertex{v000, nx0, tx0, bx0, uv00}, Vertex{v001, nx0, tx0, bx0, uv10}, Vertex{v011, nx0, tx0, bx0, uv11}),
        Triangle(Vertex{v100, nx1, tx1, bx1, uv00}, Vertex{v110, nx1, tx1, bx1, uv01}, Vertex{v111, nx1, tx1, bx1, uv11}),
        Triangle(Vertex{v100, nx1, tx1, bx1, uv00}, Vertex{v101, nx1, tx1, bx1, uv10}, Vertex{v111, nx1, tx1, bx1, uv11}),
        Triangle(Vertex{v000, ny0, ty0, by0, uv00}, Vertex{v100, ny0, ty0, by0, uv10}, Vertex{v101, ny0, ty0, by0, uv11}),
        Triangle(Vertex{v000, ny0, ty0, by0, uv00}, Vertex{v001, ny0, ty0, by0, uv01}, Vertex{v101, ny0, ty0, by0, uv11}),
        Triangle(Vertex{v010, ny1, ty1, by1, uv00}, Vertex{v110, ny1, ty1, by1, uv10}, Vertex{v111, ny1, ty1, by1, uv11}),
        Triangle(Vertex{v010, ny1, ty1, by1, uv00}, Vertex{v011, ny1, ty1, by1, uv01}, Vertex{v111, ny1, ty1, by1, uv11}),
        Triangle(Vertex{v000, nz0, tz0, bz0, uv00}, Vertex{v100, nz0, tz0, bz0, uv10}, Vertex{v110, nz0, tz0, bz0, uv11}),
        Triangle(Vertex{v000, nz0, tz0, bz0, uv00}, Vertex{v010, nz0, tz0, bz0, uv01}, Vertex{v110, nz0, tz0, bz0, uv11}),
        Triangle(Vertex{v001, nz1, tz1, bz1, uv00}, Vertex{v101, nz1, tz1, bz1, uv10}, Vertex{v111, nz1, tz1, bz1, uv11}),
        Triangle(Vertex{v001, nz1, tz1, bz1, uv00}, Vertex{v011, nz1, tz1, bz1, uv01}, Vertex{v111, nz1, tz1, bz1, uv11}),
    };

    return std::make_shared<TriangleMesh>(triangles);
}

std::shared_ptr<TriangleMesh> loadMeshOBJ(const char* file_path) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string err;

    if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &err, file_path)) {
        SDL_Log("Failed to load obj file \"%s\" due to error: %s", file_path, err.c_str());
        return nullptr;
    }

    std::vector<Triangle> triangles;
    for(const auto& shape : shapes) {
        Vertex vertices[3];
        int count = 0;
        for(const auto& index : shape.mesh.indices) {
            Vertex vertex = {};
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

                glm::vec3 bitangent = {
                    f * (-duv2.x * edge1.x + duv1.x * edge2.x),
                    f * (-duv2.x * edge1.y + duv1.x * edge2.y),
                    f * (-duv2.x * edge1.z + duv1.x * edge2.z)
                };

                vertices[0].tangent = tangent;
                vertices[1].tangent = tangent;
                vertices[2].tangent = tangent;

                vertices[0].bitangent = bitangent;
                vertices[1].bitangent = bitangent;
                vertices[2].bitangent = bitangent;

                triangles.push_back(Triangle(vertices[0], vertices[1], vertices[2]));
                count = 0;
            }
        }
    }

    return std::make_shared<TriangleMesh>(triangles);
}
