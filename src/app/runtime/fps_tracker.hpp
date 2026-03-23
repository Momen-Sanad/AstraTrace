#pragma once

#include <SDL3/SDL.h>

class FPSTracker {
public:
    inline FPSTracker() {
        last_fps_update = SDL_GetTicksNS();
        fps_counter = 0;
        frame_time_ms = 0;
    }

    inline bool update() {
        fps_counter++;
        Uint64 curr_time = SDL_GetTicksNS();
        if(curr_time > last_fps_update + SDL_NS_PER_SECOND) {
            frame_time_ms = (double(curr_time - last_fps_update) / SDL_NS_PER_MS) / fps_counter;
            last_fps_update = curr_time;
            fps_counter = 0;
            return true;
        }
        return false;
    }

    double getFrameTimeMS() const { return frame_time_ms; }
    double getFPS() const { return 1000.0 / frame_time_ms; }

private:
    Uint64 last_fps_update;
    Uint64 fps_counter;
    double frame_time_ms;
};
