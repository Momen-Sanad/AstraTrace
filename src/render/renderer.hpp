#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"
#include "core/color.hpp"
#include "core/image.hpp"

namespace render {

enum class RenderBackend {
    CpuWhitted,
    CpuPath,
    GpuPath,
};

enum class PathSamplerMode {
    Random,
    Halton,
    Sobol,
};

enum class PathLightSamplerMode {
    Uniform,
    Power,
    PartialBRDF,
    Tree,
};

enum class PathDenoiserMode {
    None,
    Temporal,
    SVGF,
};

struct PathRenderSettings {
    PathSamplerMode sampler = PathSamplerMode::Sobol;
    PathLightSamplerMode light_sampler = PathLightSamplerMode::Tree;
    PathDenoiserMode denoiser = PathDenoiserMode::SVGF;
    int max_bounces = 5;
    int samples_per_frame = 1;
    bool enable_nee = true;
    bool enable_mis = true;
    bool enable_russian_roulette = true;
    bool enable_path_regularization = false;
    bool enable_taa = true;
    bool reset_requested = false;

    bool operator==(const PathRenderSettings& other) const = default;
};

struct RenderFrameContext {
    uint64_t frame_index = 0;
    float delta_time = 0.0f;
    bool camera_changed = false;
    bool scene_changed = false;
    bool settings_changed = false;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(const Scene& scene, const Camera& camera, Image<Color8>& output) = 0;
    virtual void render(
        const Scene& scene,
        const Camera& camera,
        Image<Color8>& output,
        const RenderFrameContext& context,
        const PathRenderSettings& path_settings
    ) {
        (void)context;
        (void)path_settings;
        render(scene, camera, output);
    }
    virtual void reset() {}
    virtual std::string getStatus() const { return {}; }
};

std::unique_ptr<IRenderer> createRenderer(RenderBackend backend);

} // namespace render
