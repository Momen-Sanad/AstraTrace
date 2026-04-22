#pragma once

#include <string>
#include <utility>
#include <vector>
#include <SDL3/SDL.h>
#include "app/input/camera_controller.hpp"
#include "app/runtime/fps_tracker.hpp"
#include "core/color.hpp"
#include "core/image.hpp"
#include "render/renderer.hpp"
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"

class FrameLoop {
public:
    FrameLoop(
        SDL_Window* window,
        SDL_Renderer* sdl_renderer,
        SDL_Texture* frame_texture,
        Image<Color8>& buffer,
        Scene& scene,
        Camera& camera,
        CameraController& controller,
        render::IRenderer& renderer,
        std::string initial_scene_path
    )
        : window(window),
          sdl_renderer(sdl_renderer),
          frame_texture(frame_texture),
          buffer(buffer),
          scene(scene),
          camera(camera),
          controller(controller),
          renderer(renderer),
          active_scene_path(std::move(initial_scene_path)) {}

    int run();

private:
    void refreshSceneList();
    bool reloadScene(const std::string& scene_path);

    SDL_Window* window = nullptr;
    SDL_Renderer* sdl_renderer = nullptr;
    SDL_Texture* frame_texture = nullptr;
    Image<Color8>& buffer;
    Scene& scene;
    Camera& camera;
    CameraController& controller;
    render::IRenderer& renderer;

    std::string active_scene_path;
    std::vector<std::string> scene_paths;
    std::vector<std::string> scene_labels;
    int selected_scene_index = -1;
    std::string status_message;
};
