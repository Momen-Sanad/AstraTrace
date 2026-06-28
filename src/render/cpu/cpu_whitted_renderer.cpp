#include "render/cpu/cpu_whitted_renderer.hpp"

#include <chrono>
#include <cstdio>

#include "core/color.hpp"

namespace render::cpu {

void CpuWhittedRenderer::render(const Scene& scene, const Camera& camera, Image<Color8>& output) {
    const auto render_start = std::chrono::steady_clock::now();
    const int width = output.getWidth();
    const int height = output.getHeight();
    Color8* pixels = output.getPixels();

    #pragma omp parallel for schedule(static)
    for(int y = 0; y < height; ++y) {
        for(int x = 0, idx = y * width; x < width; ++x, ++idx) {
            Ray ray = camera.generateRay(glm::vec2((x + 0.5f) / width, (y + 0.5f) / height));
            Color radiance = integrator.trace(scene, ray);
            pixels[idx] = encodeColor(tonemap_aces(radiance));
        }
    }

    const auto render_end = std::chrono::steady_clock::now();
    progress.width = width;
    progress.height = height;
    progress.tile_count = 0;
    progress.accumulated_samples = 1;
    progress.last_frame_ms = std::chrono::duration<double, std::milli>(render_end - render_start).count();
    progress.canceled = false;
}

std::string CpuWhittedRenderer::getStatus() const {
    char text[128];
    std::snprintf(
        text,
        sizeof(text),
        "CPU Whitted: last frame %.2f ms @ %dx%d",
        progress.last_frame_ms,
        progress.width,
        progress.height
    );
    return text;
}

render::RenderProgress CpuWhittedRenderer::getProgress() const {
    return progress;
}

} // namespace render::cpu
