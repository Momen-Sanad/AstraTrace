#include <cmath>
#include <atomic>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>

#include "core/color.hpp"
#include "core/image.hpp"
#include "core/image_io.hpp"
#include "io/gltf/gltf_loader.hpp"
#include "render/cpu/alias_table.hpp"
#include "render/cpu/cpu_path_renderer.hpp"
#include "render/cpu/sampler.hpp"
#include "scene/camera/camera.hpp"
#include "scene/geometry/triangle.hpp"
#include "scene/materials/materials.hpp"
#include "scene/world/scene.hpp"
#include "scene/world/scene_showcase.hpp"

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

void checkPathNoOpExportAccumulation() {
    Scene scene;
    scene.setBackgroundColor(Color(0.2f));
    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
    camera.setHalfSize(glm::radians(60.0f), 1.0f);

    Image<Color8> image(2, 2);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::None;
    settings.accumulate_samples = true;
    settings.samples_per_frame = 64;
    renderer.render(scene, camera, image, render::RenderFrameContext{.scene_changed = true}, settings);

    settings.samples_per_frame = 1;
    renderer.render(scene, camera, image, render::RenderFrameContext{.frame_index = 1}, settings);
    require(
        renderer.getStatus().find("65 accumulated") != std::string::npos,
        "NoOp export accumulation did not count all sample batches"
    );
}

void checkTransformedSceneObject() {
    Scene scene;
    auto material = std::make_shared<PBRMaterial>();
    material->emissive_power = Color(1.0f);
    auto object = scene.createObject(std::make_shared<Sphere>(glm::vec3(0.0f), 1.0f), material);
    object->setPosition(glm::vec3(0.0f, 0.0f, -5.0f));
    object->setScale(glm::vec3(2.0f));
    scene.update();

    Ray ray{glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    RayHit hit;
    auto hit_object = scene.findClosestHit(ray, hit);
    require(hit_object == object, "scaled transformed sphere was not hit");
    require(glm::abs(hit.distance - 3.0f) < 1e-4f, "scaled transformed sphere hit distance was wrong");

    SurfaceData surface = object->getSurfaceData(ray, hit);
    require(glm::abs(glm::dot(surface.normal, glm::vec3(0.0f, 0.0f, 1.0f))) > 0.99f, "transformed sphere surface normal was wrong");

    const float expected_pdf = 9.0f / (16.0f * glm::pi<float>());
    require(glm::abs(object->pdf(ray, hit) - expected_pdf) < 1e-4f, "transformed emissive PDF ignored world area scale");
    require(object->power() > 4.0f * glm::pi<float>() * glm::pi<float>(), "transformed emissive power ignored area scale");
}

void checkBvhTraversalEquivalence() {
    Scene scene;
    auto material = std::make_shared<PBRMaterial>();
    for(int i = 0; i < 9; ++i) {
        float x = static_cast<float>((i % 3) - 1) * 1.5f;
        float y = static_cast<float>((i / 3) - 1) * 1.5f;
        scene.createObject(std::make_shared<Sphere>(glm::vec3(x, y, -6.0f), 0.45f), material);
    }
    scene.update();
    require(scene.getStats().top_level_bvh_node_count > 0, "BVH equivalence scene did not build TLAS");

    for(int y = -3; y <= 3; ++y) {
        for(int x = -3; x <= 3; ++x) {
            Ray ray{
                glm::vec3(0.0f),
                glm::normalize(glm::vec3(0.12f * x, 0.12f * y, -1.0f))
            };

            RayHit bvh_hit;
            auto bvh_object = scene.findClosestHit(ray, bvh_hit);

            float linear_distance = std::numeric_limits<float>::max();
            std::shared_ptr<SceneObject> linear_object;
            RayHit linear_hit;
            for(const auto& object : scene.getObjects()) {
                RayHit object_hit;
                object_hit.distance = linear_distance;
                if(object->intersect(ray, object_hit) && object_hit.distance < linear_distance) {
                    linear_distance = object_hit.distance;
                    linear_hit = object_hit;
                    linear_object = object;
                }
            }

            require(bvh_object == linear_object, "BVH closest-hit object differed from linear traversal");
            if(bvh_object) {
                require(glm::abs(bvh_hit.distance - linear_hit.distance) < 1e-4f, "BVH closest-hit distance differed from linear traversal");
            }
        }
    }
}

void checkTlasRefitAfterMovement() {
    Scene scene;
    scene.setTopLevelBVHRefitEnabled(true);
    auto material = std::make_shared<PBRMaterial>();
    std::vector<std::shared_ptr<SceneObject>> objects;
    for(int i = 0; i < 8; ++i) {
        objects.push_back(scene.createObject(
            std::make_shared<Sphere>(glm::vec3(0.0f), 0.35f),
            material,
            glm::vec3(static_cast<float>(i) - 3.5f, 0.0f, -5.0f)
        ));
    }
    scene.update();
    SceneStats before = scene.getStats();
    require(before.top_level_bvh_node_count > 0, "TLAS refit scene did not build a BVH");

    objects[0]->setPosition(glm::vec3(0.0f, 1.0f, -3.0f));
    scene.update();
    SceneStats after = scene.getStats();
    require(after.top_level_bvh_refit_count > before.top_level_bvh_refit_count, "TLAS refit did not record a refit");
    require(after.top_level_bvh_rebuild_count == before.top_level_bvh_rebuild_count, "TLAS refit unexpectedly rebuilt the tree");

    Ray ray{glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)};
    RayHit bvh_hit;
    auto bvh_object = scene.findClosestHit(ray, bvh_hit);

    float linear_distance = std::numeric_limits<float>::max();
    std::shared_ptr<SceneObject> linear_object;
    RayHit linear_hit;
    for(const auto& object : scene.getObjects()) {
        RayHit object_hit;
        object_hit.distance = linear_distance;
        if(object->intersect(ray, object_hit) && object_hit.distance < linear_distance) {
            linear_distance = object_hit.distance;
            linear_hit = object_hit;
            linear_object = object;
        }
    }
    require(bvh_object == linear_object, "TLAS refit closest-hit object differed from linear traversal");
    require(glm::abs(bvh_hit.distance - linear_hit.distance) < 1e-4f, "TLAS refit closest-hit distance differed from linear traversal");
}

void checkEnvironmentSampling() {
    Scene scene;
    scene.setBackgroundColor(Color(0.1f, 0.2f, 0.3f));
    glm::vec3 test_direction = glm::normalize(glm::vec3(1.0f, 0.5f, -0.25f));
    Color constant = scene.evaluateEnvironment(test_direction);
    require(glm::length(constant - Color(0.1f, 0.2f, 0.3f)) < 1e-6f, "constant environment did not preserve background color");
    const float full_sphere_pdf = 1.0f / (4.0f * std::acos(-1.0f));
    require(glm::abs(scene.environmentPdf(test_direction) - full_sphere_pdf) < 1e-6f, "constant environment pdf mismatch");

    auto image = std::make_shared<Image<Color>>(2, 2);
    image->getPixels()[0] = Color(0.1f, 0.1f, 0.1f);
    image->getPixels()[1] = Color(2.0f, 0.5f, 0.25f);
    image->getPixels()[2] = Color(0.25f, 1.2f, 0.4f);
    image->getPixels()[3] = Color(0.3f, 0.4f, 1.8f);
    scene.setEnvironmentImage(image, 1.5f);
    require(scene.hasImageEnvironment(), "image environment was not installed");
    EnvironmentSample sample = scene.sampleEnvironment(glm::vec3(0.72f, 0.35f, 0.61f));
    require(sample.pdf > 0.0f, "image environment sample pdf was zero");
    require(finiteColor(sample.radiance), "image environment sample radiance was not finite");
    require(scene.environmentPdf(sample.direction) > 0.0f, "image environment direction pdf was zero");

    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
    camera.setHalfSize(glm::radians(60.0f), 1.0f);
    Image<Color8> output(4, 4);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::None;
    settings.samples_per_frame = 1;
    renderer.render(scene, camera, output, render::RenderFrameContext{.scene_changed = true}, settings);
    bool wrote_color = false;
    for(int i = 0; i < output.getWidth() * output.getHeight(); ++i) {
        Color8 pixel = output.getPixels()[i];
        wrote_color = wrote_color || pixel.r > 0 || pixel.g > 0 || pixel.b > 0;
    }
    require(wrote_color, "image environment path render produced only black pixels");
}

void checkTileProgressAndDeterminism() {
    Scene scene;
    scene.setBackgroundColor(Color(0.2f, 0.25f, 0.3f));
    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
    camera.setHalfSize(glm::radians(60.0f), 1.0f);

    Image<Color8> a(5, 3);
    Image<Color8> b(5, 3);
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::None;
    settings.samples_per_frame = 3;
    settings.sampler = render::PathSamplerMode::Sobol;

    render::cpu::CpuPathRenderer renderer_a;
    render::cpu::CpuPathRenderer renderer_b;
    renderer_a.render(scene, camera, a, render::RenderFrameContext{.scene_changed = true}, settings);
    renderer_b.render(scene, camera, b, render::RenderFrameContext{.scene_changed = true}, settings);

    for(int i = 0; i < a.getWidth() * a.getHeight(); ++i) {
        require(a.getPixels()[i] == b.getPixels()[i], "tiled CPU path render was not deterministic");
    }
    render::RenderProgress progress = renderer_a.getProgress();
    require(progress.tile_count > 0 && progress.width == 5 && progress.height == 3, "renderer progress did not report tile dimensions");
    require(renderer_a.getStatus().find("tiles") != std::string::npos, "renderer status did not report tiles");
}

void checkCpuPathCancellation() {
    Scene scene;
    scene.setBackgroundColor(Color(0.2f));
    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
    camera.setHalfSize(glm::radians(60.0f), 1.0f);

    Image<Color8> image(4, 4);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::None;
    settings.accumulate_samples = true;
    settings.samples_per_frame = 4;
    std::atomic_bool cancel_requested(true);
    render::RenderFrameContext context{
        .scene_changed = true,
        .cancel_requested = &cancel_requested
    };
    renderer.render(scene, camera, image, context, settings);
    render::RenderProgress progress = renderer.getProgress();
    require(progress.canceled, "CPU path renderer did not report cancellation");
    require(progress.accumulated_samples == 0, "CPU path renderer accumulated a canceled frame");
    require(renderer.getStatus().find("canceled") != std::string::npos, "CPU path renderer status did not mention cancellation");
}

void checkSvgfPreviewFinite() {
    Scene scene;
    scene.setBackgroundColor(Color(0.05f, 0.08f, 0.12f));
    auto material = std::make_shared<PBRMaterial>();
    material->tint = ColorA(0.8f, 0.7f, 0.4f, 1.0f);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(0.0f, 0.0f, -3.0f), 0.75f), material);
    scene.update();

    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    camera.setHalfSize(glm::radians(60.0f), 1.0f);
    Image<Color8> image(6, 6);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::SVGF;
    settings.samples_per_frame = 1;
    settings.max_bounces = 2;
    renderer.render(scene, camera, image, render::RenderFrameContext{.scene_changed = true}, settings);
    renderer.render(scene, camera, image, render::RenderFrameContext{.frame_index = 1}, settings);
    camera.setPosition(glm::vec3(0.02f, 0.0f, 0.0f));
    renderer.render(scene, camera, image, render::RenderFrameContext{.frame_index = 2, .camera_changed = true}, settings);

    bool wrote_color = false;
    for(int i = 0; i < image.getWidth() * image.getHeight(); ++i) {
        Color8 pixel = image.getPixels()[i];
        wrote_color = wrote_color || pixel.r > 0 || pixel.g > 0 || pixel.b > 0;
    }
    require(wrote_color, "SVGF-style preview produced only black pixels");
    require(renderer.getProgress().accumulated_samples > 0, "SVGF-style preview did not update progress");
}

void checkOIDNUnavailableBehavior() {
    if(render::isOIDNAvailable()) return;

    Scene scene;
    scene.setBackgroundColor(Color(0.1f));
    Camera camera;
    camera.setPosition(glm::vec3(0.0f, 0.0f, 2.0f));
    camera.setHalfSize(glm::radians(60.0f), 1.0f);
    Image<Color8> image(2, 2);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::OIDN;
    renderer.render(scene, camera, image, render::RenderFrameContext{.scene_changed = true}, settings);
    require(
        renderer.getStatus().find("OIDN not available") != std::string::npos,
        "OIDN disabled build did not report unavailable denoiser"
    );
}

void checkMaterialShowcase() {
    Scene scene;
    Camera camera;
    buildMaterialShowcaseScene(scene, camera, 1.0f);

    SceneStats stats = scene.getStats();
    require(stats.object_count >= 7, "material showcase did not create the expected objects");
    require(stats.path_light_count > 0, "material showcase produced no path-light candidates");
    require(stats.top_level_bvh_node_count > 0, "material showcase did not build a TLAS");
    require(stats.top_level_bvh_leaf_count > 0, "material showcase TLAS had no leaves");

    Image<Color8> image(8, 8);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::None;
    settings.max_bounces = 2;
    settings.samples_per_frame = 1;
    render::RenderFrameContext context{.scene_changed = true};
    renderer.render(scene, camera, image, context, settings);

    bool wrote_color = false;
    for(int i = 0; i < image.getWidth() * image.getHeight(); ++i) {
        Color8 pixel = image.getPixels()[i];
        wrote_color = wrote_color || pixel.r > 0 || pixel.g > 0 || pixel.b > 0;
    }
    require(wrote_color, "material showcase render produced only black pixels");
}

void writeTransmissionScene(
    const std::filesystem::path& gltf_path,
    float roughness,
    float metallic,
    float transmission,
    bool include_volume = false
) {
    std::filesystem::create_directories(gltf_path.parent_path());

    const std::filesystem::path bin_path = gltf_path.parent_path() / "transmission_triangle.bin";
    const float positions[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         0.0f,  1.0f, -3.0f
    };

    {
        std::ofstream bin(bin_path, std::ios::binary);
        require(static_cast<bool>(bin), "failed to create synthetic transmission buffer");
        bin.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    }

    std::ofstream gltf(gltf_path);
    require(static_cast<bool>(gltf), "failed to create synthetic transmission glTF");
    gltf
        << R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_transmission", "KHR_materials_ior")"
        << (include_volume ? R"(, "KHR_materials_volume")" : "")
        << R"(],
  "buffers": [{"uri": "transmission_triangle.bin", "byteLength": 36}],
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
    "pbrMetallicRoughness": {
      "baseColorFactor": [0.8, 0.92, 1.0, 0.35],
      "metallicFactor": )" << metallic << R"(,
      "roughnessFactor": )" << roughness << R"(
    },
    "extensions": {
      "KHR_materials_transmission": {"transmissionFactor": )" << transmission << R"(},
      "KHR_materials_ior": {"ior": 1.45})"
        << (include_volume
            ? R"(,
      "KHR_materials_volume": {"attenuationColor": [0.8, 0.9, 1.0], "thicknessFactor": 0.22})"
            : "")
        << R"(
    }
  }],
  "meshes": [{
    "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
  }],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
}

void writeTexturedTransmissionScene(const std::filesystem::path& gltf_path) {
    std::filesystem::create_directories(gltf_path.parent_path());

    Image<Color8> orm(2, 2);
    orm.getPixels()[0] = Color8(255,  32, 0, 255);
    orm.getPixels()[1] = Color8( 64, 224, 0, 255);
    orm.getPixels()[2] = Color8(180,  96, 0, 255);
    orm.getPixels()[3] = Color8( 32, 180, 0, 255);

    std::string png_error;
    require(
        savePng(orm, gltf_path.parent_path() / "textured_transmission_orm.png", png_error),
        "failed to create synthetic textured transmission ORM texture"
    );

    const std::filesystem::path bin_path = gltf_path.parent_path() / "textured_transmission_triangle.bin";
    const float positions[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         0.0f,  1.0f, -3.0f
    };

    {
        std::ofstream bin(bin_path, std::ios::binary);
        require(static_cast<bool>(bin), "failed to create synthetic textured transmission buffer");
        bin.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    }

    std::ofstream gltf(gltf_path);
    require(static_cast<bool>(gltf), "failed to create synthetic textured transmission glTF");
    gltf
        << R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_transmission"],
  "buffers": [{"uri": "textured_transmission_triangle.bin", "byteLength": 36}],
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
  "images": [{"uri": "textured_transmission_orm.png", "mimeType": "image/png"}],
  "textures": [{"source": 0}],
  "materials": [{
    "pbrMetallicRoughness": {
      "baseColorFactor": [0.85, 0.95, 1.0, 1.0],
      "metallicRoughnessTexture": {"index": 0},
      "metallicFactor": 1.0,
      "roughnessFactor": 1.0
    },
    "extensions": {
      "KHR_materials_transmission": {"transmissionFactor": 1.0}
    }
  }],
  "meshes": [{
    "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
  }],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
}

void checkGltfTransmissionMapping() {
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "astratrace_phase2_transmission";

    const std::filesystem::path glass_path = root / "smooth_glass.gltf";
    writeTransmissionScene(glass_path, 0.02f, 0.0f, 1.0f);
    Scene glass_scene;
    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), 1.0f);
    GltfSceneLoadResult glass_result = io::gltf::loadSceneFromGLTF(glass_path.string(), glass_scene, camera, 1.0f);
    require(glass_result.success, "smooth transmission glTF failed to load");
    require(!glass_scene.getObjects().empty(), "smooth transmission scene imported no objects");
    require(
        static_cast<bool>(std::dynamic_pointer_cast<SmoothGlassMaterial>(glass_scene.getObjects()[0]->getMaterial())),
        "smooth high-transmission material did not map to SmoothGlassMaterial"
    );

    const std::filesystem::path rough_path = root / "rough_transmission.gltf";
    writeTransmissionScene(rough_path, 0.35f, 0.0f, 1.0f);
    Scene rough_scene;
    GltfSceneLoadResult rough_result = io::gltf::loadSceneFromGLTF(rough_path.string(), rough_scene, camera, 1.0f);
    require(rough_result.success, "rough transmission glTF failed to load");
    require(!rough_scene.getObjects().empty(), "rough transmission scene imported no objects");
    require(
        static_cast<bool>(std::dynamic_pointer_cast<SmoothGlassMaterial>(rough_scene.getObjects()[0]->getMaterial())),
        "rough transmission material should map to SmoothGlassMaterial preview"
    );
    require(
        rough_result.warning.find("KHR_materials_transmission") != std::string::npos,
        "rough transmission preview did not report a warning"
    );

    const std::filesystem::path textured_path = root / "textured_transmission.gltf";
    writeTexturedTransmissionScene(textured_path);
    Scene textured_scene;
    GltfSceneLoadResult textured_result =
        io::gltf::loadSceneFromGLTF(textured_path.string(), textured_scene, camera, 1.0f);
    require(textured_result.success, "textured transmission glTF failed to load");
    require(!textured_scene.getObjects().empty(), "textured transmission scene imported no objects");
    auto textured_glass =
        std::dynamic_pointer_cast<SmoothGlassMaterial>(textured_scene.getObjects()[0]->getMaterial());
    require(static_cast<bool>(textured_glass), "textured transmission material did not map to glass preview");
    require(textured_glass->base_color != nullptr, "textured transmission glass did not receive a detail texture");
    require(
        textured_glass->surface_detail_strength > 0.0f,
        "textured transmission glass did not receive surface detail strength"
    );
    Color detail_a = textured_glass->sampleBaseColor(glm::vec2(0.10f, 0.10f));
    Color detail_b = textured_glass->sampleBaseColor(glm::vec2(0.90f, 0.90f));
    require(
        glm::abs(detail_a.r - detail_b.r) > 0.02f,
        "textured transmission detail texture did not preserve variation"
    );
    SurfaceData textured_surface{};
    textured_surface.hit_direction = HitDirection::ENTERING;
    textured_surface.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    textured_surface.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    textured_surface.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    textured_surface.uv = glm::vec2(0.10f, 0.10f);
    auto textured_bsdf = textured_glass->sampleBSDF(textured_surface);
    BSDFSample textured_sample = textured_bsdf->sample(glm::vec3(0.25f, 0.25f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    require(
        textured_sample.lobe == LobeType::Diffuse && !textured_sample.is_delta && textured_sample.pdf > 0.0f,
        "textured transmission glass preview did not sample a surface-detail lobe"
    );

    const std::filesystem::path metallic_path = root / "metallic_transmission.gltf";
    writeTransmissionScene(metallic_path, 0.02f, 1.0f, 1.0f);
    Scene metallic_scene;
    GltfSceneLoadResult metallic_result =
        io::gltf::loadSceneFromGLTF(metallic_path.string(), metallic_scene, camera, 1.0f);
    require(metallic_result.success, "metallic transmission glTF failed to load");
    require(!metallic_scene.getObjects().empty(), "metallic transmission scene imported no objects");
    require(
        static_cast<bool>(std::dynamic_pointer_cast<PBRMaterial>(metallic_scene.getObjects()[0]->getMaterial())),
        "metallic transmission material should remain PBR"
    );
    require(
        metallic_result.warning.find("metallic transmission") != std::string::npos,
        "metallic transmission fallback did not report a warning"
    );

    const std::filesystem::path smooth_volume_path = root / "smooth_volume_transmission.gltf";
    writeTransmissionScene(smooth_volume_path, 0.02f, 0.0f, 1.0f, true);
    Scene smooth_volume_scene;
    GltfSceneLoadResult smooth_volume_result =
        io::gltf::loadSceneFromGLTF(smooth_volume_path.string(), smooth_volume_scene, camera, 1.0f);
    require(smooth_volume_result.success, "smooth volume transmission glTF failed to load");
    require(!smooth_volume_scene.getObjects().empty(), "smooth volume transmission scene imported no objects");
    auto smooth_volume_glass =
        std::dynamic_pointer_cast<SmoothGlassMaterial>(smooth_volume_scene.getObjects()[0]->getMaterial());
    require(
        static_cast<bool>(smooth_volume_glass),
        "smooth volume transmission material did not map to SmoothGlassMaterial preview"
    );
    require(
        smooth_volume_result.warning.find("KHR_materials_volume") != std::string::npos,
        "smooth volume transmission preview did not report a volume warning"
    );

    const std::filesystem::path rough_volume_path = root / "rough_volume_transmission.gltf";
    writeTransmissionScene(rough_volume_path, 0.35f, 0.0f, 1.0f, true);
    Scene rough_volume_scene;
    GltfSceneLoadResult rough_volume_result =
        io::gltf::loadSceneFromGLTF(rough_volume_path.string(), rough_volume_scene, camera, 1.0f);
    require(rough_volume_result.success, "rough volume transmission glTF failed to load");
    require(!rough_volume_scene.getObjects().empty(), "rough volume transmission scene imported no objects");
    require(
        static_cast<bool>(
            std::dynamic_pointer_cast<SmoothGlassMaterial>(rough_volume_scene.getObjects()[0]->getMaterial())
        ),
        "rough volume transmission material should map to SmoothGlassMaterial preview"
    );
    require(
        rough_volume_result.warning.find("KHR_materials_transmission") != std::string::npos,
        "rough volume transmission preview did not report a transmission warning"
    );
    require(
        rough_volume_result.warning.find("KHR_materials_volume") != std::string::npos,
        "rough volume transmission preview did not report a volume warning"
    );
}

void writeUvTransformScene(const std::filesystem::path& gltf_path) {
    std::filesystem::create_directories(gltf_path.parent_path());

    Image<Color8> albedo(2, 2);
    albedo.getPixels()[0] = Color8(255, 0, 0, 255);
    albedo.getPixels()[1] = Color8(0, 255, 0, 255);
    albedo.getPixels()[2] = Color8(0, 0, 255, 255);
    albedo.getPixels()[3] = Color8(255, 255, 255, 255);
    std::string png_error;
    require(savePng(albedo, gltf_path.parent_path() / "uv_transform_albedo.png", png_error), "failed to create UV transform texture");

    const std::filesystem::path bin_path = gltf_path.parent_path() / "uv_transform.bin";
    const float data[] = {
        -1.0f, -1.0f, -3.0f,  1.0f, -1.0f, -3.0f,  0.0f,  1.0f, -3.0f,
         0.0f,  0.0f,        0.0f,  0.0f,        0.0f,  0.0f,
         0.0f,  0.0f,        0.5f,  0.0f,        0.0f,  0.5f
    };
    {
        std::ofstream bin(bin_path, std::ios::binary);
        require(static_cast<bool>(bin), "failed to create UV transform buffer");
        bin.write(reinterpret_cast<const char*>(data), sizeof(data));
    }

    std::ofstream gltf(gltf_path);
    require(static_cast<bool>(gltf), "failed to create UV transform glTF");
    gltf << R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_texture_transform"],
  "buffers": [{"uri": "uv_transform.bin", "byteLength": 84}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 24},
    {"buffer": 0, "byteOffset": 60, "byteLength": 24}
  ],
  "accessors": [
    {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [-1.0, -1.0, -3.0], "max": [1.0, 1.0, -3.0]},
    {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC2"},
    {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC2"}
  ],
  "images": [{"uri": "uv_transform_albedo.png", "mimeType": "image/png"}],
  "textures": [{"source": 0}],
  "materials": [{
    "pbrMetallicRoughness": {
      "baseColorTexture": {
        "index": 0,
        "texCoord": 1,
        "extensions": {"KHR_texture_transform": {"offset": [0.25, 0.0], "scale": [1.0, 1.0]}}
      },
      "metallicFactor": 0.0,
      "roughnessFactor": 1.0
    }
  }],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1, "TEXCOORD_1": 2}, "material": 0}]}],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
}

void checkGltfUvTextureTransform() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "astratrace_phase2_uv_transform" / "uv_transform.gltf";
    writeUvTransformScene(path);

    Scene scene;
    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), 1.0f);
    GltfSceneLoadResult result = io::gltf::loadSceneFromGLTF(path.string(), scene, camera, 1.0f);
    require(result.success, "UV transform glTF failed to load");
    require(!scene.getObjects().empty(), "UV transform scene imported no objects");
    auto pbr = std::dynamic_pointer_cast<PBRMaterial>(scene.getObjects()[0]->getMaterial());
    require(static_cast<bool>(pbr), "UV transform material should be PBR");
    require(pbr->base_color_mapping.texcoord == 1, "base-color texture did not preserve TEXCOORD_1");
    require(glm::abs(pbr->base_color_mapping.offset.x - 0.25f) < 1e-6f, "base-color texture transform offset was not imported");

    SurfaceData surface{};
    surface.normal = glm::vec3(0.0f, 0.0f, 1.0f);
    surface.tangent = glm::vec3(1.0f, 0.0f, 0.0f);
    surface.bitangent = glm::vec3(0.0f, 1.0f, 0.0f);
    surface.uv = glm::vec2(0.0f);
    surface.uv1 = glm::vec2(0.0f);
    ColorA a = pbr->sampleBaseColor(surface);
    surface.uv1 = glm::vec2(0.5f, 0.5f);
    ColorA b = pbr->sampleBaseColor(surface);
    require(glm::abs(a.r - b.r) > 0.05f || glm::abs(a.g - b.g) > 0.05f, "mapped texture sampling ignored UV1/transform");
}

void writeSparseAccessorScene(const std::filesystem::path& gltf_path) {
    std::filesystem::create_directories(gltf_path.parent_path());
    const std::filesystem::path bin_path = gltf_path.parent_path() / "sparse_positions.bin";
    const uint8_t indices[] = {0, 1, 2, 0};
    const float positions[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         0.0f,  1.0f, -3.0f
    };
    {
        std::ofstream bin(bin_path, std::ios::binary);
        require(static_cast<bool>(bin), "failed to create sparse accessor buffer");
        bin.write(reinterpret_cast<const char*>(indices), sizeof(indices));
        bin.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    }

    std::ofstream gltf(gltf_path);
    require(static_cast<bool>(gltf), "failed to create sparse accessor glTF");
    gltf << R"({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "sparse_positions.bin", "byteLength": 40}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 3},
    {"buffer": 0, "byteOffset": 4, "byteLength": 36}
  ],
  "accessors": [{
    "componentType": 5126,
    "count": 3,
    "type": "VEC3",
    "sparse": {
      "count": 3,
      "indices": {"bufferView": 0, "componentType": 5121},
      "values": {"bufferView": 1}
    },
    "min": [-1.0, -1.0, -3.0],
    "max": [1.0, 1.0, -3.0]
  }],
  "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [1.0, 1.0, 1.0, 1.0]}}],
  "meshes": [{"primitives": [{"attributes": {"POSITION": 0}, "material": 0}]}],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
}

void checkGltfSparseAccessor() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "astratrace_phase2_sparse" / "sparse.gltf";
    writeSparseAccessorScene(path);

    Scene scene;
    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), 1.0f);
    GltfSceneLoadResult result = io::gltf::loadSceneFromGLTF(path.string(), scene, camera, 1.0f);
    require(result.success, "sparse accessor glTF failed to load");
    require(!scene.getObjects().empty(), "sparse accessor scene imported no objects");
    scene.update();
    AABB bounds = scene.getObjects()[0]->getBounds();
    require(bounds.min.x < -0.9f && bounds.max.x > 0.9f, "sparse accessor positions were not applied to bounds");
}

void writeIridescenceScene(const std::filesystem::path& gltf_path) {
    std::filesystem::create_directories(gltf_path.parent_path());

    const std::filesystem::path bin_path = gltf_path.parent_path() / "iridescence_triangle.bin";
    const float positions[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         0.0f,  1.0f, -3.0f
    };

    {
        std::ofstream bin(bin_path, std::ios::binary);
        require(static_cast<bool>(bin), "failed to create synthetic iridescence buffer");
        bin.write(reinterpret_cast<const char*>(positions), sizeof(positions));
    }

    std::ofstream gltf(gltf_path);
    require(static_cast<bool>(gltf), "failed to create synthetic iridescence glTF");
    gltf
        << R"({
  "asset": {"version": "2.0"},
  "extensionsUsed": ["KHR_materials_iridescence"],
  "buffers": [{"uri": "iridescence_triangle.bin", "byteLength": 36}],
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
    "pbrMetallicRoughness": {
      "baseColorFactor": [0.0, 0.0, 0.0, 1.0],
      "metallicFactor": 1.0,
      "roughnessFactor": 0.1
    },
    "extensions": {
      "KHR_materials_iridescence": {
        "iridescenceFactor": 1.0,
        "iridescenceIor": 1.33,
        "iridescenceThicknessMaximum": 400.0
      }
    }
  }],
  "meshes": [{
    "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
  }],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
}

void checkGltfIridescenceFallback() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "astratrace_phase2_iridescence" / "iridescence_triangle.gltf";
    writeIridescenceScene(path);

    Scene scene;
    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), 1.0f);
    GltfSceneLoadResult result = io::gltf::loadSceneFromGLTF(path.string(), scene, camera, 1.0f);
    require(result.success, "iridescence glTF failed to load");
    require(!scene.getObjects().empty(), "iridescence scene imported no objects");
    require(
        result.warning.find("KHR_materials_iridescence") != std::string::npos,
        "iridescence fallback did not report a warning"
    );

    auto pbr = std::dynamic_pointer_cast<PBRMaterial>(scene.getObjects()[0]->getMaterial());
    require(static_cast<bool>(pbr), "iridescence material should remain a PBR preview material");

    ColorA base = pbr->sampleBaseColor(glm::vec2(0.5f));
    Color mr = pbr->sampleMetalRoughness(glm::vec2(0.5f));
    require(
        glm::max(base.r, glm::max(base.g, base.b)) > 0.05f,
        "iridescence fallback imported as a black material"
    );
    require(mr.r < 0.75f, "iridescence fallback remained fully metallic");
    require(mr.g > 0.45f, "iridescence fallback remained too glossy");
}

void writeGuideTextureScene(const std::filesystem::path& gltf_path) {
    std::filesystem::create_directories(gltf_path.parent_path());

    Image<Color8> guide(4, 4);
    guide.clear(Color8(0, 0, 0, 0));
    guide.getPixels()[2 * 4 + 2] = Color8(0, 0, 0, 255);

    std::string png_error;
    require(
        savePng(guide, gltf_path.parent_path() / "guide.png", png_error),
        "failed to create synthetic guide texture"
    );

    const std::filesystem::path bin_path = gltf_path.parent_path() / "guide_triangle.bin";
    const float positions[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         0.0f,  1.0f, -3.0f
    };
    const float uvs[] = {
        0.625f, 0.625f,
        0.625f, 0.625f,
        0.625f, 0.625f
    };

    {
        std::ofstream bin(bin_path, std::ios::binary);
        require(static_cast<bool>(bin), "failed to create synthetic guide buffer");
        bin.write(reinterpret_cast<const char*>(positions), sizeof(positions));
        bin.write(reinterpret_cast<const char*>(uvs), sizeof(uvs));
    }

    std::ofstream gltf(gltf_path);
    require(static_cast<bool>(gltf), "failed to create synthetic guide glTF");
    gltf
        << R"({
  "asset": {"version": "2.0"},
  "buffers": [{"uri": "guide_triangle.bin", "byteLength": 60}],
  "bufferViews": [
    {"buffer": 0, "byteOffset": 0, "byteLength": 36},
    {"buffer": 0, "byteOffset": 36, "byteLength": 24}
  ],
  "accessors": [{
    "bufferView": 0,
    "byteOffset": 0,
    "componentType": 5126,
    "count": 3,
    "type": "VEC3",
    "min": [-1.0, -1.0, -3.0],
    "max": [1.0, 1.0, -3.0]
  }, {
    "bufferView": 1,
    "byteOffset": 0,
    "componentType": 5126,
    "count": 3,
    "type": "VEC2"
  }],
  "images": [{"uri": "guide.png", "mimeType": "image/png"}],
  "textures": [{"source": 0}],
  "materials": [{
    "name": "Guides Material",
    "alphaMode": "BLEND",
    "pbrMetallicRoughness": {
      "baseColorTexture": {"index": 0},
      "metallicFactor": 0.0,
      "roughnessFactor": 0.5
    }
  }],
  "meshes": [{
    "primitives": [{"attributes": {"POSITION": 0, "TEXCOORD_0": 1}, "material": 0}]
  }],
  "nodes": [{"mesh": 0}],
  "scenes": [{"nodes": [0]}],
  "scene": 0
})";
}

void checkGltfGuideTextureFallback() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "astratrace_phase2_guides" / "guide_triangle.gltf";
    writeGuideTextureScene(path);

    Scene scene;
    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), 1.0f);
    GltfSceneLoadResult result = io::gltf::loadSceneFromGLTF(path.string(), scene, camera, 1.0f);
    require(result.success, "guide texture glTF failed to load");
    require(!scene.getObjects().empty(), "guide texture scene imported no objects");
    require(
        result.warning.find("guide/label") != std::string::npos,
        "guide texture fallback did not report a warning"
    );

    auto pbr = std::dynamic_pointer_cast<PBRMaterial>(scene.getObjects()[0]->getMaterial());
    require(static_cast<bool>(pbr), "guide material should remain PBR");
    ColorA base = pbr->sampleBaseColor(glm::vec2(0.625f));
    Color emission = pbr->sampleEmissive(glm::vec2(0.625f));
    require(base.r > 0.6f && base.g > 0.6f && base.b > 0.6f, "guide texture remained dark");
    require(base.a > 0.9f, "guide texture lost alpha coverage");
    require(
        glm::max(emission.r, glm::max(emission.g, emission.b)) > 0.2f,
        "guide texture did not receive visible-only emission"
    );
    require(
        glm::max(
            pbr->getAverageEmissivePower().r,
            glm::max(pbr->getAverageEmissivePower().g, pbr->getAverageEmissivePower().b)
        ) == 0.0f,
        "guide texture should not become a path-light candidate"
    );
    require(!pbr->castsShadows(), "guide texture should not cast annotation shadows");
}

void checkPngExportUtility() {
    Scene scene;
    Camera camera;
    buildMaterialShowcaseScene(scene, camera, 1.0f);

    Image<Color8> image(8, 8);
    render::cpu::CpuPathRenderer renderer;
    render::PathRenderSettings settings;
    settings.denoiser = render::PathDenoiserMode::Temporal;
    settings.samples_per_frame = 1;
    settings.max_bounces = 2;
    render::RenderFrameContext context{.scene_changed = true};
    renderer.render(scene, camera, image, context, settings);

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "astratrace_phase2_export" / "showcase.png";
    std::string error;
    require(savePng(image, path, error), "PNG export utility failed");
    require(std::filesystem::exists(path), "PNG export utility did not create a file");
    require(std::filesystem::file_size(path) > 0, "PNG export utility created an empty file");
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
        SceneStats stats = scene.getStats();
        require(stats.top_level_bvh_node_count > 0, "scene update did not build a TLAS");
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
        checkPathNoOpExportAccumulation();
        checkTransformedSceneObject();
        checkBvhTraversalEquivalence();
        checkTlasRefitAfterMovement();
        checkEnvironmentSampling();
        checkTileProgressAndDeterminism();
        checkCpuPathCancellation();
        checkSvgfPreviewFinite();
        checkOIDNUnavailableBehavior();
        checkMaterialShowcase();
        checkGltfTransmissionMapping();
        checkGltfUvTextureTransform();
        checkGltfSparseAccessor();
        checkGltfIridescenceFallback();
        checkGltfGuideTextureFallback();
        checkPngExportUtility();
        checkRequiredScenes();
        checkLoadedLightOrientation();
    } catch(const std::exception& error) {
        std::cerr << "Phase 2 sanity check failed: " << error.what() << "\n";
        return 1;
    }

    std::cout << "Phase 2 path tracing sanity checks passed.\n";
    return 0;
}
