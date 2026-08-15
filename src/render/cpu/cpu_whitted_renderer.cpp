#include "render/cpu/cpu_whitted_renderer.hpp"

#include <chrono>
#include <cstdio>
#include <iterator>

#include "core/color.hpp"

namespace render::cpu {
namespace {

constexpr glm::vec2 WHITTED_AA_OFFSETS[] = {
    glm::vec2(0.25f, 0.25f),
    glm::vec2(0.75f, 0.25f),
    glm::vec2(0.25f, 0.75f),
    glm::vec2(0.75f, 0.75f),
};

}

void CpuWhittedRenderer::render(const Scene& scene, const Camera& camera, Image<Color8>& output) {
    const auto render_start = std::chrono::steady_clock::now();
    const int width = output.getWidth();
    const int height = output.getHeight();
    Color8* pixels = output.getPixels();

    #pragma omp parallel for schedule(static)
    for(int y = 0; y < height; ++y) {
        for(int x = 0, idx = y * width; x < width; ++x, ++idx) {
            Color radiance(0.0f);
            for(const glm::vec2& offset : WHITTED_AA_OFFSETS) {
                Ray ray = camera.generateRay(glm::vec2((x + offset.x) / width, (y + offset.y) / height));
                radiance += integrator.trace(scene, ray);
            }
            radiance /= static_cast<float>(std::size(WHITTED_AA_OFFSETS));
            pixels[idx] = encodeColor(tonemap_aces(radiance));
        }
    }

    const auto render_end = std::chrono::steady_clock::now();
    progress.width = width;
    progress.height = height;
    progress.tile_count = 0;
    progress.accumulated_samples = static_cast<int>(std::size(WHITTED_AA_OFFSETS));
    progress.last_frame_ms = std::chrono::duration<double, std::milli>(render_end - render_start).count();
    progress.canceled = false;
}

std::string CpuWhittedRenderer::getStatus() const {
    char text[128];
    std::snprintf(
        text,
        sizeof(text),
        "CPU Whitted: last frame %.2f ms @ %dx%d, %d spp AA",
        progress.last_frame_ms,
        progress.width,
        progress.height,
        progress.accumulated_samples
    );
    return text;
}

render::RenderProgress CpuWhittedRenderer::getProgress() const {
    return progress;
}

} // namespace render::cpu
