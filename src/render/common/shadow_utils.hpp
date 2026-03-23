#pragma once

#include "scene/world/scene.hpp"

namespace render::common {

Color computeShadow(const Scene& scene, Ray ray, float max_distance);

}
