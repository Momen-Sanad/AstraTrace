#include "io/gltf/gltf_scene_builder.hpp"

#include <algorithm>
#include <memory>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#include "io/gltf/gltf_material_converter.hpp"
#include "io/gltf/gltf_mesh_builder.hpp"
#include "io/gltf/gltf_utils.hpp"
#include "scene/lights/lights.hpp"

namespace io::gltf {
namespace {

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

glm::vec3 transformDirection(const glm::mat4& matrix, glm::vec3 direction) {
    glm::vec3 transformed = glm::vec3(matrix * glm::vec4(direction, 0.0f));
    if(glm::dot(transformed, transformed) <= GLTF_EPSILON) return glm::normalize(direction);
    return glm::normalize(transformed);
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

    glm::vec3 position = glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
    glm::vec3 forward = transformDirection(world_transform, glm::vec3(0.0f, 0.0f, -1.0f));
    glm::vec3 up = transformDirection(world_transform, glm::vec3(0.0f, 1.0f, 0.0f));
    float yfov = static_cast<float>(gltf_camera.perspective.yfov);
    if(yfov <= 0.0f) yfov = glm::radians(50.0f);
    float aspect = static_cast<float>(gltf_camera.perspective.aspectRatio);
    if(aspect <= 0.0f) aspect = fallback_aspect_ratio;

    camera.setPosition(position);
    camera.setRotation(forward, up);
    camera.setHalfSize(yfov, aspect);
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
        glm::vec3 direction = transformDirection(world_transform, glm::vec3(0.0f, 0.0f, -1.0f));
        return std::make_shared<DirectionLight>(direction, color);
    }

    if(gltf_light.type == "point") {
        glm::vec3 position = glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        return std::make_shared<PointLight>(position, color);
    }

    if(gltf_light.type == "spot") {
        glm::vec3 position = glm::vec3(world_transform * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));
        glm::vec3 direction = transformDirection(world_transform, glm::vec3(0.0f, 0.0f, -1.0f));
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

GltfSceneLoadResult buildSceneFromGltfModel(
    const tinygltf::Model& model,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
) {
    GltfSceneLoadResult result;

    std::vector<std::shared_ptr<Material>> materials;
    materials.reserve(std::max<std::size_t>(1, model.materials.size()));
    if(model.materials.empty()) {
        materials.push_back(createDefaultMaterial());
    } else {
        for(const tinygltf::Material& material : model.materials) {
            materials.push_back(createMaterialFromGLTF(model, material, result.warning));
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
                if(
                    material &&
                    maxChannel(material->getAverageEmissivePower()) > 0.0f &&
                    shape->surfaceArea() > 0.0f
                ) {
                    result.emissive_object_count++;
                }
            }
        }

        if(std::shared_ptr<Light> light = createLightFromNode(model, node, world_transform)) {
            scene.addLight(light);
            result.light_count++;
        }
    }

    bool has_transmissive_material = false;
    bool has_authored_emissive_material = false;
    for(const tinygltf::Material& material : model.materials) {
        if(getMaterialExtensionNumber(material, "KHR_materials_transmission", "transmissionFactor", 0.0f) > 0.0f) {
            has_transmissive_material = true;
        }
        if(hasAuthoredEmission(material)) {
            has_authored_emissive_material = true;
        }
    }

    if(result.light_count == 0 && !has_authored_emissive_material) {
        // Keep no-light asset previews readable with real shadow-casting lights.
        // Ambient is intentionally tiny: CPU Path ignores it, and Whitted only uses it as a preview lift.
        const float key_strength = has_transmissive_material ? 2.75f : 1.55f;
        const float fill_strength = has_transmissive_material ? 0.95f : 0.72f;
        const float ambient_strength = has_transmissive_material ? 0.16f : 0.08f;
        const float background_strength = has_transmissive_material ? 0.28f : 0.10f;
        scene.addLight(std::make_shared<DirectionLight>(
            glm::normalize(glm::vec3(-0.5f, -1.0f, -0.35f)),
            Color(key_strength)
        ));
        scene.addLight(std::make_shared<DirectionLight>(
            glm::normalize(glm::vec3(0.45f, -0.35f, 0.85f)),
            Color(fill_strength)
        ));
        result.light_count += 2;
        appendLine(
            result.warning,
            "No glTF lights found. Added soft fallback key/fill lights and ambient fill for visibility."
        );
        scene.setAmbient(Color(ambient_strength));
        scene.setBackgroundColor(Color(background_strength));
    } else if(result.light_count == 0) {
        if(!result.camera_loaded) {
            scene.addLight(std::make_shared<DirectionLight>(
                glm::normalize(glm::vec3(-0.35f, -1.0f, -0.45f)),
                Color(1.9f)
            ));
            scene.addLight(std::make_shared<DirectionLight>(
                glm::normalize(glm::vec3(0.45f, -0.35f, 0.85f)),
                Color(0.65f)
            ));
            result.light_count += 2;
            appendLine(
                result.warning,
                "No glTF punctual lights or camera found. Using emissive geometry plus preview key/fill lights for visibility."
            );
            scene.setAmbient(Color(0.24f));
        } else {
            appendLine(
                result.warning,
                "No glTF punctual lights found. Using emissive geometry as physical area lights."
            );
            scene.setAmbient(Color(0.06f));
        }
        scene.setBackgroundColor(Color(0.0f));
    } else {
        scene.setAmbient(Color(0.08f));
        scene.setBackgroundColor(Color(0.03f));
    }

    if(result.object_count == 0) {
        result.error = "glTF loaded, but no supported triangle primitives were imported.";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace io::gltf
