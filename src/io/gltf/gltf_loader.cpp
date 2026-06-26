#include "io/gltf/gltf_loader.hpp"

#include "io/gltf/gltf_parser.hpp"
#include "io/gltf/gltf_scene_builder.hpp"

GltfSceneLoadResult io::gltf::loadSceneFromGLTF(
    const std::string& file_path,
    Scene& scene,
    Camera& camera,
    float fallback_aspect_ratio
) {
    tinygltf::Model model;
    std::string warnings;
    std::string errors;

    if(!parseModelFromFile(file_path, model, warnings, errors)) {
        GltfSceneLoadResult result;
        result.warning = warnings;
        result.error = errors.empty() ? "Failed to load glTF file." : errors;
        return result;
    }

    GltfSceneLoadResult result = buildSceneFromGltfModel(
        model,
        scene,
        camera,
        fallback_aspect_ratio
    );
    if(!warnings.empty()) {
        result.warning = result.warning.empty() ? warnings : warnings + "\n" + result.warning;
    }
    return result;
}
