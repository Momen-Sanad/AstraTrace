#include "app/application.hpp"

#include <SDL3/SDL.h>
#include <string>
#include "app/frame_loop.hpp"
#include "app/input/camera_controller.hpp"
#include "app/runtime/timer.hpp"
#include "core/color.hpp"
#include "core/image.hpp"
#include "io/gltf/gltf_loader.hpp"
#include "render/renderer.hpp"
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"

namespace {

constexpr const char* WINDOW_TITLE = "AstraTrace";
constexpr int WINDOW_WIDTH = 640;
constexpr int WINDOW_HEIGHT = 360;

render::RenderBackend parseBackend(const std::string& value) {
    if(value == "cpu-path") return render::RenderBackend::CpuPath;
    if(value == "gpu-path") return render::RenderBackend::GpuPath;
    return render::RenderBackend::CpuWhitted;
}

} // namespace

int Application::run(int argc, char** argv) {
    TimeIt timeit;

    std::string scene_path;
    render::RenderBackend backend = render::RenderBackend::CpuWhitted;
    for(int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if(arg == "--backend" && i + 1 < argc) {
            backend = parseBackend(argv[++i]);
        } else if(scene_path.empty()) {
            scene_path = arg;
        }
    }

    if(scene_path.empty()) {
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "Usage: %s <scene.gltf | scene.glb> [--backend cpu-whitted|cpu-path|gpu-path]",
            argc > 0 ? argv[0] : "astratrace_app"
        );
        return -1;
    }

    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initialize SDL: %s", SDL_GetError());
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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
        WINDOW_WIDTH,
        WINDOW_HEIGHT
    );
    if(!frame_texture) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create frame texture: %s", SDL_GetError());
        SDL_DestroyRenderer(sdl_renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    Image<Color8> buffer(WINDOW_WIDTH, WINDOW_HEIGHT);

    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), WINDOW_WIDTH / float(WINDOW_HEIGHT));

    Scene scene;
    GltfSceneLoadResult load_result = io::gltf::loadSceneFromGLTF(
        scene_path,
        scene,
        camera,
        WINDOW_WIDTH / float(WINDOW_HEIGHT)
    );

    if(!load_result.warning.empty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "GLTF warning(s):\n%s", load_result.warning.c_str());
    }
    if(!load_result.success) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "GLTF load failed: %s", load_result.error.c_str());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    CameraController controller(window, camera);
    scene.printStats();
    SDL_Log(
        "Loaded Scene: Objects=%llu Lights=%llu CameraFromGLTF=%s",
        static_cast<unsigned long long>(load_result.object_count),
        static_cast<unsigned long long>(load_result.light_count),
        load_result.camera_loaded ? "true" : "false"
    );

    FrameLoop frame_loop(
        window,
        sdl_renderer,
        frame_texture,
        buffer,
        scene,
        camera,
        controller,
        backend,
        scene_path
    );
    int exit_code = frame_loop.run();

    SDL_DestroyTexture(frame_texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exit_code;
}
