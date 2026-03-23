#pragma once

#include "render/renderer.hpp"
#include "render/cpu/cpu_path_renderer.hpp"

namespace render::gpu {

class GpuPathRenderer : public render::IRenderer {
public:
    void render(const Scene& scene, const Camera& camera, Image<Color8>& output) override;

private:
    render::cpu::CpuPathRenderer fallback_cpu_path;
};

} // namespace render::gpu
