#include "app/frame_loop.hpp"

namespace {
constexpr const char* WINDOW_TITLE = "AstraTrace";
}

int FrameLoop::run() {
    FPSTracker fps_tracker;
    Uint64 last_frame_time = SDL_GetTicksNS();
    bool take_screenshot = false;

    SDL_Event event;
    while(true) {
        bool has_event = false;
        while(SDL_PollEvent(&event)) {
            has_event = true;
            if(event.type == SDL_EVENT_QUIT) return 0;
            if(event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_P) take_screenshot = true;
            controller.handleEvent(event);
        }

        if(!has_event) {
            Uint64 current_frame_time = SDL_GetTicksNS();
            float delta_time = static_cast<float>((current_frame_time - last_frame_time) / static_cast<double>(SDL_NS_PER_SECOND));
            last_frame_time = current_frame_time;
            controller.update(delta_time);
            scene.update();

            renderer.render(scene, camera, buffer);

            SDL_LockSurface(surface);
            SDL_ConvertPixels(
                buffer.getWidth(),
                buffer.getHeight(),
                SDL_PIXELFORMAT_ABGR8888,
                buffer.getPixels(),
                sizeof(Color8) * buffer.getWidth(),
                surface->format,
                surface->pixels,
                surface->pitch
            );
            SDL_UnlockSurface(surface);
            SDL_UpdateWindowSurface(window);

            if(take_screenshot) {
                takeScreenshot(surface);
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
}
