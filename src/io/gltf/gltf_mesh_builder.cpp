#include "io/gltf/gltf_mesh_builder.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>
#include <mikktspace.h>

#include "io/gltf/gltf_accessors.hpp"
#include "io/gltf/gltf_utils.hpp"
#include "scene/geometry/triangle.hpp"

namespace io::gltf {
namespace {

glm::vec3 chooseOrthogonal(const glm::vec3& normal) {
    if(glm::abs(normal.y) < 0.999f) {
        return glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), normal));
    }
    return glm::normalize(glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), normal));
}

struct MikktspaceUserData {
    const std::vector<glm::vec3>* positions = nullptr;
    const std::vector<glm::vec3>* normals = nullptr;
    const std::vector<glm::vec2>* uvs = nullptr;
    const std::vector<uint32_t>* indices = nullptr;
    std::vector<glm::vec3>* face_tangents = nullptr;
    std::vector<float>* face_signs = nullptr;
};

int mikktGetNumFaces(const SMikkTSpaceContext* context) {
    const auto* user = static_cast<const MikktspaceUserData*>(context->m_pUserData);
    if(!user || !user->indices) return 0;
    return static_cast<int>(user->indices->size() / 3);
}

int mikktGetNumVerticesOfFace(const SMikkTSpaceContext* context, int iFace) {
    (void)context;
    (void)iFace;
    return 3;
}

uint32_t mikktGetVertexIndex(const MikktspaceUserData& user, int iFace, int iVert) {
    std::size_t idx = static_cast<std::size_t>(iFace) * 3u + static_cast<std::size_t>(iVert);
    if(!user.indices || idx >= user.indices->size()) return 0;
    return (*user.indices)[idx];
}

void mikktGetPosition(const SMikkTSpaceContext* context, float out[], int iFace, int iVert) {
    const auto* user = static_cast<const MikktspaceUserData*>(context->m_pUserData);
    if(!user || !user->positions || !user->indices) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }

    uint32_t index = mikktGetVertexIndex(*user, iFace, iVert);
    if(index >= user->positions->size()) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }

    const glm::vec3& p = (*user->positions)[index];
    out[0] = p.x;
    out[1] = p.y;
    out[2] = p.z;
}

void mikktGetNormal(const SMikkTSpaceContext* context, float out[], int iFace, int iVert) {
    const auto* user = static_cast<const MikktspaceUserData*>(context->m_pUserData);
    if(!user || !user->normals || !user->indices) {
        out[0] = 0.0f;
        out[1] = 1.0f;
        out[2] = 0.0f;
        return;
    }

    uint32_t index = mikktGetVertexIndex(*user, iFace, iVert);
    if(index >= user->normals->size()) {
        out[0] = 0.0f;
        out[1] = 1.0f;
        out[2] = 0.0f;
        return;
    }

    const glm::vec3& n = (*user->normals)[index];
    out[0] = n.x;
    out[1] = n.y;
    out[2] = n.z;
}

void mikktGetTexCoord(const SMikkTSpaceContext* context, float out[], int iFace, int iVert) {
    const auto* user = static_cast<const MikktspaceUserData*>(context->m_pUserData);
    if(!user || !user->uvs || !user->indices) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        return;
    }

    uint32_t index = mikktGetVertexIndex(*user, iFace, iVert);
    if(index >= user->uvs->size()) {
        out[0] = 0.0f;
        out[1] = 0.0f;
        return;
    }

    const glm::vec2& uv = (*user->uvs)[index];
    out[0] = uv.x;
    out[1] = uv.y;
}

void mikktSetTSpaceBasic(
    const SMikkTSpaceContext* context,
    const float tangent[],
    float sign,
    int iFace,
    int iVert
) {
    auto* user = static_cast<MikktspaceUserData*>(context->m_pUserData);
    if(!user || !user->face_tangents || !user->face_signs) return;

    const std::size_t face_vertex_index = static_cast<std::size_t>(iFace) * 3u + static_cast<std::size_t>(iVert);
    if(face_vertex_index >= user->face_tangents->size()) return;

    (*user->face_tangents)[face_vertex_index] = glm::vec3(tangent[0], tangent[1], tangent[2]);
    (*user->face_signs)[face_vertex_index] = sign;
}

bool generateMikktspaceTangents(
    const std::vector<glm::vec3>& positions,
    const std::vector<glm::vec3>& normals,
    const std::vector<glm::vec2>& uvs,
    const std::vector<uint32_t>& indices,
    std::vector<glm::vec3>& face_tangents,
    std::vector<float>& face_signs
) {
    if(indices.size() < 3 || (indices.size() % 3) != 0) return false;

    face_tangents.assign(indices.size(), glm::vec3(0.0f));
    face_signs.assign(indices.size(), 1.0f);

    MikktspaceUserData user_data{
        .positions = &positions,
        .normals = &normals,
        .uvs = &uvs,
        .indices = &indices,
        .face_tangents = &face_tangents,
        .face_signs = &face_signs
    };

    SMikkTSpaceInterface iface{};
    iface.m_getNumFaces = mikktGetNumFaces;
    iface.m_getNumVerticesOfFace = mikktGetNumVerticesOfFace;
    iface.m_getPosition = mikktGetPosition;
    iface.m_getNormal = mikktGetNormal;
    iface.m_getTexCoord = mikktGetTexCoord;
    iface.m_setTSpaceBasic = mikktSetTSpaceBasic;

    SMikkTSpaceContext context{};
    context.m_pInterface = &iface;
    context.m_pUserData = &user_data;
    return genTangSpaceDefault(&context) != 0;
}

} // namespace

std::shared_ptr<TriangleMesh> buildMeshFromPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const glm::mat4& world_transform,
    std::string& warnings
) {
    int mode = (primitive.mode < 0) ? TINYGLTF_MODE_TRIANGLES : primitive.mode;
    if(mode != TINYGLTF_MODE_TRIANGLES) {
        appendLine(warnings, "Skipped a non-triangle primitive.");
        return nullptr;
    }

    auto position_it = primitive.attributes.find("POSITION");
    if(position_it == primitive.attributes.end()) {
        appendLine(warnings, "Skipped a primitive with missing POSITION attribute.");
        return nullptr;
    }

    const tinygltf::Accessor& position_accessor = model.accessors[position_it->second];
    std::size_t vertex_count = position_accessor.count;
    if(vertex_count == 0) return nullptr;

    std::vector<glm::vec3> positions;
    if(!readAttributeVec3(model, position_it->second, vertex_count, positions)) {
        appendLine(warnings, "Failed to read POSITION accessor.");
        return nullptr;
    }

    std::vector<glm::vec2> uvs(vertex_count, glm::vec2(0.0f));
    bool has_uv = false;
    if(auto uv_it = primitive.attributes.find("TEXCOORD_0"); uv_it != primitive.attributes.end()) {
        has_uv = readAttributeVec2(model, uv_it->second, vertex_count, uvs);
        if(!has_uv) appendLine(warnings, "Failed to read TEXCOORD_0 accessor. Falling back to zero UV.");
    }

    std::vector<glm::vec2> uvs1 = uvs;
    if(auto uv1_it = primitive.attributes.find("TEXCOORD_1"); uv1_it != primitive.attributes.end()) {
        if(!readAttributeVec2(model, uv1_it->second, vertex_count, uvs1)) {
            uvs1 = uvs;
            appendLine(warnings, "Failed to read TEXCOORD_1 accessor. Falling back to TEXCOORD_0.");
        }
    }

    std::vector<glm::vec3> normals(vertex_count, glm::vec3(0.0f));
    bool has_normals = false;
    if(auto normal_it = primitive.attributes.find("NORMAL"); normal_it != primitive.attributes.end()) {
        has_normals = readAttributeVec3(model, normal_it->second, vertex_count, normals);
        if(!has_normals) appendLine(warnings, "Failed to read NORMAL accessor. Recomputing normals.");
    }

    std::vector<glm::vec4> tangent4(vertex_count, glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    bool has_tangents = false;
    if(auto tangent_it = primitive.attributes.find("TANGENT"); tangent_it != primitive.attributes.end()) {
        has_tangents = readAttributeVec4(model, tangent_it->second, vertex_count, tangent4);
        if(!has_tangents) appendLine(warnings, "Failed to read TANGENT accessor. Recomputing tangents.");
    }

    std::vector<uint32_t> indices;
    if(primitive.indices >= 0) {
        if(!readIndexAccessor(model, model.accessors[primitive.indices], indices)) {
            appendLine(warnings, "Failed to read index accessor.");
            return nullptr;
        }
    } else {
        indices.resize(vertex_count);
        for(std::size_t i = 0; i < vertex_count; ++i) indices[i] = static_cast<uint32_t>(i);
    }

    if(indices.size() < 3) return nullptr;

    if(!has_normals) {
        for(std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            uint32_t i0 = indices[i + 0];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            if(i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) continue;
            glm::vec3 edge1 = positions[i1] - positions[i0];
            glm::vec3 edge2 = positions[i2] - positions[i0];
            glm::vec3 n = glm::cross(edge1, edge2);
            if(glm::dot(n, n) > GLTF_EPSILON) {
                n = glm::normalize(n);
                normals[i0] += n;
                normals[i1] += n;
                normals[i2] += n;
            }
        }
        for(glm::vec3& n : normals) {
            if(glm::dot(n, n) > GLTF_EPSILON) n = glm::normalize(n);
            else n = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    } else {
        for(glm::vec3& n : normals) {
            if(glm::dot(n, n) > GLTF_EPSILON) n = glm::normalize(n);
            else n = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    std::vector<glm::vec3> tangents(vertex_count, glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(vertex_count, glm::vec3(0.0f));
    std::vector<glm::vec3> face_tangents;
    std::vector<float> face_signs;
    bool has_face_tangents = false;

    if(has_tangents) {
        for(std::size_t i = 0; i < vertex_count; ++i) {
            glm::vec3 t = glm::vec3(tangent4[i]);
            if(glm::dot(t, t) <= GLTF_EPSILON) t = chooseOrthogonal(normals[i]);
            else t = glm::normalize(t);
            tangents[i] = t;
            bitangents[i] = glm::normalize(glm::cross(normals[i], t) * tangent4[i].w);
        }
    } else if(has_uv && generateMikktspaceTangents(positions, normals, uvs, indices, face_tangents, face_signs)) {
        has_face_tangents = true;
    } else {
        if(has_uv) appendLine(warnings, "Mikktspace tangent generation failed. Using fallback tangent generation.");
        for(std::size_t i = 0; i + 2 < indices.size(); i += 3) {
            uint32_t i0 = indices[i + 0];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            if(i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) continue;

            glm::vec3 edge1 = positions[i1] - positions[i0];
            glm::vec3 edge2 = positions[i2] - positions[i0];
            glm::vec2 duv1 = uvs[i1] - uvs[i0];
            glm::vec2 duv2 = uvs[i2] - uvs[i0];
            float det = duv1.x * duv2.y - duv1.y * duv2.x;
            glm::vec3 tangent = glm::abs(det) > GLTF_EPSILON
                ? (edge1 * duv2.y - edge2 * duv1.y) / det
                : chooseOrthogonal(normals[i0]);
            if(glm::dot(tangent, tangent) > GLTF_EPSILON) tangent = glm::normalize(tangent);
            else tangent = chooseOrthogonal(normals[i0]);

            tangents[i0] += tangent;
            tangents[i1] += tangent;
            tangents[i2] += tangent;
        }

        for(std::size_t i = 0; i < vertex_count; ++i) {
            glm::vec3 t = tangents[i];
            t = t - normals[i] * glm::dot(normals[i], t);
            if(glm::dot(t, t) <= GLTF_EPSILON) t = chooseOrthogonal(normals[i]);
            else t = glm::normalize(t);
            tangents[i] = t;
            bitangents[i] = glm::normalize(glm::cross(normals[i], t));
        }
    }

    const glm::mat3 world3 = glm::mat3(world_transform);
    const glm::mat3 normal_matrix = glm::transpose(glm::inverse(world3));

    auto makeVertex = [&](uint32_t vertex_index, const glm::vec3& tangent_local, const glm::vec3& bitangent_local) {
        glm::vec3 world_normal = normal_matrix * normals[vertex_index];
        if(glm::dot(world_normal, world_normal) > GLTF_EPSILON) {
            world_normal = glm::normalize(world_normal);
        } else {
            world_normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        glm::vec3 world_tangent = world3 * tangent_local;
        if(glm::dot(world_tangent, world_tangent) <= GLTF_EPSILON) {
            world_tangent = chooseOrthogonal(world_normal);
        } else {
            world_tangent = glm::normalize(world_tangent);
            world_tangent = world_tangent - world_normal * glm::dot(world_normal, world_tangent);
            if(glm::dot(world_tangent, world_tangent) <= GLTF_EPSILON) {
                world_tangent = chooseOrthogonal(world_normal);
            } else {
                world_tangent = glm::normalize(world_tangent);
            }
        }

        glm::vec3 world_bitangent = glm::cross(world_normal, world_tangent);
        if(glm::dot(world_bitangent, world_bitangent) <= GLTF_EPSILON) {
            world_bitangent = chooseOrthogonal(world_normal);
        } else {
            world_bitangent = glm::normalize(world_bitangent);
        }

        glm::vec3 source_world_bitangent = world3 * bitangent_local;
        if(glm::dot(source_world_bitangent, source_world_bitangent) > GLTF_EPSILON) {
            if(glm::dot(world_bitangent, source_world_bitangent) < 0.0f) {
                world_bitangent *= -1.0f;
            }
        }

        return Vertex{
            glm::vec3(world_transform * glm::vec4(positions[vertex_index], 1.0f)),
            world_normal,
            world_tangent,
            world_bitangent,
            uvs[vertex_index],
            uvs1[vertex_index]
        };
    };

    std::vector<Triangle> triangles;
    triangles.reserve(indices.size() / 3);
    for(std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        if(i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) continue;

        auto getLocalBasis = [&](uint32_t vertex_index, std::size_t corner_index) {
            glm::vec3 normal = normals[vertex_index];
            glm::vec3 tangent(0.0f);
            glm::vec3 bitangent(0.0f);

            if(has_face_tangents) {
                tangent = face_tangents[corner_index];
                if(glm::dot(tangent, tangent) <= GLTF_EPSILON) {
                    tangent = chooseOrthogonal(normal);
                } else {
                    tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
                    if(glm::dot(tangent, tangent) <= GLTF_EPSILON) {
                        tangent = chooseOrthogonal(normal);
                    } else {
                        tangent = glm::normalize(tangent);
                    }
                }
                bitangent = glm::cross(normal, tangent) * face_signs[corner_index];
                if(glm::dot(bitangent, bitangent) <= GLTF_EPSILON) {
                    bitangent = glm::cross(normal, tangent);
                }
                bitangent = glm::normalize(bitangent);
            } else {
                tangent = tangents[vertex_index];
                bitangent = bitangents[vertex_index];
            }

            return std::pair<glm::vec3, glm::vec3>(tangent, bitangent);
        };

        auto [t0, b0] = getLocalBasis(i0, i + 0);
        auto [t1, b1] = getLocalBasis(i1, i + 1);
        auto [t2, b2] = getLocalBasis(i2, i + 2);
        triangles.emplace_back(
            makeVertex(i0, t0, b0),
            makeVertex(i1, t1, b1),
            makeVertex(i2, t2, b2)
        );
    }

    if(triangles.empty()) return nullptr;
    return std::make_shared<TriangleMesh>(triangles);
}

} // namespace io::gltf
