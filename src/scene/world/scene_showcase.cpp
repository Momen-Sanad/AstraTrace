#include "scene/world/scene_showcase.hpp"

#include <memory>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include "scene/geometry/geometry.hpp"
#include "scene/lights/lights.hpp"
#include "scene/materials/materials.hpp"

namespace {

std::shared_ptr<PBRMaterial> makePbr(ColorA tint, float metallic, float roughness, Color emission = Color(0.0f)) {
    auto material = std::make_shared<PBRMaterial>();
    material->tint = tint;
    material->metal_roughness = checkerboard<Color>(
        1,
        1,
        Color(glm::clamp(metallic, 0.0f, 1.0f), glm::clamp(roughness, 0.0f, 1.0f), 0.0f),
        Color(glm::clamp(metallic, 0.0f, 1.0f), glm::clamp(roughness, 0.0f, 1.0f), 0.0f)
    );
    material->emissive_power = emission;
    return material;
}

} // namespace

void buildMaterialShowcaseScene(Scene& scene, Camera& camera, float aspect_ratio) {
    scene.clear();

    camera.setPosition(glm::vec3(0.0f, 1.25f, 5.8f));
    camera.setRotation(
        glm::normalize(glm::vec3(0.0f, -0.12f, -1.0f)),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    camera.setHalfSize(glm::radians(48.0f), aspect_ratio);

    auto floor_material = makePbr(ColorA(0.62f, 0.62f, 0.58f, 1.0f), 0.0f, 0.72f);
    auto wall_material = makePbr(ColorA(0.45f, 0.50f, 0.58f, 1.0f), 0.0f, 0.82f);
    auto dielectric = makePbr(ColorA(0.95f, 0.28f, 0.20f, 1.0f), 0.0f, 0.45f);
    auto rough_metal = makePbr(ColorA(0.86f, 0.70f, 0.36f, 1.0f), 1.0f, 0.34f);
    auto emitter = makePbr(ColorA(1.0f), 0.0f, 0.35f, Color(10.0f, 8.2f, 5.5f));

    auto mirror = std::make_shared<SmoothMirrorMaterial>();
    mirror->tint = Color(0.92f, 0.96f, 1.0f);

    auto glass = std::make_shared<SmoothGlassMaterial>();
    glass->tint = Color(0.82f, 0.94f, 1.0f);
    glass->refractive_index = 1.5f;

    scene.createObject(
        createRectange(
            glm::vec3(0.0f, -0.72f, 0.0f),
            glm::vec2(7.0f, 6.0f),
            glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec2(4.0f)
        ),
        floor_material
    );
    scene.createObject(
        createRectange(
            glm::vec3(0.0f, 1.65f, -2.25f),
            glm::vec2(7.0f, 4.8f),
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec2(2.0f)
        ),
        wall_material
    );
    scene.createObject(
        createRectange(
            glm::vec3(0.0f, 3.0f, -0.35f),
            glm::vec2(2.0f, 1.0f),
            glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec2(1.0f)
        ),
        emitter
    );

    scene.createObject(std::make_shared<Sphere>(glm::vec3(-2.25f, 0.0f, 0.0f), 0.72f), glass);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(-0.75f, 0.0f, 0.0f), 0.72f), mirror);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(0.75f, 0.0f, 0.0f), 0.72f), rough_metal);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(2.25f, 0.0f, 0.0f), 0.72f), dielectric);

    scene.addLight(std::make_shared<DirectionLight>(
        glm::normalize(glm::vec3(-0.35f, -1.0f, -0.45f)),
        Color(1.1f, 1.05f, 0.95f)
    ));
    scene.addLight(std::make_shared<PointLight>(
        glm::vec3(0.0f, 2.5f, 2.0f),
        Color(18.0f, 16.0f, 13.0f)
    ));

    scene.setAmbient(Color(0.015f));
    scene.setBackgroundColor(Color(0.025f, 0.028f, 0.032f));
    scene.update();
}
