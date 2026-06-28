#include "app/frame_loop.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <limits>
#include <string>

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "io/gltf/gltf_loader.hpp"
#include "platform/sdl/screenshot.hpp"
#include "scene/world/scene_showcase.hpp"

namespace {
constexpr const char* WINDOW_TITLE = "AstraTrace";
constexpr const char* BACKEND_LABEL_CPU_WHITTED = "CPU Whitted";
constexpr const char* BACKEND_LABEL_CPU_PATH = "CPU Path";

std::string toLowerAscii(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return text;
}

std::string normalizePathString(const std::filesystem::path& path) {
    std::error_code ec;
    std::filesystem::path absolute = path;
    if(!absolute.is_absolute()) {
        absolute = std::filesystem::absolute(absolute, ec);
        if(ec) absolute = path;
    }
    return absolute.lexically_normal().string();
}

std::string normalizePathKey(const std::filesystem::path& path) {
    return toLowerAscii(normalizePathString(path));
}

bool isBuiltinScenePath(const std::string& path) {
    return path == MATERIAL_SHOWCASE_SCENE_ID;
}

std::string scenePathKey(const std::string& path) {
    if(isBuiltinScenePath(path)) return toLowerAscii(path);
    return normalizePathKey(std::filesystem::path(path));
}

bool isScenePath(const std::filesystem::path& path) {
    std::string ext = toLowerAscii(path.extension().string());
    return ext == ".gltf" || ext == ".glb";
}

std::filesystem::path findScenesRoot(const std::filesystem::path& active_scene_path) {
    std::error_code ec;
    if(!active_scene_path.empty()) {
        std::filesystem::path cursor = active_scene_path.parent_path();
        while(!cursor.empty() && cursor.has_relative_path()) {
            if(toLowerAscii(cursor.filename().string()) == "scenes") return cursor;
            std::filesystem::path parent = cursor.parent_path();
            if(parent == cursor) break;
            cursor = parent;
        }
    }

    std::filesystem::path cwd = std::filesystem::current_path(ec);
    if(ec) return {};
    std::filesystem::path probe = cwd;
    for(int depth = 0; depth < 6; ++depth) {
        std::filesystem::path candidate = probe / "scenes";
        if(std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec)) {
            return candidate;
        }
        if(probe == probe.root_path()) break;
        probe = probe.parent_path();
    }
    return {};
}

bool isMouseEventType(Uint32 event_type) {
    return
        event_type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event_type == SDL_EVENT_MOUSE_BUTTON_UP ||
        event_type == SDL_EVENT_MOUSE_MOTION ||
        event_type == SDL_EVENT_MOUSE_WHEEL;
}

bool isKeyboardEventType(Uint32 event_type) {
    return
        event_type == SDL_EVENT_KEY_DOWN ||
        event_type == SDL_EVENT_KEY_UP ||
        event_type == SDL_EVENT_TEXT_INPUT;
}

void takeScreenshotFromBuffer(Image<Color8>& buffer) {
    SDL_Surface* screenshot = SDL_CreateSurfaceFrom(
        buffer.getWidth(),
        buffer.getHeight(),
        SDL_PIXELFORMAT_ABGR8888,
        buffer.getPixels(),
        static_cast<int>(sizeof(Color8) * buffer.getWidth())
    );
    if(!screenshot) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create screenshot surface: %s", SDL_GetError());
        return;
    }

    takeScreenshot(screenshot);
    SDL_DestroySurface(screenshot);
}

const char* backendToLabel(render::RenderBackend backend) {
    switch(backend) {
    case render::RenderBackend::CpuPath:
        return BACKEND_LABEL_CPU_PATH;
    case render::RenderBackend::CpuWhitted:
    default:
        return BACKEND_LABEL_CPU_WHITTED;
    }
}

const char* samplerToLabel(render::PathSamplerMode sampler) {
    switch(sampler) {
    case render::PathSamplerMode::Random:
        return "Random";
    case render::PathSamplerMode::Halton:
        return "Halton";
    case render::PathSamplerMode::Sobol:
    default:
        return "Sobol";
    }
}

const char* denoiserToLabel(render::PathDenoiserMode denoiser) {
    switch(denoiser) {
    case render::PathDenoiserMode::None:
        return "NoOp";
    case render::PathDenoiserMode::Temporal:
        return "Temporal";
    case render::PathDenoiserMode::SVGF:
        return "SVGF-style";
    case render::PathDenoiserMode::OIDN:
        return "OIDN";
    default:
        return "Unknown";
    }
}

const char* lightSamplerToLabel(render::PathLightSamplerMode sampler) {
    switch(sampler) {
    case render::PathLightSamplerMode::Uniform:
        return "Uniform";
    case render::PathLightSamplerMode::Power:
        return "Power";
    case render::PathLightSamplerMode::PartialBRDF:
    default:
        return "Contribution";
    }
}

const char* enabledLabel(bool value) {
    return value ? "on" : "off";
}

std::string sceneDisplayName(const std::string& scene_path) {
    if(isBuiltinScenePath(scene_path)) return "Material Showcase";
    std::filesystem::path path(scene_path);
    std::string filename = path.filename().string();
    return filename.empty() ? scene_path : filename;
}

render::RenderBackend backendFromIndex(int index) {
    switch(index) {
    case 1:
        return render::RenderBackend::CpuPath;
    case 0:
    default:
        return render::RenderBackend::CpuWhitted;
    }
}

bool cameraStateChanged(const Camera& before, const Camera& after) {
    constexpr float POSITION_EPSILON = 1e-5f;
    constexpr float HALF_SIZE_EPSILON = 1e-6f;
    constexpr float ROTATION_DOT_EPSILON = 1e-6f;

    if(glm::length(before.getPosition() - after.getPosition()) > POSITION_EPSILON) return true;
    if(glm::length(before.getHalfSize() - after.getHalfSize()) > HALF_SIZE_EPSILON) return true;

    float rotation_dot = glm::abs(glm::dot(before.getRotation(), after.getRotation()));
    return (1.0f - rotation_dot) > ROTATION_DOT_EPSILON;
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
    float radius = glm::max(0.5f * glm::length(extent), 1.0f);
    glm::vec3 position = center + glm::vec3(0.0f, 0.25f * radius, 2.4f * radius);
    glm::vec3 forward = glm::normalize(center - position);

    camera.setPosition(position);
    camera.setRotation(forward, glm::vec3(0.0f, 1.0f, 0.0f));
    camera.setHalfSize(glm::radians(50.0f), aspect_ratio);
    return true;
}

} // namespace

void FrameLoop::logBenchmarkSnapshot(const char* event) const {
    const SceneStats stats = scene.getStats();
    const render::RenderProgress progress = renderer ? renderer->getProgress() : render::RenderProgress{};
    const bool has_frame_timing = progress.last_frame_ms > 0.0;
    const double megapixels = static_cast<double>(buffer.getWidth()) * static_cast<double>(buffer.getHeight()) / 1000000.0;
    const double frame_seconds = progress.last_frame_ms / 1000.0;
    const double mpix_per_second = has_frame_timing && frame_seconds > 0.0
        ? megapixels / frame_seconds
        : 0.0;
    const double path_msamples_per_second =
        active_backend == render::RenderBackend::CpuPath && has_frame_timing && frame_seconds > 0.0
            ? (megapixels * static_cast<double>(std::max(path_settings.samples_per_frame, 1))) / frame_seconds
            : 0.0;

    SDL_Log(
        "\n"
        "================ AstraTrace Benchmark Snapshot ================\n"
        "Event: %s\n"
        "Scene: %s\n"
        "Scene Path: %s\n"
        "Backend: %s\n"
        "Resolution: %dx%d\n"
        "Scene Stats: objects=%llu, punctualLights=%llu, pathLights=%llu, environment=%s\n"
        "TLAS: nodes=%llu, leaves=%llu, maxDepth=%d, rebuilds=%llu, refits=%llu\n"
        "TLAS Timing: lastBuild=%.3f ms, lastRefit=%.3f ms\n"
        "Renderer: %s\n"
        "Frame Timing: %s\n"
        "================================================================",
        event,
        sceneDisplayName(active_scene_path).c_str(),
        active_scene_path.c_str(),
        backendToLabel(active_backend),
        buffer.getWidth(),
        buffer.getHeight(),
        static_cast<unsigned long long>(stats.object_count),
        static_cast<unsigned long long>(stats.punctual_light_count),
        static_cast<unsigned long long>(stats.path_light_count),
        scene.hasImageEnvironment() ? "image" : "constant",
        static_cast<unsigned long long>(stats.top_level_bvh_node_count),
        static_cast<unsigned long long>(stats.top_level_bvh_leaf_count),
        stats.top_level_bvh_max_depth,
        static_cast<unsigned long long>(stats.top_level_bvh_rebuild_count),
        static_cast<unsigned long long>(stats.top_level_bvh_refit_count),
        stats.last_top_level_bvh_build_ms,
        stats.last_top_level_bvh_refit_ms,
        renderer ? renderer->getStatus().c_str() : "renderer unavailable",
        has_frame_timing ? "see throughput lines below" : "pending first rendered frame"
    );

    if(active_backend == render::RenderBackend::CpuPath) {
        SDL_Log(
            "Path Settings: samplesPerFrame=%d, maxBounces=%d, denoiser=%s, sampler=%s, lightSampler=%s, NEE=%s, MIS=%s, RR=%s, regularization=%s",
            path_settings.samples_per_frame,
            path_settings.max_bounces,
            denoiserToLabel(path_settings.denoiser),
            samplerToLabel(path_settings.sampler),
            lightSamplerToLabel(path_settings.light_sampler),
            enabledLabel(path_settings.enable_nee),
            enabledLabel(path_settings.enable_mis),
            enabledLabel(path_settings.enable_russian_roulette),
            enabledLabel(path_settings.enable_path_regularization)
        );
        SDL_Log(
            "Path Progress: accumulatedSamples=%u, tiles=%d, lastBatch=%.2f ms, throughput=%.3f Mpixel-samples/s",
            progress.accumulated_samples,
            progress.tile_count,
            progress.last_frame_ms,
            path_msamples_per_second
        );
    } else {
        SDL_Log(
            "Whitted Progress: lastFrame=%.2f ms, throughput=%.3f Mpixels/s",
            progress.last_frame_ms,
            mpix_per_second
        );
    }
}

void FrameLoop::refreshSceneList() {
    std::filesystem::path active_path = isBuiltinScenePath(active_scene_path)
        ? std::filesystem::path()
        : std::filesystem::path(active_scene_path);
    const std::string active_key = scenePathKey(active_scene_path);
    std::filesystem::path scenes_root = findScenesRoot(active_path);

    scene_paths.clear();
    scene_labels.clear();
    selected_scene_index = -1;
    scene_paths.push_back(MATERIAL_SHOWCASE_SCENE_ID);

    if(!scenes_root.empty()) {
        std::error_code ec;
        for(const std::filesystem::directory_entry& entry :
            std::filesystem::recursive_directory_iterator(scenes_root, ec)) {
            if(ec) break;
            if(!entry.is_regular_file()) continue;
            if(!isScenePath(entry.path())) continue;
            scene_paths.push_back(normalizePathString(entry.path()));
        }
    }

    std::sort(scene_paths.begin(), scene_paths.end());
    scene_paths.erase(std::unique(scene_paths.begin(), scene_paths.end()), scene_paths.end());

    if(!active_scene_path.empty() && !isBuiltinScenePath(active_scene_path)) {
        auto active_it = std::find_if(scene_paths.begin(), scene_paths.end(), [&](const std::string& path) {
            return scenePathKey(path) == active_key;
        });
        if(active_it == scene_paths.end()) {
            scene_paths.push_back(normalizePathString(active_scene_path));
        }
    }

    if(!scene_paths.empty()) {
        std::sort(scene_paths.begin(), scene_paths.end());
        for(const std::string& scene_path : scene_paths) {
            std::string label;
            if(isBuiltinScenePath(scene_path)) {
                label = "Material Showcase";
            } else {
                std::filesystem::path path(scene_path);
                label = path.filename().string();
                if(!scenes_root.empty()) {
                    std::error_code ec;
                    std::filesystem::path relative = std::filesystem::relative(path, scenes_root, ec);
                    if(!ec && !relative.empty()) label = relative.string();
                }
            }
            if(!isBuiltinScenePath(scene_path) && !scenes_root.empty()) {
                std::error_code ec;
                std::filesystem::path path(scene_path);
                std::filesystem::path relative = std::filesystem::relative(path, scenes_root, ec);
                if(!ec && !relative.empty()) label = relative.string();
            }
            scene_labels.push_back(label);
        }

        auto selected_it = std::find_if(scene_paths.begin(), scene_paths.end(), [&](const std::string& path) {
            return scenePathKey(path) == active_key;
        });
        if(selected_it != scene_paths.end()) {
            selected_scene_index = static_cast<int>(std::distance(scene_paths.begin(), selected_it));
        } else {
            selected_scene_index = 0;
        }
    }
}

bool FrameLoop::reloadScene(const std::string& scene_path) {
    Scene loaded_scene;
    Camera loaded_camera;
    const float aspect_ratio = buffer.getWidth() / static_cast<float>(buffer.getHeight());
    loaded_camera.setHalfSize(glm::radians(90.0f), aspect_ratio);

    if(isBuiltinScenePath(scene_path)) {
        buildMaterialShowcaseScene(loaded_scene, loaded_camera, aspect_ratio);
        scene = std::move(loaded_scene);
        camera = loaded_camera;
        controller.resetFromCamera();

        active_scene_path = MATERIAL_SHOWCASE_SCENE_ID;
        status_message = "Loaded: Material Showcase";
        scene_changed_for_render = true;
        if(renderer) renderer->reset();

        logBenchmarkSnapshot("Scene loaded");
        pending_benchmark_event = "First rendered frame after scene load";
        pending_benchmark_log = true;
        refreshSceneList();
        return true;
    }

    GltfSceneLoadResult load_result = io::gltf::loadSceneFromGLTF(
        scene_path,
        loaded_scene,
        loaded_camera,
        aspect_ratio
    );

    if(!load_result.warning.empty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GLTF warning(s):\n%s", load_result.warning.c_str());
    }
    if(!load_result.success) {
        status_message = "Load failed: " + load_result.error;
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GLTF load failed: %s", load_result.error.c_str());
        return false;
    }

    scene = std::move(loaded_scene);
    scene.update();
    if(!load_result.camera_loaded) {
        frameCameraToScene(scene, loaded_camera, aspect_ratio);
    }
    camera = loaded_camera;
    controller.resetFromCamera();

    active_scene_path = normalizePathString(scene_path);
    status_message = "Loaded: " + active_scene_path;
    scene_changed_for_render = true;
    if(renderer) renderer->reset();

    logBenchmarkSnapshot(load_result.camera_loaded ? "Scene loaded (glTF camera)" : "Scene loaded (auto-framed camera)");
    pending_benchmark_event = "First rendered frame after scene load";
    pending_benchmark_log = true;

    refreshSceneList();
    return true;
}

bool FrameLoop::switchBackend(render::RenderBackend backend) {
    if(backend == active_backend) {
        status_message = std::string("Backend already active: ") + backendToLabel(active_backend);
        return true;
    }

    std::unique_ptr<render::IRenderer> new_renderer = render::createRenderer(backend);
    if(!new_renderer) {
        status_message = std::string("Failed to initialize backend: ") + backendToLabel(backend);
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "Failed to initialize renderer backend: %s",
            backendToLabel(backend)
        );
        return false;
    }

    renderer = std::move(new_renderer);
    renderer->reset();
    active_backend = backend;
    selected_backend = backend;
    scene_changed_for_render = true;
    status_message = std::string("Backend switched to: ") + backendToLabel(active_backend);
    logBenchmarkSnapshot("Backend switched");
    pending_benchmark_event = "First rendered frame after backend switch";
    pending_benchmark_log = true;
    return true;
}

int FrameLoop::run() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if(!ImGui_ImplSDL3_InitForSDLRenderer(window, sdl_renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize ImGui SDL3 backend.");
        ImGui::DestroyContext();
        return -1;
    }
    if(!ImGui_ImplSDLRenderer3_Init(sdl_renderer)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize ImGui SDL renderer backend.");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return -1;
    }

    if(!isBuiltinScenePath(active_scene_path)) {
        active_scene_path = normalizePathString(active_scene_path);
    }
    refreshSceneList();
    status_message = isBuiltinScenePath(active_scene_path)
        ? "Loaded: Material Showcase"
        : "Loaded: " + active_scene_path;
    logBenchmarkSnapshot("Startup scene ready");

    FPSTracker fps_tracker;
    Uint64 last_frame_time = SDL_GetTicksNS();
    bool take_screenshot = false;

    SDL_Event event;
    while(true) {
        while(SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if(event.type == SDL_EVENT_QUIT) {
                ImGui_ImplSDLRenderer3_Shutdown();
                ImGui_ImplSDL3_Shutdown();
                ImGui::DestroyContext();
                return 0;
            }

            const bool keyboard_event = isKeyboardEventType(event.type);
            const bool mouse_event = isMouseEventType(event.type);
            const ImGuiIO& io = ImGui::GetIO();

            if(
                event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_P &&
                !io.WantCaptureKeyboard
            ) {
                take_screenshot = true;
            }

            if((keyboard_event && io.WantCaptureKeyboard) || (mouse_event && io.WantCaptureMouse)) {
                continue;
            }
            controller.handleEvent(event);
        }

        Uint64 current_frame_time = SDL_GetTicksNS();
        float delta_time = static_cast<float>((current_frame_time - last_frame_time) / static_cast<double>(SDL_NS_PER_SECOND));
        last_frame_time = current_frame_time;

        Camera camera_before_update = camera;
        controller.update(delta_time);
        bool camera_changed = cameraStateChanged(camera_before_update, camera);
        scene.update();
        if(renderer) {
            render::PathRenderSettings settings_for_render = path_settings;
            render::RenderFrameContext frame_context{
                .frame_index = render_frame_index++,
                .delta_time = delta_time,
                .camera_changed = camera_changed,
                .scene_changed = scene_changed_for_render,
                .settings_changed = !(settings_for_render == previous_path_settings)
            };
            renderer->render(scene, camera, buffer, frame_context, settings_for_render);
            settings_for_render.reset_requested = false;
            path_settings.reset_requested = false;
            previous_path_settings = settings_for_render;
            scene_changed_for_render = false;
            if(pending_benchmark_log) {
                logBenchmarkSnapshot(pending_benchmark_event.c_str());
                pending_benchmark_log = false;
            }
        }

        if(!SDL_UpdateTexture(
            frame_texture,
            nullptr,
            buffer.getPixels(),
            static_cast<int>(sizeof(Color8) * buffer.getWidth())
        )) {
            SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to update frame texture: %s", SDL_GetError());
        }

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Scene Switcher");
        if(scene_labels.empty()) {
            ImGui::TextUnformatted("No .gltf/.glb files found.");
        } else {
            const char* selected_label =
                (selected_scene_index >= 0 && selected_scene_index < static_cast<int>(scene_labels.size()))
                    ? scene_labels[static_cast<std::size_t>(selected_scene_index)].c_str()
                    : "<none>";

            if(ImGui::BeginCombo("Scene", selected_label)) {
                for(std::size_t i = 0; i < scene_labels.size(); ++i) {
                    bool selected = (selected_scene_index == static_cast<int>(i));
                    if(ImGui::Selectable(scene_labels[i].c_str(), selected)) {
                        selected_scene_index = static_cast<int>(i);
                    }
                    if(selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            if(ImGui::Button("Load Selected")) {
                if(selected_scene_index >= 0 && selected_scene_index < static_cast<int>(scene_paths.size())) {
                    reloadScene(scene_paths[static_cast<std::size_t>(selected_scene_index)]);
                }
            }
            ImGui::SameLine();
            if(ImGui::Button("Refresh")) {
                refreshSceneList();
            }
        }

        ImGui::Separator();
        if(ImGui::BeginCombo("Backend", backendToLabel(selected_backend))) {
            for(int i = 0; i < 2; ++i) {
                const render::RenderBackend backend_option = backendFromIndex(i);
                bool selected = (selected_backend == backend_option);
                if(ImGui::Selectable(backendToLabel(backend_option), selected)) {
                    selected_backend = backend_option;
                }
                if(selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if(ImGui::Button("Apply Backend")) {
            switchBackend(selected_backend);
        }

        ImGui::Text("Active backend: %s", backendToLabel(active_backend));
        if(active_backend == render::RenderBackend::CpuPath) {
            ImGui::Separator();
            ImGui::TextUnformatted("Path Tracing");

            const char* sampler_items[] = {"Random", "Halton", "Sobol"};
            int sampler_index = static_cast<int>(path_settings.sampler);
            if(ImGui::Combo("Sampler", &sampler_index, sampler_items, IM_ARRAYSIZE(sampler_items))) {
                path_settings.sampler = static_cast<render::PathSamplerMode>(sampler_index);
            }

            const char* denoiser_items[] = {"NoOp", "Temporal", "SVGF", "OIDN"};
            if(!render::isOIDNAvailable() && path_settings.denoiser == render::PathDenoiserMode::OIDN) {
                path_settings.denoiser = render::PathDenoiserMode::Temporal;
            }
            int denoiser_index = static_cast<int>(path_settings.denoiser);
            const int denoiser_count = render::isOIDNAvailable() ? IM_ARRAYSIZE(denoiser_items) : 3;
            if(ImGui::Combo("Denoiser", &denoiser_index, denoiser_items, denoiser_count)) {
                path_settings.denoiser = static_cast<render::PathDenoiserMode>(denoiser_index);
            }
            if(!render::isOIDNAvailable()) {
                ImGui::TextUnformatted("OIDN not available in this build.");
            }

            const char* light_sampler_items[] = {"Uniform", "Power", "Contribution"};
            int light_sampler_index = static_cast<int>(path_settings.light_sampler);
            if(ImGui::Combo(
                "Light Sampler",
                &light_sampler_index,
                light_sampler_items,
                IM_ARRAYSIZE(light_sampler_items)
            )) {
                path_settings.light_sampler = static_cast<render::PathLightSamplerMode>(light_sampler_index);
            }

            ImGui::SliderInt("Max Bounces", &path_settings.max_bounces, 1, 16);
            ImGui::SliderInt("Samples / Frame", &path_settings.samples_per_frame, 1, 16);
            ImGui::Checkbox("Next Event Estimation", &path_settings.enable_nee);
            ImGui::Checkbox("Multiple Importance Sampling", &path_settings.enable_mis);
            ImGui::Checkbox("Russian Roulette", &path_settings.enable_russian_roulette);
            ImGui::Checkbox("Path Regularization", &path_settings.enable_path_regularization);
            ImGui::Checkbox("TAA Jitter", &path_settings.enable_taa);
            if(ImGui::Button("Reset Path History")) {
                path_settings.reset_requested = true;
            }

            if(renderer) {
                std::string render_status = renderer->getStatus();
                if(!render_status.empty()) ImGui::TextUnformatted(render_status.c_str());
                render::RenderProgress progress = renderer->getProgress();
                if(progress.tile_count > 0) {
                    ImGui::Text(
                        "Progress: %u samples, %d tiles, %.2f ms/frame",
                        progress.accumulated_samples,
                        progress.tile_count,
                        progress.last_frame_ms
                    );
                }
            }
        }

        ImGui::Separator();
        ImGui::Text("Current: %s", active_scene_path.c_str());
        if(!status_message.empty()) {
            ImGui::TextWrapped("%s", status_message.c_str());
        }
        ImGui::End();

        ImGui::Render();

        SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 255);
        SDL_RenderClear(sdl_renderer);
        SDL_RenderTexture(sdl_renderer, frame_texture, nullptr, nullptr);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), sdl_renderer);
        SDL_RenderPresent(sdl_renderer);

        if(take_screenshot) {
            takeScreenshotFromBuffer(buffer);
            take_screenshot = false;
        }

        if(fps_tracker.update()) {
            char title_buffer[128];
            SDL_snprintf(
                title_buffer,
                sizeof(title_buffer),
                "%s (%0.2f fps, Frame Time: %0.2f ms)",
                WINDOW_TITLE,
                fps_tracker.getFPS(),
                fps_tracker.getFrameTimeMS()
            );
            SDL_SetWindowTitle(window, title_buffer);
        }
    }
}
