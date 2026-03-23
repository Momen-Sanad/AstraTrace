#pragma once

#include "render/renderer.hpp"
#include "render/cpu/path_integrator.hpp"

namespace render::cpu {

class CpuPathRenderer : public render::IRenderer {
public:
    void render(const Scene& scene, const Camera& camera, Image<Color8>& output) override;

private:
    PathIntegrator integrator;
};

} // namespace render::cpu
