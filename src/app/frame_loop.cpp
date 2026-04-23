#include "app/frame_loop.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

#include <glm/trigonometric.hpp>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include "io/gltf/gltf_loader.hpp"
#include "platform/sdl/screenshot.hpp"

namespace {
constexpr const char* WINDOW_TITLE = "AstraTrace";
constexpr const char* BACKEND_LABEL_CPU_WHITTED = "CPU Whitted";
constexpr const char* BACKEND_LABEL_CPU_PATH = "CPU Path";
constexpr const char* BACKEND_LABEL_GPU_PATH = "GPU Path";

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
    case render::RenderBackend::GpuPath:
        return BACKEND_LABEL_GPU_PATH;
    case render::RenderBackend::CpuWhitted:
    default:
        return BACKEND_LABEL_CPU_WHITTED;
    }
}

render::RenderBackend backendFromIndex(int index) {
    switch(index) {
    case 1:
        return render::RenderBackend::CpuPath;
    case 2:
        return render::RenderBackend::GpuPath;
    case 0:
    default:
        return render::RenderBackend::CpuWhitted;
    }
}

} // namespace

void FrameLoop::refreshSceneList() {
    std::filesystem::path active_path(active_scene_path);
    const std::string active_key = normalizePathKey(active_path);
    std::filesystem::path scenes_root = findScenesRoot(active_path);

    scene_paths.clear();
    scene_labels.clear();
    selected_scene_index = -1;

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

    if(!active_scene_path.empty()) {
        auto active_it = std::find_if(scene_paths.begin(), scene_paths.end(), [&](const std::string& path) {
            return normalizePathKey(path) == active_key;
        });
        if(active_it == scene_paths.end()) {
            scene_paths.push_back(normalizePathString(active_scene_path));
        }
    }

    if(!scene_paths.empty()) {
        std::sort(scene_paths.begin(), scene_paths.end());
        for(const std::string& scene_path : scene_paths) {
            std::filesystem::path path(scene_path);
            std::string label = path.filename().string();
            if(!scenes_root.empty()) {
                std::error_code ec;
                std::filesystem::path relative = std::filesystem::relative(path, scenes_root, ec);
                if(!ec && !relative.empty()) label = relative.string();
            }
            scene_labels.push_back(label);
        }

        auto selected_it = std::find_if(scene_paths.begin(), scene_paths.end(), [&](const std::string& path) {
            return normalizePathKey(path) == active_key;
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
    camera = loaded_camera;
    controller.resetFromCamera();

    active_scene_path = normalizePathString(scene_path);
    status_message = "Loaded: " + active_scene_path;

    scene.printStats();
    SDL_Log(
        "Loaded Scene: Objects=%llu Lights=%llu CameraFromGLTF=%s",
        static_cast<unsigned long long>(load_result.object_count),
        static_cast<unsigned long long>(load_result.light_count),
        load_result.camera_loaded ? "true" : "false"
    );

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
    active_backend = backend;
    selected_backend = backend;
    status_message = std::string("Backend switched to: ") + backendToLabel(active_backend);
    SDL_Log("Renderer backend switched to: %s", backendToLabel(active_backend));
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

    active_scene_path = normalizePathString(active_scene_path);
    refreshSceneList();
    status_message = "Loaded: " + active_scene_path;

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

        controller.update(delta_time);
        scene.update();
        if(renderer) {
            renderer->render(scene, camera, buffer);
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
            for(int i = 0; i < 3; ++i) {
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
