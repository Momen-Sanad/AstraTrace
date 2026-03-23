#pragma once

#include <SDL3/SDL.h>
#include "app/input/camera_controller.hpp"
#include "app/runtime/fps_tracker.hpp"
#include "core/color.hpp"
#include "core/image.hpp"
#include "platform/sdl/screenshot.hpp"
#include "render/renderer.hpp"
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"

class FrameLoop {
public:
    FrameLoop(
        SDL_Window* window,
        SDL_Surface* surface,
        Image<Color8>& buffer,
        Scene& scene,
        Camera& camera,
        CameraController& controller,
        render::IRenderer& renderer
    )
        : window(window),
          surface(surface),
          buffer(buffer),
          scene(scene),
          camera(camera),
          controller(controller),
          renderer(renderer) {}

    int run();

private:
    SDL_Window* window = nullptr;
    SDL_Surface* surface = nullptr;
    Image<Color8>& buffer;
    Scene& scene;
    Camera& camera;
    CameraController& controller;
    render::IRenderer& renderer;
};
