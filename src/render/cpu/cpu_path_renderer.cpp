#include "render/cpu/cpu_path_renderer.hpp"

#include "core/color.hpp"

namespace render::cpu {

void CpuPathRenderer::render(const Scene& scene, const Camera& camera, Image<Color8>& output) {
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
}

} // namespace render::cpu
