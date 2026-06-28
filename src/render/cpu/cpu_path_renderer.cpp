#include "render/cpu/cpu_path_renderer.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include "core/color.hpp"
#include "render/cpu/sampler.hpp"

#if defined(ASTRATRACE_HAS_OIDN) && ASTRATRACE_HAS_OIDN
#include <OpenImageDenoise/oidn.h>
#endif

namespace render::cpu {
namespace {

Color sanitize(Color color) {
    if(!std::isfinite(color.r) || !std::isfinite(color.g) || !std::isfinite(color.b)) return Color(0.0f);
    return glm::max(color, Color(0.0f));
}

float luminance(Color color) {
    return glm::dot(color, Color(0.2126f, 0.7152f, 0.0722f));
}

float historyDepth(const PathResult& path, const Camera& camera) {
    if(path.object_id == 0) return 0.0f;
    return glm::length(path.hit_position - camera.getPosition());
}

bool compatibleHistory(const PathResult& a, const PathResult& b, float a_depth, float b_depth) {
    if(a.object_id != b.object_id) return false;
    if(a.object_id == 0) return true;
    if(glm::dot(a.shading_normal, b.shading_normal) < 0.75f) return false;
    float d2 = glm::dot(a.hit_position - b.hit_position, a.hit_position - b.hit_position);
    if(d2 >= 0.02f) return false;
    float depth_tolerance = glm::max(0.025f, 0.03f * glm::max(a_depth, b_depth));
    return glm::abs(a_depth - b_depth) <= depth_tolerance;
}

Color albedoFromPath(const PathResult& path) {
    if(path.object_id == 0) return Color(1.0f);
    return glm::clamp(path.subsurface_albedo + 0.25f * path.specular_color, Color(0.0f), Color(1.0f));
}

glm::vec3 normalFromPath(const PathResult& path) {
    if(path.object_id == 0) return glm::vec3(0.0f, 1.0f, 0.0f);
    return glm::normalize(path.shading_normal);
}

bool runOIDNDenoise(
    int width,
    int height,
    const std::vector<Color>& color,
    const std::vector<Color>& albedo,
    const std::vector<Color>& normal,
    std::vector<Color>& output,
    std::string& error
) {
#if defined(ASTRATRACE_HAS_OIDN) && ASTRATRACE_HAS_OIDN
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<float> color_data(count * 3u);
    std::vector<float> albedo_data(count * 3u);
    std::vector<float> normal_data(count * 3u);
    std::vector<float> output_data(count * 3u);
    for(std::size_t i = 0; i < count; ++i) {
        const Color c = glm::max(color[i], Color(0.0f));
        const Color a = glm::clamp(albedo[i], Color(0.0f), Color(1.0f));
        const glm::vec3 n = glm::clamp(normal[i], glm::vec3(-1.0f), glm::vec3(1.0f));
        color_data[i * 3u + 0u] = c.r;
        color_data[i * 3u + 1u] = c.g;
        color_data[i * 3u + 2u] = c.b;
        albedo_data[i * 3u + 0u] = a.r;
        albedo_data[i * 3u + 1u] = a.g;
        albedo_data[i * 3u + 2u] = a.b;
        normal_data[i * 3u + 0u] = n.x;
        normal_data[i * 3u + 1u] = n.y;
        normal_data[i * 3u + 2u] = n.z;
    }

    OIDNDevice device = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
    if(!device) {
        error = "failed to create OIDN device";
        return false;
    }
    oidnCommitDevice(device);

    OIDNFilter filter = oidnNewFilter(device, "RT");
    oidnSetSharedFilterImage(filter, "color", color_data.data(), OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
    oidnSetSharedFilterImage(filter, "albedo", albedo_data.data(), OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
    oidnSetSharedFilterImage(filter, "normal", normal_data.data(), OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
    oidnSetSharedFilterImage(filter, "output", output_data.data(), OIDN_FORMAT_FLOAT3, width, height, 0, 0, 0);
    oidnSetFilterBool(filter, "hdr", true);
    oidnCommitFilter(filter);
    oidnExecuteFilter(filter);

    const char* message = nullptr;
    OIDNError oidn_error = oidnGetDeviceError(device, &message);
    oidnReleaseFilter(filter);
    oidnReleaseDevice(device);
    if(oidn_error != OIDN_ERROR_NONE) {
        error = message ? message : "OIDN filter failed";
        return false;
    }

    output.resize(count);
    for(std::size_t i = 0; i < count; ++i) {
        output[i] = Color(
            output_data[i * 3u + 0u],
            output_data[i * 3u + 1u],
            output_data[i * 3u + 2u]
        );
    }
    return true;
#else
    (void)width;
    (void)height;
    (void)color;
    (void)albedo;
    (void)normal;
    (void)output;
    error = "OIDN not available in this build";
    return false;
#endif
}

}

void CpuPathRenderer::ensureBuffers(int width, int height) {
    if(width == buffer_width && height == buffer_height) return;
    buffer_width = width;
    buffer_height = height;
    std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    accumulation.assign(count, Color(0.0f));
    oidn_albedo_accumulation.assign(count, Color(0.0f));
    oidn_normal_accumulation.assign(count, Color(0.0f));
    svgf_history.assign(count, {});
    svgf_current.assign(count, {});
    svgf_filter_a.assign(count, Color(0.0f));
    svgf_filter_b.assign(count, Color(0.0f));
    accumulated_samples = 0;
    next_sample_index = 0;
    has_previous_camera = false;
    progress = {};
    status_note.clear();
}

void CpuPathRenderer::clearHistory() {
    std::fill(accumulation.begin(), accumulation.end(), Color(0.0f));
    std::fill(oidn_albedo_accumulation.begin(), oidn_albedo_accumulation.end(), Color(0.0f));
    std::fill(oidn_normal_accumulation.begin(), oidn_normal_accumulation.end(), Color(0.0f));
    std::fill(svgf_history.begin(), svgf_history.end(), HistoryPixel{});
    std::fill(svgf_current.begin(), svgf_current.end(), HistoryPixel{});
    std::fill(svgf_filter_a.begin(), svgf_filter_a.end(), Color(0.0f));
    std::fill(svgf_filter_b.begin(), svgf_filter_b.end(), Color(0.0f));
    accumulated_samples = 0;
    next_sample_index = 0;
    has_previous_camera = false;
    status_note.clear();
    progress.accumulated_samples = 0;
    progress.canceled = false;
}

void CpuPathRenderer::reset() {
    clearHistory();
}

std::string CpuPathRenderer::getStatus() const {
    char text[256];
    std::snprintf(
        text,
        sizeof(text),
        "CPU path: %u accumulated sample%s, %d tiles @ %dx%d, last frame %.2f ms%s%s",
        accumulated_samples,
        accumulated_samples == 1 ? "" : "s",
        progress.tile_count,
        progress.width,
        progress.height,
        progress.last_frame_ms,
        status_note.empty() ? "" : " - ",
        status_note.c_str()
    );
    return text;
}

render::RenderProgress CpuPathRenderer::getProgress() const {
    return progress;
}

void CpuPathRenderer::render(const Scene& scene, const Camera& camera, Image<Color8>& output) {
    RenderFrameContext context{};
    PathRenderSettings settings{};
    render(scene, camera, output, context, settings);
}

std::vector<CpuPathRenderer::Tile> CpuPathRenderer::buildTiles(int width, int height) const {
    constexpr int TILE_SIZE = 16;
    std::vector<Tile> tiles;
    tiles.reserve(
        static_cast<std::size_t>((width + TILE_SIZE - 1) / TILE_SIZE) *
        static_cast<std::size_t>((height + TILE_SIZE - 1) / TILE_SIZE)
    );
    for(int y = 0; y < height; y += TILE_SIZE) {
        for(int x = 0; x < width; x += TILE_SIZE) {
            tiles.push_back({
                x,
                y,
                glm::min(x + TILE_SIZE, width),
                glm::min(y + TILE_SIZE, height)
            });
        }
    }
    return tiles;
}

void CpuPathRenderer::filterSVGFATrous(
    const std::vector<Color>& input,
    std::vector<Color>& output,
    const std::vector<HistoryPixel>& guide,
    int width,
    int height,
    int step
) const {
    constexpr std::array<float, 5> kernel = {1.0f / 16.0f, 1.0f / 4.0f, 3.0f / 8.0f, 1.0f / 4.0f, 1.0f / 16.0f};
    output.assign(input.size(), Color(0.0f));

    #pragma omp parallel for schedule(static)
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            const int center_index = y * width + x;
            const HistoryPixel& center = guide[static_cast<std::size_t>(center_index)];
            Color sum(0.0f);
            float weight_sum = 0.0f;

            for(int oy = -2; oy <= 2; ++oy) {
                const int sy = glm::clamp(y + oy * step, 0, height - 1);
                for(int ox = -2; ox <= 2; ++ox) {
                    const int sx = glm::clamp(x + ox * step, 0, width - 1);
                    const int sample_index = sy * width + sx;
                    const HistoryPixel& sample = guide[static_cast<std::size_t>(sample_index)];

                    float object_weight = center.path.object_id == sample.path.object_id ? 1.0f : 0.0f;
                    if(center.path.object_id == 0 && sample.path.object_id == 0) object_weight = 1.0f;
                    if(object_weight <= 0.0f) continue;

                    float normal_weight = 1.0f;
                    if(center.path.object_id != 0 && sample.path.object_id != 0) {
                        normal_weight = glm::pow(glm::max(0.0f, glm::dot(center.normal, sample.normal)), 16.0f);
                    }

                    const float depth_scale = glm::max(0.04f, 0.04f * glm::max(center.depth, sample.depth));
                    const float depth_weight = std::exp(-glm::abs(center.depth - sample.depth) / depth_scale);
                    const Color albedo_delta = glm::abs(center.albedo - sample.albedo);
                    const float albedo_max = glm::max(albedo_delta.r, glm::max(albedo_delta.g, albedo_delta.b));
                    const float albedo_weight = std::exp(-8.0f * albedo_max);
                    const float center_lum = luminance(input[static_cast<std::size_t>(center_index)]);
                    const float sample_lum = luminance(input[static_cast<std::size_t>(sample_index)]);
                    const float lum_scale = glm::max(0.03f, 0.22f * glm::max(center_lum, sample_lum));
                    const float luminance_weight = std::exp(-glm::abs(center_lum - sample_lum) / lum_scale);
                    const float variance_weight = 1.0f / (1.0f + 8.0f * glm::max(center.variance, sample.variance));
                    const float spatial = kernel[static_cast<std::size_t>(ox + 2)] *
                        kernel[static_cast<std::size_t>(oy + 2)];
                    const float weight =
                        spatial *
                        object_weight *
                        normal_weight *
                        depth_weight *
                        albedo_weight *
                        luminance_weight *
                        variance_weight;
                    sum += input[static_cast<std::size_t>(sample_index)] * weight;
                    weight_sum += weight;
                }
            }

            output[static_cast<std::size_t>(center_index)] =
                weight_sum > 0.0f ? sum / weight_sum : input[static_cast<std::size_t>(center_index)];
        }
    }
}

int CpuPathRenderer::findReprojectedHistoryIndex(
    const PathResult& current,
    int current_index,
    int width,
    int height,
    bool camera_changed
) const {
    if(!camera_changed) return current_index;
    if(!has_previous_camera || current.object_id == 0) return -1;

    glm::vec2 previous_uv(0.0f);
    float previous_depth = 0.0f;
    if(!previous_camera.projectWorldToUV(current.hit_position, previous_uv, previous_depth)) {
        return -1;
    }

    int previous_x = glm::clamp(static_cast<int>(previous_uv.x * static_cast<float>(width)), 0, width - 1);
    int previous_y = glm::clamp(static_cast<int>(previous_uv.y * static_cast<float>(height)), 0, height - 1);
    return previous_y * width + previous_x;
}

void CpuPathRenderer::render(
    const Scene& scene,
    const Camera& camera,
    Image<Color8>& output,
    const render::RenderFrameContext& context,
    const render::PathRenderSettings& path_settings
) {
    const auto render_start = std::chrono::steady_clock::now();
    const int width = output.getWidth();
    const int height = output.getHeight();
    ensureBuffers(width, height);

    PathRenderSettings settings = path_settings;
    settings.max_bounces = glm::clamp(settings.max_bounces, 1, 32);
    settings.samples_per_frame = glm::clamp(settings.samples_per_frame, 1, 64);
    status_note.clear();
    progress.canceled = false;
    auto cancelRequested = [&context]() {
        return context.cancel_requested && context.cancel_requested->load(std::memory_order_relaxed);
    };
    bool oidn_unavailable = false;
    if(settings.denoiser == PathDenoiserMode::OIDN && !render::isOIDNAvailable()) {
        oidn_unavailable = true;
        settings.denoiser = PathDenoiserMode::Temporal;
    }

    PathRenderSettings comparable_settings = settings;
    comparable_settings.reset_requested = false;
    comparable_settings.samples_per_frame = 1;
    bool settings_changed = context.settings_changed || !(comparable_settings == last_settings);
    bool reset_for_camera = context.camera_changed && settings.denoiser == PathDenoiserMode::Temporal;
    if(settings.reset_requested || context.scene_changed || settings_changed || reset_for_camera) {
        clearHistory();
    }
    if(oidn_unavailable) {
        status_note = "OIDN not available in this build; using temporal accumulation";
    }
    last_settings = comparable_settings;

    const std::size_t pixel_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::vector<Tile> tiles = buildTiles(width, height);
    progress.width = width;
    progress.height = height;
    progress.tile_count = static_cast<int>(tiles.size());
    std::vector<PathResult> current(pixel_count);
    std::vector<Color> current_color(pixel_count, Color(0.0f));
    std::vector<Color> current_albedo;
    std::vector<Color> current_normal;
    if(settings.denoiser == PathDenoiserMode::OIDN) {
        current_albedo.assign(pixel_count, Color(0.0f));
        current_normal.assign(pixel_count, Color(0.0f));
    }
    glm::vec2 frame_jitter = settings.enable_taa
        ? Sampler::halton2D(static_cast<uint32_t>(context.frame_index + 1u))
        : glm::vec2(0.5f);

    auto finishFrame = [&]() {
        progress.accumulated_samples = accumulated_samples;
        const auto render_end = std::chrono::steady_clock::now();
        progress.last_frame_ms = std::chrono::duration<double, std::milli>(render_end - render_start).count();
        previous_camera = camera;
        has_previous_camera = true;
    };

    if(cancelRequested()) {
        status_note = "render canceled";
        progress.canceled = true;
        finishFrame();
        return;
    }

    bool canceled_frame = false;
    for(int sample = 0; sample < settings.samples_per_frame; ++sample) {
        if(cancelRequested()) {
            canceled_frame = true;
            break;
        }
        uint32_t sample_index = static_cast<uint32_t>(next_sample_index + static_cast<uint64_t>(sample));

        #pragma omp parallel for schedule(dynamic, 1)
        for(int tile_index = 0; tile_index < static_cast<int>(tiles.size()); ++tile_index) {
            if(cancelRequested()) continue;
            const Tile& tile = tiles[static_cast<std::size_t>(tile_index)];
            for(int y = tile.y0; y < tile.y1; ++y) {
                for(int x = tile.x0; x < tile.x1; ++x) {
                    if(cancelRequested()) continue;
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
                    const std::size_t idx = static_cast<std::size_t>(index);
                    current[idx] = result;
                    current_color[idx] += sanitize(result.radiance());
                    if(settings.denoiser == PathDenoiserMode::OIDN) {
                        current_albedo[idx] += albedoFromPath(result);
                        current_normal[idx] += normalFromPath(result);
                    }
                }
            }
        }
        if(cancelRequested()) {
            canceled_frame = true;
            break;
        }
    }

    if(canceled_frame) {
        status_note = "render canceled";
        progress.canceled = true;
        finishFrame();
        return;
    }

    next_sample_index += static_cast<uint64_t>(settings.samples_per_frame);
    const uint32_t new_accumulated_samples = glm::min<uint32_t>(
        accumulated_samples + static_cast<uint32_t>(settings.samples_per_frame),
        0xffffffffu
    );
    std::vector<Color> current_average = current_color;
    for(Color& color : current_average) {
        color /= static_cast<float>(settings.samples_per_frame);
    }

    Color8* pixels = output.getPixels();

    if(settings.denoiser == PathDenoiserMode::None) {
        #pragma omp parallel for schedule(static)
        for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
            const std::size_t idx = static_cast<std::size_t>(i);
            Color color = current_average[idx];
            if(settings.accumulate_samples) {
                accumulation[idx] += current_color[idx];
                color = accumulation[idx] / static_cast<float>(new_accumulated_samples);
            }
            pixels[i] = encodeColor(tonemap_aces(color));
        }
        if(settings.accumulate_samples) accumulated_samples = new_accumulated_samples;
        finishFrame();
        return;
    }

    if(settings.denoiser == PathDenoiserMode::Temporal) {
        #pragma omp parallel for schedule(static)
        for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
            const std::size_t idx = static_cast<std::size_t>(i);
            accumulation[idx] += current_color[idx];
            Color average = accumulation[idx] / static_cast<float>(new_accumulated_samples);
            pixels[i] = encodeColor(tonemap_aces(average));
        }
        accumulated_samples = new_accumulated_samples;
        finishFrame();
        return;
    }

    if(settings.denoiser == PathDenoiserMode::OIDN) {
        std::vector<Color> denoised;
        std::string oidn_error;
        #pragma omp parallel for schedule(static)
        for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
            const std::size_t idx = static_cast<std::size_t>(i);
            accumulation[idx] += current_color[idx];
            oidn_albedo_accumulation[idx] += current_albedo[idx];
            oidn_normal_accumulation[idx] += current_normal[idx];
        }

        std::vector<Color> average_color(pixel_count);
        std::vector<Color> average_albedo(pixel_count);
        std::vector<Color> average_normal(pixel_count);
        #pragma omp parallel for schedule(static)
        for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
            const std::size_t idx = static_cast<std::size_t>(i);
            average_color[idx] = accumulation[idx] / static_cast<float>(new_accumulated_samples);
            average_albedo[idx] = glm::clamp(
                oidn_albedo_accumulation[idx] / static_cast<float>(new_accumulated_samples),
                Color(0.0f),
                Color(1.0f)
            );
            glm::vec3 normal = oidn_normal_accumulation[idx] / static_cast<float>(new_accumulated_samples);
            average_normal[idx] = glm::length(normal) > 1e-5f ? glm::normalize(normal) : Color(0.0f, 1.0f, 0.0f);
        }

        if(!runOIDNDenoise(width, height, average_color, average_albedo, average_normal, denoised, oidn_error)) {
            status_note = oidn_error.empty() ? "OIDN failed; showing temporal accumulation" : oidn_error;
            denoised = std::move(average_color);
        }

        #pragma omp parallel for schedule(static)
        for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
            pixels[i] = encodeColor(tonemap_aces(denoised[static_cast<std::size_t>(i)]));
        }
        accumulated_samples = new_accumulated_samples;
        finishFrame();
        return;
    }

    #pragma omp parallel for schedule(static)
    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            int index = y * width + x;
            std::size_t idx = static_cast<std::size_t>(index);
            Color spatial = current_average[idx];
            int history_index = findReprojectedHistoryIndex(
                current[idx],
                index,
                width,
                height,
                context.camera_changed
            );
            const HistoryPixel* history = history_index >= 0
                ? &svgf_history[static_cast<std::size_t>(history_index)]
                : nullptr;
            float current_depth = historyDepth(current[idx], camera);
            Color current_albedo_value = albedoFromPath(current[idx]);
            glm::vec3 current_normal_value = normalFromPath(current[idx]);
            bool has_compatible_history =
                history &&
                history->frames > 0 &&
                compatibleHistory(current[idx], history->path, current_depth, history->depth) &&
                (current[idx].object_id == 0 || glm::dot(current_normal_value, history->normal) >= 0.75f);
            if(has_compatible_history) {
                const Color albedo_delta = glm::abs(current_albedo_value - history->albedo);
                has_compatible_history =
                    glm::max(albedo_delta.r, glm::max(albedo_delta.g, albedo_delta.b)) < 0.35f;
            }

            float alpha = 0.18f;
            if(!has_compatible_history) {
                alpha = 1.0f;
            } else {
                float lum_now = luminance(spatial);
                float lum_history = luminance(history->color);
                float diff = glm::abs(lum_now - lum_history);
                alpha = glm::clamp(0.06f + diff * 0.10f + history->variance * 0.05f, 0.06f, 0.65f);
            }

            Color history_color = has_compatible_history ? history->color : Color(0.0f);
            Color blended = alpha * spatial + (1.0f - alpha) * history_color;
            float lum_now = luminance(spatial);
            float moment1 = lum_now;
            float moment2 = lum_now * lum_now;
            if(has_compatible_history) {
                moment1 = alpha * lum_now + (1.0f - alpha) * history->moment1;
                moment2 = alpha * lum_now * lum_now + (1.0f - alpha) * history->moment2;
            }
            svgf_current[idx].path = current[idx];
            svgf_current[idx].color = sanitize(blended);
            svgf_current[idx].albedo = current_albedo_value;
            svgf_current[idx].normal = current_normal_value;
            svgf_current[idx].depth = current_depth;
            svgf_current[idx].moment1 = moment1;
            svgf_current[idx].moment2 = moment2;
            svgf_current[idx].variance = glm::max(0.0f, moment2 - moment1 * moment1);
            svgf_current[idx].frames = has_compatible_history ? history->frames + 1u : 1u;
            svgf_filter_a[idx] = svgf_current[idx].color;
        }
    }

    for(int pass = 0; pass < 3; ++pass) {
        filterSVGFATrous(svgf_filter_a, svgf_filter_b, svgf_current, width, height, 1 << pass);
        svgf_filter_a.swap(svgf_filter_b);
    }

    #pragma omp parallel for schedule(static)
    for(int i = 0; i < static_cast<int>(pixel_count); ++i) {
        const std::size_t idx = static_cast<std::size_t>(i);
        const Color filtered = sanitize(svgf_filter_a[idx]);
        const Color history_ready = svgf_current[idx].color;
        const float filter_weight = glm::clamp(0.72f - 0.18f * svgf_current[idx].variance, 0.45f, 0.72f);
        const Color output_color = glm::mix(history_ready, filtered, filter_weight);
        pixels[i] = encodeColor(tonemap_aces(output_color));
    }

    svgf_history.swap(svgf_current);
    accumulated_samples = new_accumulated_samples;
    finishFrame();
}

} // namespace render::cpu
