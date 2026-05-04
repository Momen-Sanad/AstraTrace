#pragma once

#include <vector>
#include "render/renderer.hpp"
#include "render/cpu/path_integrator.hpp"

namespace render::cpu {

class CpuPathRenderer : public render::IRenderer {
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
    struct HistoryPixel {
        PathResult path;
        Color color = Color(0.0f);
        float depth = 0.0f;
        uint32_t frames = 0;
    };

    void ensureBuffers(int width, int height);
    void clearHistory();
    Color denoiseSVGF(const std::vector<PathResult>& current, int x, int y, int width, int height) const;

    PathIntegrator integrator;
    std::vector<Color> accumulation;
    std::vector<HistoryPixel> svgf_history;
    std::vector<HistoryPixel> svgf_current;
    int buffer_width = 0;
    int buffer_height = 0;
    uint32_t accumulated_samples = 0;
    uint64_t next_sample_index = 0;
    PathRenderSettings last_settings;
};

} // namespace render::cpu
