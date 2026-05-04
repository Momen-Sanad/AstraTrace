#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>

#include "core/color.hpp"
#include "core/image.hpp"
#include "io/gltf/gltf_loader.hpp"
#include "render/cpu/alias_table.hpp"
#include "render/cpu/cpu_path_renderer.hpp"
#include "render/cpu/sampler.hpp"
#include "scene/camera/camera.hpp"
#include "scene/geometry/triangle.hpp"
#include "scene/materials/pbr_material.hpp"
#include "scene/world/scene.hpp"

namespace {

void require(bool condition, const char* message) {
    if(!condition) throw std::runtime_error(message);
}

bool finiteColor(Color color) {
    return std::isfinite(color.r) && std::isfinite(color.g) && std::isfinite(color.b);
}

void checkSamplers() {
    for(render::PathSamplerMode mode : {
        render::PathSamplerMode::Random,
        render::PathSamplerMode::Halton,
        render::PathSamplerMode::Sobol
    }) {
        render::cpu::Sampler sampler(mode, glm::ivec2(7, 11), 3);
        float first = sampler.next();
        require(first >= 0.0f && first < 1.0f, "sampler produced an out-of-range value");
        bool varied = false;
        for(int i = 0; i < 32; ++i) {
            float value = sampler.next();
            require(value >= 0.0f && value < 1.0f, "sampler produced an out-of-range value");
            varied = varied || glm::abs(value - first) > 1e-6f;
        }
        require(varied, "sampler did not advance dimensions");
    }
}

void checkAliasTable() {
    render::cpu::AliasTable table({0.0f, 1.0f, 3.0f});
    require(table.size() == 3, "alias table size mismatch");
    require(glm::abs(table.pdf(0) - 0.0f) < 1e-6f, "alias table zero-weight pdf mismatch");
    require(glm::abs(table.pdf(1) - 0.25f) < 1e-6f, "alias table medium pdf mismatch");
    require(glm::abs(table.pdf(2) - 0.75f) < 1e-6f, "alias table high pdf mismatch");

    float pdf = 0.0f;
    std::size_t index = table.sample(0.9f, 0.9f, pdf);
    require(index < 3 && pdf > 0.0f, "alias table sampled invalid index/pdf");
}

void checkShapeSampling() {
    Vertex v0{
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 0.0f, 1.0f),
        glm::vec3(1.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f),
        glm::vec2(0.0f, 0.0f)
    };
    Vertex v1 = v0;
    Vertex v2 = v0;
    v1.position = glm::vec3(1.0f, 0.0f, 0.0f);
    v1.uv = glm::vec2(1.0f, 0.0f);
    v2.position = glm::vec3(0.0f, 1.0f, 0.0f);
    v2.uv = glm::vec2(0.0f, 1.0f);

    Triangle triangle(v0, v1, v2);
    require(glm::abs(triangle.surfaceArea() - 0.5f) < 1e-6f, "triangle surface area mismatch");

    glm::vec3 point;
    glm::vec3 normal;
    glm::vec2 uv;
    float pdf = 0.0f;
    triangle.samplePoint(glm::vec3(0.25f, 0.5f, 0.0f), point, normal, uv, pdf);
    require(glm::abs(pdf - 2.0f) < 1e-5f, "triangle area pdf mismatch");
    require(glm::dot(normal, glm::vec3(0.0f, 0.0f, 1.0f)) > 0.999f, "triangle sampled normal mismatch");
}

void checkBSDF() {
    PBRMaterial material;
    material.tint = ColorA(0.7f, 0.2f, 0.1f, 1.0f);
    material.emissive_power = Color(0.0f);

    SurfaceData surface;
    surface.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    surface.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    surface.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    surface.uv = glm::vec2(0.5f);

    auto bsdf = material.sampleBSDF(surface);
    require(static_cast<bool>(bsdf), "PBR material did not create a BSDF");
    glm::vec3 view(0.0f, 0.0f, 1.0f);
    glm::vec3 light = glm::normalize(glm::vec3(0.25f, 0.1f, 1.0f));
    DiffuseSpecular value = bsdf->evaluate(light, view);
    require(finiteColor(value.sum()) && glm::max(value.sum().r, glm::max(value.sum().g, value.sum().b)) > 0.0f, "BSDF evaluation failed");
    require(bsdf->pdf(light, view) > 0.0f, "BSDF pdf failed");
}

void checkRendererHistory() {
    Scene scene;
    scene.setBackgroundColor(Color(0.1f, 0.2f, 0.3f));
    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
    camera.setHalfSize(glm::radians(60.0f), 1.0f);

    Image<Color8> image(4, 4);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::Temporal;
    settings.samples_per_frame = 1;
    render::RenderFrameContext context{.scene_changed = true};
    renderer.render(scene, camera, image, context, settings);

    settings.reset_requested = true;
    context = render::RenderFrameContext{.settings_changed = true};
    renderer.render(scene, camera, image, context, settings);
    require(!renderer.getStatus().empty(), "renderer status was empty after temporal render");
}

void checkRequiredScenes() {
    struct SceneCase {
        const char* path;
        bool requires_emissive_geometry;
    };

    const SceneCase cases[] = {
        {"scenes/cornell-box-1.glb", true},
        {"scenes/cornell-box-2.glb", false},
        {"scenes/sponza.glb", false},
    };

    for(const SceneCase& scene_case : cases) {
        require(std::filesystem::exists(scene_case.path), "required scene file was missing");

        Scene scene;
        Camera camera;
        camera.setHalfSize(glm::radians(90.0f), 1.0f);
        GltfSceneLoadResult load_result = io::gltf::loadSceneFromGLTF(
            scene_case.path,
            scene,
            camera,
            1.0f
        );
        require(load_result.success, "required scene failed to load");
        if(scene_case.requires_emissive_geometry) {
            require(load_result.emissive_object_count > 0, "Cornell box did not expose emissive geometry");
            require(load_result.light_count == 0, "Cornell box emissive scene received fallback punctual lights");
        }

        scene.update();
        require(!scene.getPathLights().empty(), "scene produced no path-light candidates");

        Image<Color8> image(8, 8);
        render::cpu::CpuPathRenderer renderer;
        render::PathRenderSettings settings;
        settings.denoiser = render::PathDenoiserMode::None;
        settings.max_bounces = 2;
        settings.samples_per_frame = 1;
        render::RenderFrameContext context{.scene_changed = true};
        renderer.render(scene, camera, image, context, settings);
    }
}

void writeLightOrientationScene(
    const std::filesystem::path& gltf_path,
    const std::string& light_definition,
    const std::string& light_node_transform = {}
) {
    std::filesystem::create_directories(gltf_path.parent_path());

    const std::filesystem::path bin_path = gltf_path.parent_path() / "light_orientation.bin";
    const float positions[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         0.0f,  1.0f, -3.0f
    };

    {
        std::ofstream bin(bin_path, std::ios::binary);
        require(static_cast<bool>(bin), "failed to create synthetic glTF buffer");
        bin.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    }

    std::string json =
        std::string(R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_lights_punctual"],
  "extensions": {
    "KHR_lights_punctual": {
      "lights": [)") + light_definition + R"(]
    }
  },
  "buffers": [{"uri": "light_orientation.bin", "byteLength": 36}],
  "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": 36}],
  "accessors": [{
    "bufferView": 0,
    "byteOffset": 0,
    "componentType": 5126,
    "count": 3,
    "type": "VEC3",
    "min": [-1.0, -1.0, -3.0],
    "max": [1.0, 1.0, -3.0]
  }],
  "materials": [{
    "pbrMetallicRoughness": {"baseColorFactor": [1.0, 1.0, 1.0, 1.0]}
  }],
  "meshes": [{
    "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
  }],
  "nodes": [
    {"mesh": 0},
    {"extensions": {"KHR_lights_punctual": {"light": 0}})" + light_node_transform + R"(}
  ],
  "scenes": [{"nodes": [0, 1]}],
  "scene": 0
})";

    std::ofstream gltf(gltf_path);
    require(static_cast<bool>(gltf), "failed to create synthetic glTF scene");
    gltf << json;
}

Scene loadSyntheticLightScene(const std::filesystem::path& path) {
    Scene scene;
    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), 1.0f);
    GltfSceneLoadResult result = io::gltf::loadSceneFromGLTF(path.string(), scene, camera, 1.0f);
    require(result.success, "synthetic light-orientation glTF failed to load");
    require(result.light_count == 1, "synthetic glTF did not import exactly one punctual light");
    require(scene.getLights().size() == 1, "synthetic glTF scene had unexpected light count");
    return scene;
}

void checkLoadedLightOrientation() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "astratrace_phase2_light_orientation";

    const std::filesystem::path directional_path = root / "directional_identity.gltf";
    writeLightOrientationScene(
        directional_path,
        R"({"type": "directional", "color": [1.0, 1.0, 1.0], "intensity": 2.0})"
    );
    Scene directional_scene = loadSyntheticLightScene(directional_path);
    LightEvaluation directional_eval = directional_scene.getLights()[0]->evaluate(glm::vec3(0.0f));
    require(
        glm::dot(directional_eval.light_vector, glm::vec3(0.0f, 0.0f, 1.0f)) > 0.999f,
        "identity glTF directional light was inverted"
    );

    const std::filesystem::path rotated_directional_path = root / "directional_rotated_y.gltf";
    writeLightOrientationScene(
        rotated_directional_path,
        R"({"type": "directional", "color": [1.0, 1.0, 1.0], "intensity": 2.0})",
        R"(, "rotation": [0.0, 1.0, 0.0, 0.0])"
    );
    Scene rotated_directional_scene = loadSyntheticLightScene(rotated_directional_path);
    LightEvaluation rotated_eval = rotated_directional_scene.getLights()[0]->evaluate(glm::vec3(0.0f));
    require(
        glm::dot(rotated_eval.light_vector, glm::vec3(0.0f, 0.0f, -1.0f)) > 0.999f,
        "rotated glTF directional light did not follow node rotation"
    );

    const std::filesystem::path spot_path = root / "spot_identity.gltf";
    writeLightOrientationScene(
        spot_path,
        R"({"type": "spot", "color": [1.0, 1.0, 1.0], "intensity": 16.0, "spot": {"innerConeAngle": 0.1, "outerConeAngle": 0.7}})"
    );
    Scene spot_scene = loadSyntheticLightScene(spot_path);
    LightEvaluation front_eval = spot_scene.getLights()[0]->evaluate(glm::vec3(0.0f, 0.0f, -2.0f));
    LightEvaluation back_eval = spot_scene.getLights()[0]->evaluate(glm::vec3(0.0f, 0.0f, 2.0f));
    require(
        glm::max(front_eval.radiance.r, glm::max(front_eval.radiance.g, front_eval.radiance.b)) > 0.0f,
        "identity glTF spot light did not illuminate local -Z"
    );
    require(
        glm::max(back_eval.radiance.r, glm::max(back_eval.radiance.g, back_eval.radiance.b)) == 0.0f,
        "identity glTF spot light illuminated backward"
    );
}

} // namespace

int main() {
    try {
        checkSamplers();
        checkAliasTable();
        checkShapeSampling();
        checkBSDF();
        checkRendererHistory();
        checkRequiredScenes();
        checkLoadedLightOrientation();
    } catch(const std::exception& error) {
        std::cerr << "Phase 2 sanity check failed: " << error.what() << "\n";
        return 1;
    }

    std::cout << "Phase 2 path tracing sanity checks passed.\n";
    return 0;
}
