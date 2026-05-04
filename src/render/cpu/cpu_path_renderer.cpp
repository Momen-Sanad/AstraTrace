#include "render/cpu/cpu_path_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "core/color.hpp"
#include "render/cpu/sampler.hpp"

namespace render::cpu {
namespace {

Color sanitize(Color color) {
    if(!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b)) return Color(0.0f);
    return glm::max(color, Color(0.0f));
}

float luminance(Color color) {
    return glm::dot(color, Color(0.2126f, 0.7152f, 0.0722f));
}

bool compatibleHistory(const PathResult& a, const PathResult& b) {
    if(a.object_id != b.object_id) return false;
    if(a.object_id == 0) return true;
    if(glm::dot(a.shading_normal, b.shading_normal) < 0.75f) return false;
    float d2 = glm::dot(a.hit_position - b.hit_position, a.hit_position - b.hit_position);
    return d2 < 0.02f;
}

}

void CpuPathRenderer::ensureBuffers(int width, int height) {
    if(width == buffer_width && height == buffer_height) return;
    buffer_width = width;
    buffer_height = height;
    std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    accumulation.assign(count, Color(0.0f));
    svgf_history.assign(count, {});
    svgf_current.assign(count, {});
    accumulated_samples = 0;
    next_sample_index = 0;
}

void CpuPathRenderer::clearHistory() {
    std::fill(accumulation.begin(), accumulation.end(), Color(0.0f));
    std::fill(svgf_history.begin(), svgf_history.end(), HistoryPixel{});
    std::fill(svgf_current.begin(), svgf_current.end(), HistoryPixel{});
    accumulated_samples = 0;
    next_sample_index = 0;
}

void CpuPathRenderer::reset() {
    clearHistory();
}

std::string CpuPathRenderer::getStatus() const {
    char text[128];
    std::snprintf(
        text,
        sizeof(text),
        "CPU path: %u accumulated sample%s",
        accumulated_samples,
        accumulated_samples == 1 ? "" : "s"
    );
    return text;
}

void CpuPathRenderer::render(const Scene& scene, const Camera& camera, Image<Color8>& output) {
    RenderFrameContext context{};
    PathRenderSettings settings{};
    render(scene, camera, output, context, settings);
}

Color CpuPathRenderer::denoiseSVGF(
    const std::vector<PathResult>& current,
    int x,
    int y,
    int width,
    int height
) const {
    const int index = y * width + x;
    const PathResult& center = current[static_cast<std::size_t>(index)];
    Color sum(0.0f);
    float weight_sum = 0.0f;

    for(int oy = -2; oy <= 2; ++oy) {
        int sy = glm::clamp(y + oy, 0, height - 1);
        for(int ox = -2; ox <= 2; ++ox) {
            int sx = glm::clamp(x + ox, 0, width - 1);
            int sample_index = sy * width + sx;
            const PathResult& sample = current[static_cast<std::size_t>(sample_index)];
            Color color = sample.radiance();

            float spatial = std::exp(-0.5f * static_cast<float>(ox * ox + oy * oy));
            float normal_weight = center.object_id == 0 || sample.object_id == 0
                ? 1.0f
                : glm::max(0.0f, glm::dot(center.shading_normal, sample.shading_normal));
            float depth_weight = 1.0f;
            if(center.object_id != 0 && sample.object_id != 0) {
                depth_weight = std::exp(-16.0f * glm::length(center.hit_position - sample.hit_position));
            }
            float object_weight = center.object_id == sample.object_id ? 1.0f : 0.15f;
            float weight = spatial * normal_weight * depth_weight * object_weight;
            sum += color * weight;
            weight_sum += weight;
        }
    }

    if(weight_sum <= 0.0f) return center.radiance();
    return sum / weight_sum;
}

void CpuPathRenderer::render(
    const Scene& scene,
    const Camera& camera,
    Image<Color8>& output,
    const render::RenderFrameContext& context,
    const render::PathRenderSettings& path_settings
) {
    const int width = output.getWidth();
    const int height = output.getHeight();
    ensureBuffers(width, height);

    PathRenderSettings settings = path_settings;
    settings.max_bounces = glm::clamp(settings.max_bounces, 1, 32);
    settings.samples_per_frame = glm::clamp(settings.samples_per_frame, 1, 64);

    if(
        settings.reset_requested ||
        context.camera_changed ||
        context.scene_changed ||
        context.settings_changed ||
        !(settings == last_settings)
    ) {
        clearHistory();
    }
    last_settings = settings;

    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<PathResult> current(pixel_count);
    std::vector<Color> current_color(pixel_count, Color(0.0f));
    glm::vec2 frame_jitter = settings.enable_taa
        ? Sampler::halton2D(static_cast<uint32_t>(context.frame_index + 1u))
        : glm::vec2(0.5f);

    for(int sample = 0; sample < settings.samples_per_frame; ++sample) {
        uint32_t sample_index = static_cast<uint32_t>(next_sample_index + static_cast<uint64_t>(sample));

        #pragma omp parallel for schedule(dynamic, 4)
        for(int y = 0; y < height; ++y) {
            for(int x = 0; x < width; ++x) {
                int index = y * width + x;
                PathResult result = integrator.trace(
                    scene,
                    camera,
                    glm::ivec2(x, y),
                    frame_jitter,
                    glm::ivec2(width, height),
                    sample_index,
                    settings
                );
                current[static_cast<std::size_t>(index)] = result;
                current_color[static_cast<std::size_t>(index)] += sanitize(result.radiance());
            }
        }
    }

    next_sample_index += static_cast<uint64_t>(settings.samples_per_frame);
    for(Color& color : current_color) {
        color /= static_cast<float>(settings.samples_per_frame);
    }

    Color8* pixels = output.getPixels();

    if(settings.denoiser == PathDenoiserMode::None) {
        #pragma omp parallel for schedule(static)
        for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
            pixels[i] = encodeColor(tonemap_aces(current_color[static_cast<std::size_t>(i)]));
        }
        return;
    }

    if(settings.denoiser == PathDenoiserMode::Temporal) {
        ++accumulated_samples;
        #pragma omp parallel for schedule(static)
        for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
            accumulation[static_cast<std::size_t>(i)] += current_color[static_cast<std::size_t>(i)];
            Color average = accumulation[static_cast<std::size_t>(i)] / static_cast<float>(accumulated_samples);
            pixels[i] = encodeColor(tonemap_aces(average));
        }
        return;
    }

    #pragma omp parallel for schedule(static)
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            int index = y * width + x;
            std::size_t idx = static_cast<std::size_t>(index);
            Color spatial = denoiseSVGF(current, x, y, width, height);
            const HistoryPixel& history = svgf_history[idx];

            float alpha = 0.18f;
            if(history.frames == 0 || !compatibleHistory(current[idx], history.path)) {
                alpha = 1.0f;
            } else {
                float lum_now = luminance(spatial);
                float lum_history = luminance(history.color);
                float diff = glm::abs(lum_now - lum_history);
                alpha = glm::clamp(0.08f + diff * 0.08f, 0.08f, 0.65f);
            }

            Color blended = alpha * spatial + (1.0f - alpha) * history.color;
            svgf_current[idx].path = current[idx];
            svgf_current[idx].color = sanitize(blended);
            svgf_current[idx].frames = history.frames + 1u;
            pixels[index] = encodeColor(tonemap_aces(svgf_current[idx].color));
        }
    }

    svgf_history.swap(svgf_current);
    accumulated_samples = glm::min<uint32_t>(accumulated_samples + settings.samples_per_frame, 0xffffffffu);
}

} // namespace render::cpu
