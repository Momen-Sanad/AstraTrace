#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include <glm/gtc/constants.hpp>

#include "core/image.hpp"
#include "core/image_io.hpp"
#include "io/gltf/gltf_loader.hpp"
#include "render/cpu/cpu_path_renderer.hpp"
#include "render/cpu/cpu_whitted_renderer.hpp"
#include "scene/camera/camera.hpp"
#include "scene/materials/materials.hpp"
#include "scene/world/scene.hpp"

namespace {

std::shared_ptr<SceneObject> findFirstTexturedGlass(const Scene& scene) {
    for(const auto& object : scene.getObjects()) {
        auto glass = std::dynamic_pointer_cast<SmoothGlassMaterial>(object->getMaterial());
        if(glass && glass->surface_detail_strength > 0.0f && glass->base_color) return object;
    }
    return nullptr;
}

Camera makeCloseupCamera(const SceneObject& object, float aspect_ratio) {
    AABB bounds = object.getBounds();
    glm::vec3 center = 0.5f * (bounds.min + bounds.max);
    glm::vec3 size = bounds.max - bounds.min;
    float radius = 0.5f * glm::length(size);
    radius = glm::max(radius, 0.01f);

    Camera camera;
    glm::vec3 offset = glm::normalize(glm::vec3(0.10f, 0.06f, 0.14f)) * (radius * 5.5f);
    camera.setPosition(center + offset);
    camera.setRotation(glm::normalize(center - camera.getPosition()), glm::vec3(0.0f, 1.0f, 0.0f));
    camera.setHalfSize(glm::radians(18.0f), aspect_ratio);
    return camera;
}

bool renderWhitted(const Scene& scene, const Camera& camera, Image<Color8>& image) {
    render::cpu::CpuWhittedRenderer renderer;
    renderer.render(scene, camera, image);
    return true;
}

bool renderPath(const Scene& scene, const Camera& camera, Image<Color8>& image, int samples, bool temporal) {
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = temporal ? render::PathDenoiserMode::Temporal : render::PathDenoiserMode::None;
    settings.max_bounces = 6;
    settings.sampler = render::PathSamplerMode::Sobol;
    settings.light_sampler = render::PathLightSamplerMode::PartialBRDF;
    settings.enable_taa = false;

    render::RenderFrameContext context{};
    int remaining = std::max(1, samples);
    while(remaining > 0) {
        settings.samples_per_frame = std::min(64, remaining);
        context.frame_index++;
        context.settings_changed = context.frame_index == 1;
        renderer.render(scene, camera, image, context, settings);
        remaining -= settings.samples_per_frame;
        if(!temporal) break;
    }
    return true;
}

}

int main(int argc, char** argv) {
    if(argc < 3) {
        std::cerr << "Usage: astratrace_material_closeup_render <scene.gltf|scene.glb> <out.png> "
                     "[whitted|path|path-temporal] [width] [height] [samples]\n";
        return 2;
    }

    const std::string scene_path = argv[1];
    const std::filesystem::path out_path = argv[2];
    const std::string backend = argc >= 4 ? argv[3] : "whitted";
    const int width = argc >= 5 ? std::max(1, std::atoi(argv[4])) : 512;
    const int height = argc >= 6 ? std::max(1, std::atoi(argv[5])) : 512;
    const int samples = argc >= 7 ? std::max(1, std::atoi(argv[6])) : 64;

    Scene scene;
    Camera loaded_camera;
    GltfSceneLoadResult loaded = io::gltf::loadSceneFromGLTF(
        scene_path,
        scene,
        loaded_camera,
        static_cast<float>(width) / static_cast<float>(height)
    );
    if(!loaded.success) {
        std::cerr << "Failed to load scene: " << loaded.error << "\n";
        return 1;
    }
    if(!loaded.warning.empty()) {
        std::cout << "Warnings:\n" << loaded.warning << "\n";
    }
    scene.update();

    auto target = findFirstTexturedGlass(scene);
    if(!target) {
        std::cerr << "No textured glass preview material found.\n";
        return 1;
    }

    Camera camera = makeCloseupCamera(*target, static_cast<float>(width) / static_cast<float>(height));
    Image<Color8> image(width, height);
    if(backend == "whitted") {
        renderWhitted(scene, camera, image);
    } else if(backend == "path") {
        renderPath(scene, camera, image, samples, false);
    } else if(backend == "path-temporal") {
        renderPath(scene, camera, image, samples, true);
    } else {
        std::cerr << "Unknown backend '" << backend << "'. Expected whitted, path, or path-temporal.\n";
        return 2;
    }

    std::string error;
    if(!savePng(image, out_path, error)) {
        std::cerr << "Failed to write PNG: " << error << "\n";
        return 1;
    }

    AABB bounds = target->getBounds();
    glm::vec3 center = 0.5f * (bounds.min + bounds.max);
    std::cout << "Rendered textured glass close-up for object_id=" << target->getID()
              << " center=(" << center.x << "," << center.y << "," << center.z << ")"
              << " to " << out_path.string() << "\n";
    return 0;
}
