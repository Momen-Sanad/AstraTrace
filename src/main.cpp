#include <SDL3/SDL.h>
#include <algorithm>
#include <string>
#include "camera.hpp"
#include "camera_controller.hpp"
#include "gltf_loader.hpp"
#include "scene.hpp"
#include "utils.hpp"

// The goal of this lab is to write a whitted-style ray tracer.

#define WINDOW_TITLE "Raytracer"
#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 360
#define RENDER_ROW_CHUNK 8

int main(int argc, char** argv){
    TimeIt timeit;

    if(argc < 2) {
        SDL_LogError(
            SDL_LOG_CATEGORY_ERROR,
            "Usage: %s <scene.gltf | scene.glb>",
            argc > 0 ? argv[0] : "Lab4"
        );
        return -1;
    }
    const std::string gltf_path = argv[1];
    
    // First we need to initialize SDL3.
    if(!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to initalize SDL: %s", SDL_GetError());
        return -1;
    }

    // Then we create a window where we will draw our image.
    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if(!window) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to create window: %s", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    // Then we get a surface from the window. When we blit our image onto this surface, it will appear on the window.
    SDL_Surface* surface = SDL_GetWindowSurface(window);
    if(!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to get window surface: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // Then we create image (pixel buffer) on which we will draw.
    Image<Color8> buffer(WINDOW_WIDTH, WINDOW_HEIGHT);

    // Create camera & scene, then load from glTF.
    Camera camera;
    camera.setHalfSize(glm::radians(90.0f), WINDOW_WIDTH/(float)WINDOW_HEIGHT);
    Scene scene;
    GltfSceneLoadResult load_result = loadSceneFromGLTF(
        gltf_path, scene, camera, WINDOW_WIDTH / float(WINDOW_HEIGHT)
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

    // Print some statistics
    scene.printStats();
    SDL_Log("Loaded Scene: Objects=%llu Lights=%llu CameraFromGLTF=%s",
        static_cast<unsigned long long>(load_result.object_count),
        static_cast<unsigned long long>(load_result.light_count),
        load_result.camera_loaded ? "true" : "false");

    FPSTracker fps_tracker;
    // We will store the last frame time to compute the delta time (we will use it to update the camera).
    Uint64 last_frame_time = SDL_GetTicksNS();
    // When the user presses "P", we will set this true to save the next frame to a file.
    bool take_screenshot = false;

    // Now we will start the main loop
    SDL_Event event;
    while(true) {
        bool has_event = false;
        while(SDL_PollEvent(&event)) { // Drain all pending events first.
            has_event = true;
            if(event.type == SDL_EVENT_QUIT) goto cleanup; // Quit the app.
            if(event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_P) take_screenshot = true;
            controller.handleEvent(event);
        }

        if(!has_event) { // If there are no remaining events in the event queue, we draw a new frame

            // First, we compute the delta time since the last frame and update our camera controller.
            Uint64 current_frame_time = SDL_GetTicksNS();
            float delta_time = (float)((current_frame_time - last_frame_time) / (double)SDL_NS_PER_SECOND);
            last_frame_time = current_frame_time;
            controller.update(delta_time);

            // We update scene objects and TLAS
            scene.update();
            
            Color8* pixels = buffer.getPixels();
            // To draw the frame, we will loop over each pixel and trace a ray from it.
            // We render in small row chunks so the main thread can pump events during long frames.
            bool quit_requested = false;
            for(int row_start = 0; row_start < WINDOW_HEIGHT && !quit_requested; row_start += RENDER_ROW_CHUNK) {
                int row_end = std::min(row_start + RENDER_ROW_CHUNK, WINDOW_HEIGHT);

                // We use OpenMP to parallelize the loop over the current pixel row chunk.
                #pragma omp parallel for schedule(static)
                for(int j = row_start; j < row_end; j++) {
                    for(int i = 0, idx = j * WINDOW_WIDTH; i < WINDOW_WIDTH; i++, idx++) {
                        // For each pixel, we generate a ray from the camera.
                        Ray ray = camera.generateRay(glm::vec2((i+0.5f)/WINDOW_WIDTH, (j+0.5f)/WINDOW_HEIGHT));
                        // Then, we trace the ray against the scene in a Whitted-style, and get back the radiance reaching the camera through the pixel.
                        Color radiance = scene.rayTrace(ray, 5);
                        // Finally, we tonemap and encode the computed radiance and store them in the pixel.
                        pixels[idx] = encodeColor(tonemap_aces(radiance));
                    }
                }

                // Process OS/window events in between chunks to keep the app responsive.
                while(SDL_PollEvent(&event)) {
                    if(event.type == SDL_EVENT_QUIT) {
                        quit_requested = true;
                        break;
                    }
                    if(event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_P) take_screenshot = true;
                    controller.handleEvent(event);
                }
            }
            if(quit_requested) break;

            // After the frame is ready, we display it on the window surface.
            SDL_LockSurface(surface);
            SDL_ConvertPixels(WINDOW_WIDTH, WINDOW_HEIGHT, SDL_PIXELFORMAT_ABGR8888, buffer.getPixels(), sizeof(Color8) * WINDOW_WIDTH, surface->format, surface->pixels, surface->pitch);
            SDL_UnlockSurface(surface);
            SDL_UpdateWindowSurface(window);

            // If the user requeqest that we save a screenshot.
            if(take_screenshot) {
                takeScreenshot(surface);
                take_screenshot = false;
            }

            // Finally, we will update the FPS counter, and if a second has passed since the last FPS update, we recompute and display the new FPS.
            if(fps_tracker.update()) {
                char title_buffer[64];
                SDL_snprintf(title_buffer, sizeof(title_buffer), WINDOW_TITLE " (%0.2f fps, Frame Time: %0.2f ms)", fps_tracker.getFPS(), fps_tracker.getFrameTimeMS());
                SDL_SetWindowTitle(window, title_buffer);
            }
        }
    }

cleanup:
    // Some clean up
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
