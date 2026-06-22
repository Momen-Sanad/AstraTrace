#pragma once

#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"

constexpr const char* MATERIAL_SHOWCASE_SCENE_ID = "builtin:material-showcase";

void buildMaterialShowcaseScene(Scene& scene, Camera& camera, float aspect_ratio);
