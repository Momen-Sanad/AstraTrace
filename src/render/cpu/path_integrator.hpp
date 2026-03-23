#pragma once

#include "render/cpu/whitted_integrator.hpp"

namespace render::cpu {

class PathIntegrator {
public:
    explicit PathIntegrator(int max_depth = 5) : fallback(max_depth) {}
    Color trace(const Scene& scene, const Ray& ray) const;

private:
    WhittedIntegrator fallback;
};

} // namespace render::cpu
