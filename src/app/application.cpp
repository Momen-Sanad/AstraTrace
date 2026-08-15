#include "app/application.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include "app/frame_loop.hpp"
#include "app/input/camera_controller.hpp"
#include "app/runtime/timer.hpp"
#include "core/color.hpp"
#include "core/image.hpp"
#include "core/image_io.hpp"
#include "io/gltf/gltf_loader.hpp"
#include "render/renderer.hpp"
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"
#include "scene/world/scene_showcase.hpp"

namespace {

constexpr const char* WINDOW_TITLE = "AstraTrace";
constexpr int DEFAULT_WINDOW_WIDTH = 640;
constexpr int DEFAULT_WINDOW_HEIGHT = 360;
constexpr int EXPORT_PATH_BATCH_SAMPLES = 8;

volatile std::sig_atomic_t g_export_cancel_signal = 0;

void handleExportCancelSignal(int) {
    g_export_cancel_signal = 1;
}

struct AppConfig {
    std::string scene_path;
    bool use_material_showcase = false;
    render::RenderBackend backend = render::RenderBackend::CpuWhitted;
    bool export_mode = false;
    std::filesystem::path export_path;
    int width = DEFAULT_WINDOW_WIDTH;
    int height = DEFAULT_WINDOW_HEIGHT;
    int samples = 64;
    int bounces = 5;
    std::filesystem::path environment_path;
    float environment_strength = 1.0f;
    render::PathDenoiserMode denoiser = render::PathDenoiserMode::Temporal;
    render::PathSamplerMode sampler = render::PathSamplerMode::Sobol;
    render::PathLightSamplerMode light_sampler = render::PathLightSamplerMode::PartialBRDF;
};

std::string toLowerAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

bool parsePositiveInt(const std::string& value, int& out) {
    try {
        std::size_t consumed = 0;
        int parsed = std::stoi(value, &consumed);
        if(consumed != value.size() || parsed <= 0) return false;
        out = parsed;
        return true;
    } catch(...) {
        return false;
    }
}

bool parseNonNegativeFloat(const std::string& value, float& out) {
    try {
        std::size_t consumed = 0;
        float parsed = std::stof(value, &consumed);
        if(consumed != value.size() || parsed < 0.0f || !std::isfinite(parsed)) return false;
        out = parsed;
        return true;
    } catch(...) {
        return false;
    }
}

bool parseBackend(const std::string& value, render::RenderBackend& backend) {
    std::string lower = toLowerAscii(value);
    if(lower == "cpu-whitted") {
        backend = render::RenderBackend::CpuWhitted;
        return true;
    }
    if(lower == "cpu-path") {
        backend = render::RenderBackend::CpuPath;
        return true;
    }
    return false;
}

bool parseDenoiser(const std::string& value, render::PathDenoiserMode& denoiser) {
    std::string lower = toLowerAscii(value);
    if(lower == "none" || lower == "noop" || lower == "no-op") {
        denoiser = render::PathDenoiserMode::None;
        return true;
    }
    if(lower == "temporal") {
        denoiser = render::PathDenoiserMode::Temporal;
        return true;
    }
    if(lower == "svgf") {
        denoiser = render::PathDenoiserMode::SVGF;
        return true;
    }
    if(lower == "oidn") {
        denoiser = render::PathDenoiserMode::OIDN;
        return true;
    }
    return false;
}

bool parseSampler(const std::string& value, render::PathSamplerMode& sampler) {
    std::string lower = toLowerAscii(value);
    if(lower == "random") {
        sampler = render::PathSamplerMode::Random;
        return true;
    }
    if(lower == "halton") {
        sampler = render::PathSamplerMode::Halton;
        return true;
    }
    if(lower == "sobol") {
        sampler = render::PathSamplerMode::Sobol;
        return true;
    }
    return false;
}

bool parseLightSampler(const std::string& value, render::PathLightSamplerMode& sampler) {
    std::string lower = toLowerAscii(value);
    if(lower == "uniform") {
        sampler = render::PathLightSamplerMode::Uniform;
        return true;
    }
    if(lower == "power") {
        sampler = render::PathLightSamplerMode::Power;
        return true;
    }
    if(lower == "contribution" || lower == "partial-brdf" || lower == "partial_brdf") {
        sampler = render::PathLightSamplerMode::PartialBRDF;
        return true;
    }
    return false;
}

bool requireValue(int index, int argc, const std::string& option, std::string& error) {
    if(index + 1 < argc) return true;
    error = "Missing value for " + option;
    return false;
}

bool frameCameraToScene(const Scene& scene, Camera& camera, float aspect_ratio) {
    const auto& objects = scene.getObjects();
    if(objects.empty()) return false;

    AABB bounds;
    bounds.min = glm::vec3(std::numeric_limits<float>::max());
    bounds.max = glm::vec3(std::numeric_limits<float>::lowest());
    for(const auto& object : objects) {
        AABB object_bounds = object->getBounds();
        bounds.min = glm::min(bounds.min, object_bounds.min);
        bounds.max = glm::max(bounds.max, object_bounds.max);
    }

    glm::vec3 center = 0.5f * (bounds.min + bounds.max);
    glm::vec3 extent = bounds.max - bounds.min;
    float radius = glm::max(0.5f * glm::length(extent), 0.05f);
    glm::vec3 position = center + glm::vec3(0.0f, 0.25f * radius, 2.4f * radius);
    glm::vec3 forward = glm::normalize(center - position);

    camera.setPosition(position);
    camera.setRotation(forward, glm::vec3(0.0f, 1.0f, 0.0f));
    camera.setHalfSize(glm::radians(50.0f), aspect_ratio);
    return true;
}

bool parseArgs(int argc, char** argv, AppConfig& config, std::string& error) {
    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--backend") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parseBackend(argv[++i], config.backend)) {
                error = "Invalid backend. Expected cpu-whitted or cpu-path.";
                return false;
            }
        } else if(arg == "--showcase") {
            if(!requireValue(i, argc, arg, error)) return false;
            std::string showcase = toLowerAscii(argv[++i]);
            if(showcase != "material") {
                error = "Invalid showcase. Expected material.";
                return false;
            }
            config.use_material_showcase = true;
        } else if(arg == "--export") {
            if(!requireValue(i, argc, arg, error)) return false;
            config.export_mode = true;
            config.export_path = argv[++i];
        } else if(arg == "--width") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parsePositiveInt(argv[++i], config.width)) {
                error = "Invalid width. Expected a positive integer.";
                return false;
            }
        } else if(arg == "--height") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parsePositiveInt(argv[++i], config.height)) {
                error = "Invalid height. Expected a positive integer.";
                return false;
            }
        } else if(arg == "--samples") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parsePositiveInt(argv[++i], config.samples)) {
                error = "Invalid samples. Expected a positive integer.";
                return false;
            }
        } else if(arg == "--bounces") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parsePositiveInt(argv[++i], config.bounces)) {
                error = "Invalid bounces. Expected a positive integer.";
                return false;
            }
        } else if(arg == "--environment") {
            if(!requireValue(i, argc, arg, error)) return false;
            config.environment_path = argv[++i];
        } else if(arg == "--environment-strength") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parseNonNegativeFloat(argv[++i], config.environment_strength)) {
                error = "Invalid environment strength. Expected a non-negative number.";
                return false;
            }
        } else if(arg == "--denoiser") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parseDenoiser(argv[++i], config.denoiser)) {
                error = "Invalid denoiser. Expected none, temporal, svgf, or oidn.";
                return false;
            }
        } else if(arg == "--sampler") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parseSampler(argv[++i], config.sampler)) {
                error = "Invalid sampler. Expected random, halton, or sobol.";
                return false;
            }
        } else if(arg == "--light-sampler") {
            if(!requireValue(i, argc, arg, error)) return false;
            if(!parseLightSampler(argv[++i], config.light_sampler)) {
                error = "Invalid light sampler. Expected uniform, power, or contribution.";
                return false;
            }
        } else if(!arg.empty() && arg[0] == '-') {
            error = "Unknown option: " + arg;
            return false;
        } else if(config.scene_path.empty()) {
            config.scene_path = arg;
        } else {
            error = "Unexpected extra positional argument: " + arg;
            return false;
        }
    }

    if(config.use_material_showcase && !config.scene_path.empty()) {
        error = "Specify either a scene path or --showcase material, not both.";
        return false;
    }
    if(!config.use_material_showcase && config.scene_path.empty()) {
        error = "Missing scene path or --showcase material.";
        return false;
    }
    if(config.export_mode && config.export_path.empty()) {
        error = "Missing export output path.";
        return false;
    }
    return true;
}

void printUsage(const char* executable) {
    SDL_Log(
        "Usage:\n"
        "  %s <scene.gltf | scene.glb> [--backend cpu-whitted|cpu-path]\n"
        "  %s --showcase material [--backend cpu-whitted|cpu-path]\n"
        "  %s <scene | --showcase material> --export <out.png> [--width 1280] [--height 720] "
        "[--samples 64] [--bounces 5] [--denoiser none|temporal|svgf|oidn] [--sampler random|halton|sobol] "
        "[--light-sampler uniform|power|contribution] [--environment <image.hdr|png|jpg>] "
        "[--environment-strength 1.0]",
        executable,
        executable,
        executable
    );
}

bool prepareScene(
    const AppConfig& config,
    Scene& scene,
    Camera& camera,
    float aspect_ratio,
    std::string& active_scene_label
) {
    scene.clear();
    camera.setHalfSize(glm::radians(90.0f), aspect_ratio);

    if(config.use_material_showcase) {
        buildMaterialShowcaseScene(scene, camera, aspect_ratio);
        active_scene_label = MATERIAL_SHOWCASE_SCENE_ID;
        if(!config.environment_path.empty()) {
            auto environment = loadColorImageAnyDepth(config.environment_path.string().c_str(), true);
            if(!environment) {
                SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load environment image: %s", config.environment_path.string().c_str());
                return false;
            }
            scene.setEnvironmentImage(environment, config.environment_strength);
        }
        return true;
    }

    GltfSceneLoadResult load_result = io::gltf::loadSceneFromGLTF(
        config.scene_path,
        scene,
        camera,
        aspect_ratio
    );

    if(!load_result.warning.empty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GLTF warning(s):\n%s", load_result.warning.c_str());
    }
    if(!load_result.success) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GLTF load failed: %s", load_result.error.c_str());
        return false;
    }

    scene.update();
    if(!load_result.camera_loaded) {
        frameCameraToScene(scene, camera, aspect_ratio);
    } else {
        camera.setHalfSize(camera.getFovY(), aspect_ratio);
    }
    if(!config.environment_path.empty()) {
        auto environment = loadColorImageAnyDepth(config.environment_path.string().c_str(), true);
        if(!environment) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to load environment image: %s", config.environment_path.string().c_str());
            return false;
        }
        scene.setEnvironmentImage(environment, config.environment_strength);
        SDL_Log("Loaded environment: %s (strength %.3f)", config.environment_path.string().c_str(), config.environment_strength);
    }
    active_scene_label = config.scene_path;
    SDL_Log(
        "Loaded Scene: Objects=%llu Lights=%llu CameraFromGLTF=%s",
        static_cast<unsigned long long>(load_result.object_count),
        static_cast<unsigned long long>(load_result.light_count),
        load_result.camera_loaded ? "true" : "false"
    );
    return true;
}

struct AsyncExportState {
    SDL_Mutex* mutex = nullptr;
    render::RenderProgress progress;
    int requested_samples = 0;
    int completed_samples = 0;
    bool done = false;
    bool canceled = false;
    bool success = false;
    std::string error;
    std::string status;
};

class SdlMutexLock {
public:
    explicit SdlMutexLock(SDL_Mutex* mutex) : mutex(mutex) {
        if(mutex) SDL_LockMutex(mutex);
    }
    ~SdlMutexLock() {
        if(mutex) SDL_UnlockMutex(mutex);
    }

private:
    SDL_Mutex* mutex = nullptr;
};

struct AsyncExportJob {
    const AppConfig* config = nullptr;
    const Scene* scene = nullptr;
    const Camera* camera = nullptr;
    Image<Color8>* buffer = nullptr;
    std::atomic_bool* cancel_requested = nullptr;
    AsyncExportState* state = nullptr;
};

int SDLCALL runAsyncPathExportWorker(void* data) {
    auto* job = static_cast<AsyncExportJob*>(data);
    const AppConfig& config = *job->config;
    const Scene& scene = *job->scene;
    const Camera& camera = *job->camera;
    Image<Color8>& buffer = *job->buffer;
    std::atomic_bool& cancel_requested = *job->cancel_requested;
    AsyncExportState& state = *job->state;

    std::unique_ptr<render::IRenderer> renderer = render::createRenderer(render::RenderBackend::CpuPath);
    if(!renderer) {
        SdlMutexLock lock(state.mutex);
        state.done = true;
        state.error = "Failed to create CPU path renderer.";
        return 1;
    }

    render::PathRenderSettings settings;
    settings.sampler = config.sampler;
    settings.light_sampler = config.light_sampler;
    settings.denoiser = config.denoiser;
    settings.max_bounces = config.bounces;
    settings.accumulate_samples = config.denoiser != render::PathDenoiserMode::SVGF;

    int remaining = config.samples;
    uint64_t frame_index = 0;
    while(remaining > 0 && !cancel_requested.load(std::memory_order_relaxed)) {
        settings.samples_per_frame = std::min(remaining, EXPORT_PATH_BATCH_SAMPLES);
        render::RenderFrameContext context{
            .frame_index = frame_index,
            .delta_time = 0.0f,
            .camera_changed = false,
            .scene_changed = frame_index == 0,
            .settings_changed = frame_index == 0,
            .cancel_requested = &cancel_requested
        };
        renderer->render(scene, camera, buffer, context, settings);

        render::RenderProgress progress = renderer->getProgress();
        if(progress.canceled || cancel_requested.load(std::memory_order_relaxed)) {
            SdlMutexLock lock(state.mutex);
            state.done = true;
            state.canceled = true;
            state.progress = progress;
            state.status = renderer->getStatus();
            return 0;
        }

        remaining -= settings.samples_per_frame;
        ++frame_index;
        {
            SdlMutexLock lock(state.mutex);
            state.completed_samples = config.samples - remaining;
            state.progress = progress;
            state.status = renderer->getStatus();
        }
    }

    if(cancel_requested.load(std::memory_order_relaxed)) {
        SdlMutexLock lock(state.mutex);
        state.done = true;
        state.canceled = true;
        return 0;
    }

    std::string save_error;
    if(!savePng(buffer, config.export_path, save_error)) {
        SdlMutexLock lock(state.mutex);
        state.done = true;
        state.error = "Failed to write PNG: " + save_error;
        return 1;
    }

    SdlMutexLock lock(state.mutex);
    state.done = true;
    state.success = true;
    return 0;
}

int runAsyncPathExport(
    const AppConfig& config,
    const Scene& scene,
    const Camera& camera,
    Image<Color8>& buffer
) {
    if(config.denoiser == render::PathDenoiserMode::OIDN && !render::isOIDNAvailable()) {
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "OIDN not available in this build. Rebuild with ASTRATRACE_ENABLE_OIDN=ON and Open Image Denoise installed."
        );
        return -1;
    }

    std::atomic_bool cancel_requested(false);
    AsyncExportState state;
    state.requested_samples = config.samples;
    state.mutex = SDL_CreateMutex();
    if(!state.mutex) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create export mutex: %s", SDL_GetError());
        return -1;
    }
    g_export_cancel_signal = 0;

    auto previous_sigint = std::signal(SIGINT, handleExportCancelSignal);
    auto previous_sigterm = std::signal(SIGTERM, handleExportCancelSignal);

    SDL_Log(
        "Started async CPU path export: %d samples, %dx%d, batches of %d. Press Ctrl+C to cancel.",
        config.samples,
        config.width,
        config.height,
        EXPORT_PATH_BATCH_SAMPLES
    );

    AsyncExportJob job{
        .config = &config,
        .scene = &scene,
        .camera = &camera,
        .buffer = &buffer,
        .cancel_requested = &cancel_requested,
        .state = &state
    };
    SDL_Thread* worker = SDL_CreateThread(runAsyncPathExportWorker, "AstraTrace Export", &job);
    if(!worker) {
        SDL_DestroyMutex(state.mutex);
        std::signal(SIGINT, previous_sigint);
        std::signal(SIGTERM, previous_sigterm);
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create export thread: %s", SDL_GetError());
        return -1;
    }

    int last_logged_samples = -1;
    auto last_log_time = std::chrono::steady_clock::now();
    bool done = false;
    while(!done) {
        if(g_export_cancel_signal) {
            cancel_requested.store(true, std::memory_order_relaxed);
        }

        int completed_samples = 0;
        int requested_samples = 0;
        bool canceled = false;
        std::string status;
        {
            SdlMutexLock lock(state.mutex);
            completed_samples = state.completed_samples;
            requested_samples = state.requested_samples;
            canceled = state.canceled;
            status = state.status;
            done = state.done;
        }

        const auto now = std::chrono::steady_clock::now();
        const bool time_to_log =
            std::chrono::duration<double>(now - last_log_time).count() >= 1.0;
        if(completed_samples != last_logged_samples || time_to_log || canceled || done) {
            SDL_Log(
                "Export progress: %d/%d samples%s%s",
                completed_samples,
                requested_samples,
                status.empty() ? "" : " - ",
                status.c_str()
            );
            last_logged_samples = completed_samples;
            last_log_time = now;
        }

        if(done) break;
        SDL_Delay(150);
    }

    int worker_status = 0;
    SDL_WaitThread(worker, &worker_status);
    std::signal(SIGINT, previous_sigint);
    std::signal(SIGTERM, previous_sigterm);

    bool canceled = false;
    bool success = false;
    std::string error;
    {
        SdlMutexLock lock(state.mutex);
        canceled = state.canceled;
        success = state.success;
        error = state.error;
    }
    SDL_DestroyMutex(state.mutex);
    if(canceled) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Export canceled before writing PNG.");
        return -1;
    }
    if(!success) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s", error.empty() ? "Export failed." : error.c_str());
        return -1;
    }

    SDL_Log("Exported render: %s", config.export_path.string().c_str());
    return 0;
}

int runExport(const AppConfig& config) {
    Scene scene;
    Camera camera;
    std::string active_scene_label;
    const float aspect_ratio = config.width / static_cast<float>(config.height);
    if(!prepareScene(config, scene, camera, aspect_ratio, active_scene_label)) return -1;
    scene.printStats();

    Image<Color8> buffer(config.width, config.height);
    if(config.backend == render::RenderBackend::CpuPath) {
        return runAsyncPathExport(config, scene, camera, buffer);
    }

    std::unique_ptr<render::IRenderer> renderer = render::createRenderer(config.backend);
    if(!renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create renderer.");
        return -1;
    }
    renderer->render(scene, camera, buffer);

    std::string save_error;
    if(!savePng(buffer, config.export_path, save_error)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to write PNG: %s", save_error.c_str());
        return -1;
    }

    SDL_Log("Exported render: %s", config.export_path.string().c_str());
    return 0;
}

int runInteractive(const AppConfig& config) {
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, config.width, config.height, 0);
    if(!window) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* sdl_renderer = SDL_CreateRenderer(window, nullptr);
    if(!sdl_renderer) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create SDL renderer: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    SDL_Texture* frame_texture = SDL_CreateTexture(
        sdl_renderer,
        SDL_PIXELFORMAT_ABGR8888,
        SDL_TEXTUREACCESS_STREAMING,
        config.width,
        config.height
    );
    if(!frame_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create frame texture: %s", SDL_GetError());
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    Image<Color8> buffer(config.width, config.height);
    Camera camera;
    Scene scene;
    std::string active_scene_label;
    const float aspect_ratio = config.width / static_cast<float>(config.height);
    if(!prepareScene(config, scene, camera, aspect_ratio, active_scene_label)) {
        SDL_DestroyTexture(frame_texture);
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    CameraController controller(window, camera);

    FrameLoop frame_loop(
        window,
        sdl_renderer,
        frame_texture,
        buffer,
        scene,
        camera,
        controller,
        config.backend,
        active_scene_label
    );
    int exit_code = frame_loop.run();

    SDL_DestroyTexture(frame_texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}

} // namespace

int Application::run(int argc, char** argv) {
    TimeIt timeit;
    (void)timeit;

    AppConfig config;
    std::string error;
    if(!parseArgs(argc, argv, config, error)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "%s", error.c_str());
        printUsage(argc > 0 ? argv[0] : "astratrace_app");
        return -1;
    }

    if(config.export_mode) {
        return runExport(config);
    }
    return runInteractive(config);
}
