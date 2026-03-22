#pragma once

#include <SDL3/SDL.h>
#include <string>

/////////////////////////////////
// Profiling & FPS Calculation //
/////////////////////////////////

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

    double getFrameTimeMS() const { return frame_time_ms; } // Return the time it took to render one frame in milliseconds
    double getFPS() const { return 1000.0 / frame_time_ms; } // Return the current FPS.
private:
    Uint64 last_fps_update; // When was the last time we calculated the FPS.
    Uint64 fps_counter; // How many frames has passed since last_fps_update.
    double frame_time_ms; // The time it took to render one frame in milliseconds
};

struct TimeIt {
    std::string label; // The label to print before the time.
    Uint64 start_time; // The time when the function was called.
    
    TimeIt() : label("") { start(); }
    TimeIt(std::string label) : label(label) { start(); }
    
    inline void start() {  start_time = SDL_GetPerformanceCounter(); }
    inline void start(std::string label) { this->label = label; start(); }

    inline double end(bool verbose = true) const { 
        double elapsed = (double)(SDL_GetPerformanceCounter() - start_time) / SDL_GetPerformanceFrequency();
        if(verbose) {
            double t = elapsed;
            const char* unit = "";
            if(t >= 1.0) { 
                unit = "s"; 
            } else {
                t *= 1000.0;
                if(t >= 1.0) { 
                    unit = "ms";
                } else {
                    t *= 1000.0;
                    if(t >= 1.0) { 
                        unit = "us";
                    } else {
                        t *= 1000.0;
                        unit = "ns";
                    }
                }
            }
            SDL_Log("%s: Time Elapsed = %0.3f%s", label.c_str(), t, unit);
        }
        return elapsed; 
    }
};

/////////////////
// Screenshots //
/////////////////

inline void takeScreenshot(SDL_Surface* surface) {
    // Create a directory for screenshots if it doesn't exist.
    SDL_CreateDirectory("screenshots");
    // Create a file name with a date time tag for the screenshot.
    SDL_Time now;
    SDL_GetCurrentTime(&now);
    SDL_DateTime date_time;
    SDL_TimeToDateTime(now, &date_time, true);
    char screenshot_file_name[64];
    SDL_snprintf(screenshot_file_name, sizeof(screenshot_file_name), 
        "screenshots/screenshot-%04d-%02d-%02d-%02d-%02d-%02d-%03d.png", 
        date_time.year, date_time.month, date_time.day, date_time.hour, date_time.minute, date_time.second, date_time.nanosecond/1000000);
    // Save the surface to a png file.
    SDL_SavePNG(surface, screenshot_file_name);
    SDL_Log("Screenshot taken: %s", screenshot_file_name);
}