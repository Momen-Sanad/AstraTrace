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
    render::RenderProgress getProgress() const override;

private:
    struct Tile {
        int x0 = 0;
        int y0 = 0;
        int x1 = 0;
        int y1 = 0;
    };

    struct HistoryPixel {
        PathResult path;
        Color color = Color(0.0f);
        Color albedo = Color(0.0f);
        glm::vec3 normal = glm::vec3(0.0f);
        float depth = 0.0f;
        float moment1 = 0.0f;
        float moment2 = 0.0f;
        float variance = 0.0f;
        uint32_t frames = 0;
    };

    void ensureBuffers(int width, int height);
    void clearHistory();
    std::vector<Tile> buildTiles(int width, int height) const;
    void filterSVGFATrous(
        const std::vector<Color>& input,
        std::vector<Color>& output,
        const std::vector<HistoryPixel>& guide,
        int width,
        int height,
        int step
    ) const;
    int findReprojectedHistoryIndex(
        const PathResult& current,
        int current_index,
        int width,
        int height,
        bool camera_changed
    ) const;

    PathIntegrator integrator;
    std::vector<Color> accumulation;
    std::vector<Color> oidn_albedo_accumulation;
    std::vector<Color> oidn_normal_accumulation;
    std::vector<HistoryPixel> svgf_history;
    std::vector<HistoryPixel> svgf_current;
    std::vector<Color> svgf_filter_a;
    std::vector<Color> svgf_filter_b;
    int buffer_width = 0;
    int buffer_height = 0;
    uint32_t accumulated_samples = 0;
    uint64_t next_sample_index = 0;
    render::RenderProgress progress;
    std::string status_note;
    PathRenderSettings last_settings;
    Camera previous_camera;
    bool has_previous_camera = false;
};

} // namespace render::cpu
