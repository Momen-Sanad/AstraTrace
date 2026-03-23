#pragma once

#include "scene/world/scene.hpp"

namespace render::cpu {

class WhittedIntegrator {
public:
    explicit WhittedIntegrator(int max_depth = 5) : max_depth(max_depth) {}

    Color trace(const Scene& scene, const Ray& ray) const;

private:
    Color traceRecursive(const Scene& scene, const Ray& ray, int depth) const;
    int max_depth = 5;
};

} // namespace render::cpu
