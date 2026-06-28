#pragma once

#include "render/renderer.hpp"
#include "render/cpu/whitted_integrator.hpp"

namespace render::cpu {

class CpuWhittedRenderer : public render::IRenderer {
public:
    void render(const Scene& scene, const Camera& camera, Image<Color8>& output) override;
    std::string getStatus() const override;
    render::RenderProgress getProgress() const override;

private:
    WhittedIntegrator integrator;
    render::RenderProgress progress;
};

} // namespace render::cpu
