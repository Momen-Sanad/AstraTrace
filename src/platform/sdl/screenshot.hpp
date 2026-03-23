#pragma once

#include <SDL3/SDL.h>

inline void takeScreenshot(SDL_Surface* surface) {
    SDL_CreateDirectory("screenshots");

    SDL_Time now;
    SDL_GetCurrentTime(&now);
    SDL_DateTime date_time;
    SDL_TimeToDateTime(now, &date_time, true);

    char screenshot_file_name[64];
    SDL_snprintf(
        screenshot_file_name,
        sizeof(screenshot_file_name),
        "screenshots/screenshot-%04d-%02d-%02d-%02d-%02d-%02d-%03d.png",
        date_time.year,
        date_time.month,
        date_time.day,
        date_time.hour,
        date_time.minute,
        date_time.second,
        date_time.nanosecond / 1000000
    );

    SDL_SavePNG(surface, screenshot_file_name);
    SDL_Log("Screenshot taken: %s", screenshot_file_name);
}
