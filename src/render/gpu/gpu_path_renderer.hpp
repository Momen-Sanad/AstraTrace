#pragma once

#include "render/renderer.hpp"
#include "render/cpu/cpu_path_renderer.hpp"

namespace render::gpu {

class GpuPathRenderer : public render::IRenderer {
public:
    void render(const Scene& scene, const Camera& camera, Image<Color8>& output) override;
    void render(
        const Scene& scene,
        const Camera& camera,
        Image<Color8>& output,
        const render::RenderFrameContext& context,
        const render::PathRenderSettings& path_settings
    ) override;
    void reset() override;
    std::string getStatus() const override;

private:
    render::cpu::CpuPathRenderer fallback_cpu_path;
};

} // namespace render::gpu
