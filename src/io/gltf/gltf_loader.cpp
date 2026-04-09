/*
App reads the scene path from CLI, then calls glTF loader
Loader picks .glb vs .gltf by extension and uses TinyGLTF LoadBinaryFromFile / LoadASCIIFromFile
builds PBR materials/textures (base color, normal, occlusion, metallic/roughness, emissive, emissive strength)
traverses scene nodes, applies world transforms, imports mesh primitives (triangles only), 
recomputes normals/tangents when missing
imports camera (perspective only) and punctual lights (directional/point/spot)
*/
#include "io/gltf/gltf_loader.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "core/color.hpp"
#include "core/image.hpp"
#include "scene/lights/lights.hpp"
#include "scene/materials/materials.hpp"
#include "scene/geometry/geometry.hpp"

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_JSON
#include <nlohmann/json.hpp>
#include <tiny_gltf.h>

namespace {

constexpr float EPSILON = 1e-8f;

void appendLine(std::string& text, const std::string& line) {
    if(line.empty()) return;
    if(!text.empty()) text += "\n";
    text += line;
}

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string getExtension(const std::string& path) {
    std::size_t dot = path.find_last_of('.');
    if(dot == std::string::npos) return "";
    return toLower(path.substr(dot));
}

glm::mat4 getNodeLocalMatrix(const tinygltf::Node& node) {
    if(node.matrix.size() == 16) {
        glm::mat4 matrix(1.0f);
        for(int col = 0; col < 4; ++col) {
            for(int row = 0; row < 4; ++row) {
                matrix[col][row] = static_cast<float>(node.matrix[col * 4 + row]);
            }
        }
        return matrix;
    }

    glm::vec3 translation(0.0f);
    if(node.translation.size() == 3) {
        translation = glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])
        );
    }

    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    if(node.rotation.size() == 4) {
        rotation = glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2])
        );
    }

    glm::vec3 scale(1.0f);
    if(node.scale.size() == 3) {
        scale = glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])
        );
    }

    return glm::translate(glm::mat4(1.0f), translation)
         * glm::mat4_cast(rotation)
         * glm::scale(glm::mat4(1.0f), scale);
}

void collectNodeWorldTransforms(
    const tinygltf::Model& model,
    int node_index,
    const glm::mat4& parent,
    std::vector<glm::mat4>& world_transforms,
    std::vector<int>& ordered_nodes,
    std::vector<bool>& visited
) {
    if(node_index < 0 || node_index >= static_cast<int>(model.nodes.size())) return;
    if(visited[node_index]) return;
    visited[node_index] = true;

    const tinygltf::Node& node = model.nodes[node_index];
    glm::mat4 world = parent * getNodeLocalMatrix(node);
    world_transforms[node_index] = world;
    ordered_nodes.push_back(node_index);

    for(int child : node.children) {
        collectNodeWorldTransforms(model, child, world, world_transforms, ordered_nodes, visited);
    }
}

bool getAccessorLayout(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    const unsigned char*& base,
    int& stride,
    int& component_count,
    int& component_size
) {
    if(accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(model.bufferViews.size())) return false;
    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    if(view.buffer < 0 || view.buffer >= static_cast<int>(model.buffers.size())) return false;
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    if(buffer.data.empty()) return false;

    component_size = tinygltf::GetComponentSizeInBytes(static_cast<uint32_t>(accessor.componentType));
    component_count = tinygltf::GetNumComponentsInType(static_cast<uint32_t>(accessor.type));
    if(component_size <= 0 || component_count <= 0) return false;

    stride = accessor.ByteStride(view);
    if(stride <= 0) return false;

    std::size_t offset = static_cast<std::size_t>(view.byteOffset + accessor.byteOffset);
    if(offset >= buffer.data.size()) return false;
    base = buffer.data.data() + offset;
    return true;
}

double readComponentValue(const unsigned char* ptr, int component_type, bool normalized) {
    switch(component_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE: {
        int8_t v = *reinterpret_cast<const int8_t*>(ptr);
        if(normalized) return std::max(-1.0, static_cast<double>(v) / 127.0);
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        uint8_t v = *reinterpret_cast<const uint8_t*>(ptr);
        if(normalized) return static_cast<double>(v) / 255.0;
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT: {
        int16_t v = *reinterpret_cast<const int16_t*>(ptr);
        if(normalized) return std::max(-1.0, static_cast<double>(v) / 32767.0);
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t v = *reinterpret_cast<const uint16_t*>(ptr);
        if(normalized) return static_cast<double>(v) / 65535.0;
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        uint32_t v = *reinterpret_cast<const uint32_t*>(ptr);
        if(normalized) return static_cast<double>(v) / 4294967295.0;
        return static_cast<double>(v);
    }
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return static_cast<double>(*reinterpret_cast<const float*>(ptr));
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        return *reinterpret_cast<const double*>(ptr);
    default:
        return 0.0;
    }
}

bool readAccessorElement(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::size_t element_index,
    std::array<double, 4>& out
) {
    const unsigned char* base = nullptr;
    int stride = 0, component_count = 0, component_size = 0;
    if(!getAccessorLayout(model, accessor, base, stride, component_count, component_size)) return false;
    if(element_index >= accessor.count) return false;

    const unsigned char* element_ptr = base + element_index * static_cast<std::size_t>(stride);
    out = {0.0, 0.0, 0.0, 1.0};
    for(int c = 0; c < component_count && c < 4; ++c) {
        out[c] = readComponentValue(
            element_ptr + c * component_size,
            accessor.componentType,
            accessor.normalized
        );
    }
    return true;
}

bool readIndexAccessor(
    const tinygltf::Model& model,
    const tinygltf::Accessor& accessor,
    std::vector<uint32_t>& out_indices
) {
    const unsigned char* base = nullptr;
    int stride = 0, component_count = 0, component_size = 0;
    if(!getAccessorLayout(model, accessor, base, stride, component_count, component_size)) return false;
    if(component_count != 1) return false;

    out_indices.resize(accessor.count);
    for(std::size_t i = 0; i < accessor.count; ++i) {
        const unsigned char* ptr = base + i * static_cast<std::size_t>(stride);
        double value = readComponentValue(ptr, accessor.componentType, false);
        out_indices[i] = value < 0.0 ? 0u : static_cast<uint32_t>(value);
    }
    return true;
}

glm::vec3 chooseOrthogonal(const glm::vec3& normal) {
    if(glm::abs(normal.y) < 0.999f) {
        return glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), normal));
    }
    return glm::normalize(glm::cross(glm::vec3(1.0f, 0.0f, 0.0f), normal));
}

const tinygltf::Image* getTextureImage(const tinygltf::Model& model, int texture_index) {
    if(texture_index < 0 || texture_index >= static_cast<int>(model.textures.size())) return nullptr;
    int image_index = model.textures[texture_index].source;
    if(image_index < 0 || image_index >= static_cast<int>(model.images.size())) return nullptr;
    const tinygltf::Image& image = model.images[image_index];
    if(image.width <= 0 || image.height <= 0 || image.image.empty()) return nullptr;
    return &image;
}

float readImageChannel(const tinygltf::Image& image, int x, int y, int channel) {
    int comp = std::max(1, image.component);
    int clamped_x = std::clamp(x, 0, image.width - 1);
    int clamped_y = std::clamp(y, 0, image.height - 1);
    int clamped_channel = std::clamp(channel, 0, comp - 1);

    // glTF image data + UVs are consumed in the same orientation.
    // Do not vertically flip here.
    std::size_t idx = (static_cast<std::size_t>(clamped_y) * image.width + clamped_x) * comp + clamped_channel;
    if(idx >= image.image.size()) return 0.0f;
    return image.image[idx] / 255.0f;
}

std::shared_ptr<Image<ColorA>> buildBaseColorImage(
    const tinygltf::Image& image,
    bool linearize,
    const std::string& alpha_mode,
    float alpha_cutoff
) {
    auto out = std::make_shared<Image<ColorA>>(image.width, image.height);
    ColorA* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float r = readImageChannel(image, x, y, 0);
            float g = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float b = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);
            float a = image.component > 3 ? readImageChannel(image, x, y, 3) : 1.0f;

            if(linearize) {
                r = convertGammaToLinear(r);
                g = convertGammaToLinear(g);
                b = convertGammaToLinear(b);
            }

            if(alpha_mode == "OPAQUE") {
                a = 1.0f;
            } else if(alpha_mode == "MASK") {
                a = (a >= alpha_cutoff) ? 1.0f : 0.0f;
            }

            pixels[y * image.width + x] = ColorA(r, g, b, a);
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> buildColorImage(const tinygltf::Image& image, bool linearize) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float r = readImageChannel(image, x, y, 0);
            float g = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float b = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);
            if(linearize) {
                r = convertGammaToLinear(r);
                g = convertGammaToLinear(g);
                b = convertGammaToLinear(b);
            }
            pixels[y * image.width + x] = Color(r, g, b);
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> buildNormalImage(const tinygltf::Image& image, float scale) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            glm::vec3 local(
                2.0f * readImageChannel(image, x, y, 0) - 1.0f,
                2.0f * readImageChannel(image, x, y, image.component > 1 ? 1 : 0) - 1.0f,
                2.0f * readImageChannel(image, x, y, image.component > 2 ? 2 : 0) - 1.0f
            );
            local.x *= scale;
            local.y *= scale;
            if(glm::dot(local, local) > EPSILON) local = glm::normalize(local);
            else local = glm::vec3(0.0f, 0.0f, 1.0f);

            pixels[y * image.width + x] = Color(
                0.5f * (local.x + 1.0f),
                0.5f * (local.y + 1.0f),
                0.5f * (local.z + 1.0f)
            );
        }
    }
    return out;
}

std::shared_ptr<Image<float>> buildOcclusionImage(const tinygltf::Image& image, float strength) {
    auto out = std::make_shared<Image<float>>(image.width, image.height);
    float* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float occlusion = readImageChannel(image, x, y, 0);
            pixels[y * image.width + x] = glm::mix(1.0f, occlusion, strength);
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> buildMetalRoughnessImage(
    const tinygltf::Image& image,
    float metallic_factor,
    float roughness_factor
) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float g = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float b = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);
            pixels[y * image.width + x] = Color(
                glm::clamp(b * metallic_factor, 0.0f, 1.0f),
                glm::clamp(g * roughness_factor, 0.0f, 1.0f),
                0.0f
            );
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> makeSolidColorImage(const Color& color) {
    auto out = std::make_shared<Image<Color>>(1, 1);
    out->getPixels()[0] = color;
    return out;
}

float getEmissiveStrength(const tinygltf::Material& material) {
    auto ext_it = material.extensions.find("KHR_materials_emissive_strength");
    if(ext_it == material.extensions.end()) return 1.0f;
    const tinygltf::Value& ext = ext_it->second;
    if(!ext.IsObject()) return 1.0f;
    if(!ext.Has("emissiveStrength")) return 1.0f;
    const tinygltf::Value& value = ext.Get("emissiveStrength");
    if(!value.IsNumber()) return 1.0f;
    return static_cast<float>(value.GetNumberAsDouble());
}

std::shared_ptr<Material> createMaterialFromGLTF(
    const tinygltf::Model& model,
    const tinygltf::Material& material
) {
    auto pbr = std::make_shared<PBRMaterial>();

    const tinygltf::PbrMetallicRoughness& mr = material.pbrMetallicRoughness;
    pbr->tint = ColorA(
        static_cast<float>(mr.baseColorFactor.size() > 0 ? mr.baseColorFactor[0] : 1.0),
        static_cast<float>(mr.baseColorFactor.size() > 1 ? mr.baseColorFactor[1] : 1.0),
        static_cast<float>(mr.baseColorFactor.size() > 2 ? mr.baseColorFactor[2] : 1.0),
        static_cast<float>(mr.baseColorFactor.size() > 3 ? mr.baseColorFactor[3] : 1.0)
    );

    if(const tinygltf::Image* base_color_img = getTextureImage(model, mr.baseColorTexture.index)) {
        pbr->base_color = buildBaseColorImage(
            *base_color_img,
            true,
            material.alphaMode,
            static_cast<float>(material.alphaCutoff)
        );
    } else if(material.alphaMode == "OPAQUE") {
        pbr->tint.a = 1.0f;
    } else if(material.alphaMode == "MASK") {
        pbr->tint.a = (pbr->tint.a >= static_cast<float>(material.alphaCutoff)) ? 1.0f : 0.0f;
    }

    if(const tinygltf::Image* normal_img = getTextureImage(model, material.normalTexture.index)) {
        pbr->normal = buildNormalImage(*normal_img, static_cast<float>(material.normalTexture.scale));
    }

    if(const tinygltf::Image* occlusion_img = getTextureImage(model, material.occlusionTexture.index)) {
        pbr->occlusion = buildOcclusionImage(*occlusion_img, static_cast<float>(material.occlusionTexture.strength));
    }

    float emissive_strength = getEmissiveStrength(material);
    pbr->emissive_power = emissive_strength * Color(
        static_cast<float>(material.emissiveFactor.size() > 0 ? material.emissiveFactor[0] : 0.0),
        static_cast<float>(material.emissiveFactor.size() > 1 ? material.emissiveFactor[1] : 0.0),
        static_cast<float>(material.emissiveFactor.size() > 2 ? material.emissiveFactor[2] : 0.0)
    );
    if(const tinygltf::Image* emissive_img = getTextureImage(model, material.emissiveTexture.index)) {
        pbr->emissive = buildColorImage(*emissive_img, true);
    }

    float metallic_factor = static_cast<float>(mr.metallicFactor);
    float roughness_factor = static_cast<float>(mr.roughnessFactor);
    if(const tinygltf::Image* mr_img = getTextureImage(model, mr.metallicRoughnessTexture.index)) {
        pbr->metal_roughness = buildMetalRoughnessImage(*mr_img, metallic_factor, roughness_factor);
    } else if(glm::abs(metallic_factor) > EPSILON || glm::abs(roughness_factor - 1.0f) > EPSILON) {
        pbr->metal_roughness = makeSolidColorImage(Color(
            glm::clamp(metallic_factor, 0.0f, 1.0f),
            glm::clamp(roughness_factor, 0.0f, 1.0f),
            0.0f
        ));
    }

    return pbr;
}

std::shared_ptr<Material> createDefaultMaterial() {
    auto material = std::make_shared<PBRMaterial>();
    material->tint = ColorA(0.85f, 0.85f, 0.85f, 1.0f);
    material->emissive_power = Color(0.0f);
    return material;
}

bool readAttributeVec3(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t count,
    std::vector<glm::vec3>& out
) {
    if(accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) return false;
    const tinygltf::Accessor& accessor = model.accessors[accessor_index];
    if(accessor.count < count) return false;
    out.resize(count);
    for(std::size_t i = 0; i < count; ++i) {
        std::array<double, 4> v;
        if(!readAccessorElement(model, accessor, i, v)) return false;
        out[i] = glm::vec3(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
    }
    return true;
}

bool readAttributeVec2(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t count,
    std::vector<glm::vec2>& out
) {
    if(accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) return false;
    const tinygltf::Accessor& accessor = model.accessors[accessor_index];
    if(accessor.count < count) return false;
    out.resize(count);
    for(std::size_t i = 0; i < count; ++i) {
        std::array<double, 4> v;
        if(!readAccessorElement(model, accessor, i, v)) return false;
        out[i] = glm::vec2(static_cast<float>(v[0]), static_cast<float>(v[1]));
    }
    return true;
}

bool readAttributeVec4(
    const tinygltf::Model& model,
    int accessor_index,
    std::size_t count,
    std::vector<glm::vec4>& out
) {
    if(accessor_index < 0 || accessor_index >= static_cast<int>(model.accessors.size())) return false;
    const tinygltf::Accessor& accessor = model.accessors[accessor_index];
    if(accessor.count < count) return false;
    out.resize(count);
    for(std::size_t i = 0; i < count; ++i) {
        std::array<double, 4> v;
        if(!readAccessorElement(model, accessor, i, v)) return false;
        out[i] = glm::vec4(
            static_cast<float>(v[0]),
            static_cast<float>(v[1]),
            static_cast<float>(v[2]),
            static_cast<float>(v[3])
        );
    }
    return true;
}

std::shared_ptr<TriangleMesh> buildMeshFromPrimitive(
    const tinygltf::Model& model,
    const tinygltf::Primitive& primitive,
    const glm::mat4& world_transform,
    std::string& warnings
) {
    int mode = (primitive.mode < 0) ? TINYGLTF_MODE_TRIANGLES : primitive.mode;
    if(mode != TINYGLTF_MODE_TRIANGLES) {
        appendLine(warnings, "Skipped a primitive with non-triangle topology.");
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
            if(glm::dot(n, n) > EPSILON) {
                n = glm::normalize(n);
                normals[i0] += n;
                normals[i1] += n;
                normals[i2] += n;
            }
        }
        for(glm::vec3& n : normals) {
            if(glm::dot(n, n) > EPSILON) n = glm::normalize(n);
            else n = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    } else {
        for(glm::vec3& n : normals) {
            if(glm::dot(n, n) > EPSILON) n = glm::normalize(n);
            else n = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }

    std::vector<glm::vec3> tangents(vertex_count, glm::vec3(0.0f));
    std::vector<glm::vec3> bitangents(vertex_count, glm::vec3(0.0f));
    if(has_tangents) {
        for(std::size_t i = 0; i < vertex_count; ++i) {
            glm::vec3 t = glm::vec3(tangent4[i]);
            if(glm::dot(t, t) <= EPSILON) t = chooseOrthogonal(normals[i]);
            else t = glm::normalize(t);
            glm::vec3 b = glm::normalize(glm::cross(normals[i], t));
            if(tangent4[i].w < 0.0f) b *= -1.0f;
            tangents[i] = t;
            bitangents[i] = b;
        }
    } else {
        std::vector<glm::vec3> tan_accum(vertex_count, glm::vec3(0.0f));
        std::vector<glm::vec3> bitan_accum(vertex_count, glm::vec3(0.0f));

        if(has_uv) {
            for(std::size_t i = 0; i + 2 < indices.size(); i += 3) {
                uint32_t i0 = indices[i + 0];
                uint32_t i1 = indices[i + 1];
                uint32_t i2 = indices[i + 2];
                if(i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) continue;

                const glm::vec3& p0 = positions[i0];
                const glm::vec3& p1 = positions[i1];
                const glm::vec3& p2 = positions[i2];
                const glm::vec2& uv0 = uvs[i0];
                const glm::vec2& uv1 = uvs[i1];
                const glm::vec2& uv2 = uvs[i2];

                glm::vec3 edge1 = p1 - p0;
                glm::vec3 edge2 = p2 - p0;
                glm::vec2 duv1 = uv1 - uv0;
                glm::vec2 duv2 = uv2 - uv0;

                float det = duv1.x * duv2.y - duv1.y * duv2.x;
                if(glm::abs(det) <= EPSILON) continue;
                float inv_det = 1.0f / det;

                glm::vec3 tangent = inv_det * (duv2.y * edge1 - duv1.y * edge2);
                glm::vec3 bitangent = inv_det * (-duv2.x * edge1 + duv1.x * edge2);
                tan_accum[i0] += tangent;
                tan_accum[i1] += tangent;
                tan_accum[i2] += tangent;
                bitan_accum[i0] += bitangent;
                bitan_accum[i1] += bitangent;
                bitan_accum[i2] += bitangent;
            }
        }

        for(std::size_t i = 0; i < vertex_count; ++i) {
            glm::vec3 n = normals[i];
            glm::vec3 t = tan_accum[i];
            if(glm::dot(t, t) <= EPSILON) {
                t = chooseOrthogonal(n);
            } else {
                t = glm::normalize(t - n * glm::dot(n, t));
            }
            glm::vec3 b = glm::normalize(glm::cross(n, t));
            if(has_uv && glm::dot(b, bitan_accum[i]) < 0.0f) b *= -1.0f;
            tangents[i] = t;
            bitangents[i] = b;
        }
    }

    glm::mat3 world3 = glm::mat3(world_transform);
    glm::mat3 normal_matrix = glm::mat3(glm::transpose(glm::inverse(world_transform)));

    std::vector<Vertex> transformed_vertices(vertex_count);
    for(std::size_t i = 0; i < vertex_count; ++i) {
        glm::vec3 position = glm::vec3(world_transform * glm::vec4(positions[i], 1.0f));
        glm::vec3 normal = glm::normalize(normal_matrix * normals[i]);
        glm::vec3 tangent = glm::normalize(world3 * tangents[i]);
        tangent = glm::normalize(tangent - normal * glm::dot(normal, tangent));
        glm::vec3 bitangent = glm::normalize(glm::cross(normal, tangent));
        if(glm::dot(bitangent, world3 * bitangents[i]) < 0.0f) bitangent *= -1.0f;

        transformed_vertices[i] = Vertex{
            position,
            normal,
            tangent,
            bitangent,
            uvs[i]
        };
    }

    std::vector<Triangle> triangles;
    triangles.reserve(indices.size() / 3);
    for(std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        uint32_t i0 = indices[i + 0];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        if(i0 >= vertex_count || i1 >= vertex_count || i2 >= vertex_count) continue;
        triangles.emplace_back(transformed_vertices[i0], transformed_vertices[i1], transformed_vertices[i2]);
    }

    if(triangles.empty()) return nullptr;
    return std::make_shared<TriangleMesh>(std::span<Triangle>(triangles.data(), triangles.size()));
}

bool applyCameraFromNode(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    const glm::mat4& world_transform,
    Camera& camera,
    float fallback_aspect_ratio
) {
    if(node.camera < 0 || node.camera >= static_cast<int>(model.cameras.size())) return false;
    const tinygltf::Camera& gltf_camera = model.cameras[node.camera];
    if(gltf_camera.type != "perspective") return false;

    float fov_y = static_cast<float>(gltf_camera.perspective.yfov);
    if(fov_y <= 0.0f) fov_y = glm::radians(60.0f);
    float aspect = static_cast<float>(gltf_camera.perspective.aspectRatio);
    if(aspect <= 0.0f) aspect = fallback_aspect_ratio;

    glm::vec3 position = glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    glm::vec3 forward = glm::normalize(glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
    glm::vec3 up = glm::normalize(glm::vec3(world_transform * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
    if(glm::dot(forward, forward) <= EPSILON || glm::dot(up, up) <= EPSILON) return false;

    camera.setPosition(position);
    camera.setRotation(forward, up);
    camera.setHalfSize(fov_y, aspect);
    return true;
}

std::shared_ptr<Light> createLightFromNode(
    const tinygltf::Model& model,
    const tinygltf::Node& node,
    const glm::mat4& world_transform
) {
    if(node.light < 0 || node.light >= static_cast<int>(model.lights.size())) return nullptr;
    const tinygltf::Light& gltf_light = model.lights[node.light];

    Color color(1.0f);
    if(gltf_light.color.size() >= 3) {
        color = Color(
            static_cast<float>(gltf_light.color[0]),
            static_cast<float>(gltf_light.color[1]),
            static_cast<float>(gltf_light.color[2])
        );
    }
    color *= static_cast<float>(gltf_light.intensity);

    if(gltf_light.type == "directional") {
        glm::vec3 direction = glm::normalize(glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
        return std::make_shared<DirectionLight>(direction, color);
    }

    if(gltf_light.type == "point") {
        glm::vec3 position = glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        return std::make_shared<PointLight>(position, color);
    }

    if(gltf_light.type == "spot") {
        glm::vec3 position = glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 direction = glm::normalize(glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
        return std::make_shared<SpotLight>(
            position,
            direction,
            static_cast<float>(gltf_light.spot.innerConeAngle),
            static_cast<float>(gltf_light.spot.outerConeAngle),
            color
        );
    }

    return nullptr;
}

} // namespace

GltfSceneLoadResult io::gltf::loadSceneFromGLTF(
    const std::string& file_path,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
) {
    GltfSceneLoadResult result;
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;

    std::string warnings, errors;
    std::string extension = getExtension(file_path);
    bool loaded = false;
    if(extension == ".glb") {
        loaded = loader.LoadBinaryFromFile(&model, &errors, &warnings, file_path);
    } else {
        loaded = loader.LoadASCIIFromFile(&model, &errors, &warnings, file_path);
    }

    result.warning = warnings;
    if(!loaded) {
        result.error = errors.empty() ? "Failed to load glTF file." : errors;
        return result;
    }

    std::vector<std::shared_ptr<Material>> materials;
    materials.reserve(std::max<std::size_t>(1, model.materials.size()));
    if(model.materials.empty()) {
        materials.push_back(createDefaultMaterial());
    } else {
        for(const tinygltf::Material& material : model.materials) {
            materials.push_back(createMaterialFromGLTF(model, material));
        }
    }
    std::shared_ptr<Material> default_material = createDefaultMaterial();

    std::vector<int> root_nodes;
    if(model.defaultScene >= 0 && model.defaultScene < static_cast<int>(model.scenes.size())) {
        root_nodes = model.scenes[model.defaultScene].nodes;
    } else if(!model.scenes.empty()) {
        root_nodes = model.scenes[0].nodes;
    } else {
        for(int i = 0; i < static_cast<int>(model.nodes.size()); ++i) root_nodes.push_back(i);
    }

    std::vector<glm::mat4> world_transforms(model.nodes.size(), glm::mat4(1.0f));
    std::vector<int> ordered_nodes;
    std::vector<bool> visited(model.nodes.size(), false);
    for(int node_index : root_nodes) {
        collectNodeWorldTransforms(
            model, node_index, glm::mat4(1.0f), world_transforms, ordered_nodes, visited
        );
    }

    for(int node_index : ordered_nodes) {
        const tinygltf::Node& node = model.nodes[node_index];
        const glm::mat4& world_transform = world_transforms[node_index];

        if(!result.camera_loaded) {
            result.camera_loaded = applyCameraFromNode(
                model, node, world_transform, camera, fallback_aspect_ratio
            );
        }

        if(node.mesh >= 0 && node.mesh < static_cast<int>(model.meshes.size())) {
            const tinygltf::Mesh& mesh = model.meshes[node.mesh];
            for(const tinygltf::Primitive& primitive : mesh.primitives) {
                std::shared_ptr<TriangleMesh> shape = buildMeshFromPrimitive(
                    model, primitive, world_transform, result.warning
                );
                if(!shape) continue;

                std::shared_ptr<Material> material = default_material;
                if(primitive.material >= 0 && primitive.material < static_cast<int>(materials.size())) {
                    material = materials[primitive.material];
                } else if(!materials.empty()) {
                    material = materials[0];
                }

                scene.createObject(shape, material);
                result.object_count++;
            }
        }

        if(std::shared_ptr<Light> light = createLightFromNode(model, node, world_transform)) {
            scene.addLight(light);
            result.light_count++;
        }
    }

    if(result.light_count == 0) {
        // Many sample glTF scenes ship without punctual lights and assume IBL.
        // Always use full-quality fallback key + fill lighting.
        scene.addLight(std::make_shared<DirectionLight>(
            glm::normalize(glm::vec3(-0.5f, -1.0f, -0.35f)),
            Color(2.8f)
        ));
        scene.addLight(std::make_shared<DirectionLight>(
            glm::normalize(glm::vec3(0.45f, -0.35f, 0.85f)),
            Color(0.85f)
        ));
        result.light_count += 2;
        appendLine(
            result.warning,
            "No glTF lights found. Added fallback key/fill directional lights for visibility."
        );
        scene.setAmbient(Color(0.16f));
        scene.setBackgroundColor(Color(0.07f));
    } else {
        scene.setAmbient(Color(0.02f));
        scene.setBackgroundColor(Color(0.03f));
    }

    if(result.object_count == 0) {
        result.error = "glTF loaded, but no supported triangle primitives were imported.";
        return result;
    }

    result.success = true;
    return result;
}
