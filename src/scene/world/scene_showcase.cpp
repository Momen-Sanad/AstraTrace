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

    camera.setPosition(glm::vec3(0.0f, 1.08f, 6.9f));
    camera.setRotation(
        glm::normalize(glm::vec3(0.0f, -0.08f, -1.0f)),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    camera.setHalfSize(glm::radians(44.0f), aspect_ratio);

    auto floor_material = makePbr(ColorA(0.62f, 0.62f, 0.58f, 1.0f), 0.0f, 0.72f);
    auto wall_material = makePbr(ColorA(0.42f, 0.47f, 0.54f, 1.0f), 0.0f, 0.82f);
    auto chrome = makePbr(ColorA(0.92f, 0.95f, 1.0f, 1.0f), 1.0f, 0.08f);
    auto gold = makePbr(ColorA(0.95f, 0.76f, 0.34f, 1.0f), 1.0f, 0.24f);
    auto rough_copper = makePbr(ColorA(0.80f, 0.42f, 0.23f, 1.0f), 1.0f, 0.62f);
    auto glossy_dielectric = makePbr(ColorA(0.95f, 0.18f, 0.12f, 1.0f), 0.0f, 0.18f);
    auto rough_dielectric = makePbr(ColorA(0.20f, 0.42f, 0.95f, 1.0f), 0.0f, 0.78f);
    auto emissive_sphere = makePbr(ColorA(0.40f, 0.90f, 1.0f, 1.0f), 0.0f, 0.28f, Color(0.25f, 0.75f, 1.1f));
    auto emitter = makePbr(ColorA(1.0f), 0.0f, 0.35f, Color(8.5f, 7.4f, 5.4f));

    auto mirror = std::make_shared<SmoothMirrorMaterial>();
    mirror->tint = Color(0.92f, 0.96f, 1.0f);

    auto glass = std::make_shared<SmoothGlassMaterial>();
    glass->tint = Color(0.82f, 0.94f, 1.0f);
    glass->refractive_index = 1.5f;

    scene.createObject(
        createRectange(
            glm::vec3(0.0f, -0.72f, 0.0f),
            glm::vec2(9.4f, 6.4f),
            glm::angleAxis(-glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec2(5.0f)
        ),
        floor_material
    );
    scene.createObject(
        createRectange(
            glm::vec3(0.0f, 1.58f, -2.15f),
            glm::vec2(9.4f, 4.7f),
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            glm::vec2(2.0f)
        ),
        wall_material
    );
    scene.createObject(
        createRectange(
            glm::vec3(0.0f, 2.95f, -0.15f),
            glm::vec2(2.6f, 1.0f),
            glm::angleAxis(glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::vec2(1.0f)
        ),
        emitter
    );

    const float radius = 0.46f;
    const float y = -0.26f;
    scene.createObject(std::make_shared<Sphere>(glm::vec3(-3.65f, y, 0.0f), radius), glass);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(-2.60f, y, 0.0f), radius), mirror);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(-1.55f, y, 0.0f), radius), chrome);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(-0.50f, y, 0.0f), radius), gold);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(0.55f, y, 0.0f), radius), rough_copper);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(1.60f, y, 0.0f), radius), glossy_dielectric);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(2.65f, y, 0.0f), radius), rough_dielectric);
    scene.createObject(std::make_shared<Sphere>(glm::vec3(3.70f, y, 0.0f), radius), emissive_sphere);

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
