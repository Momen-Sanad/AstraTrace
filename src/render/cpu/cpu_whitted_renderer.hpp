#pragma once

#include "render/renderer.hpp"
#include "render/cpu/whitted_integrator.hpp"

namespace render::cpu {

class CpuWhittedRenderer : public render::IRenderer {
public:
    void render(const Scene& scene, const Camera& camera, Image<Color8>& output) override;

private:
    WhittedIntegrator integrator;
};

} // namespace render::cpu
