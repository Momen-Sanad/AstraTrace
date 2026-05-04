#include "render/gpu/gpu_path_renderer.hpp"

#include <SDL3/SDL_log.h>

namespace render::gpu {

void GpuPathRenderer::render(const Scene& scene, const Camera& camera, Image<Color8>& output) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GPU path renderer backend is currently routed to CPU path renderer.");
    fallback_cpu_path.render(scene, camera, output);
}

void GpuPathRenderer::render(
    const Scene& scene,
    const Camera& camera,
    Image<Color8>& output,
    const render::RenderFrameContext& context,
    const render::PathRenderSettings& path_settings
) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GPU path renderer backend is currently routed to CPU path renderer.");
    fallback_cpu_path.render(scene, camera, output, context, path_settings);
}

void GpuPathRenderer::reset() {
    fallback_cpu_path.reset();
}

std::string GpuPathRenderer::getStatus() const {
    return fallback_cpu_path.getStatus();
}

} // namespace render::gpu
