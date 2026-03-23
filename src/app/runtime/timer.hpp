#pragma once

#include <SDL3/SDL.h>
#include <string>

struct TimeIt {
    std::string label;
    Uint64 start_time;

    TimeIt() : label("") { start(); }
    explicit TimeIt(std::string label) : label(std::move(label)) { start(); }

    inline void start() { start_time = SDL_GetPerformanceCounter(); }
    inline void start(std::string next_label) {
        label = std::move(next_label);
        start();
    }

    inline double end(bool verbose = true) const {
        double elapsed = static_cast<double>(SDL_GetPerformanceCounter() - start_time) / SDL_GetPerformanceFrequency();
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
