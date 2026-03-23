#pragma once

#include <SDL3/SDL.h>
#include "scene/camera/camera.hpp"

// This is the camera controller class. It handles the input from the keyboard and mouse to control the camera.
// You can skip reading this class, as it is not directly related to the ray tracing algorithm.
class CameraController {
public:
    CameraController(SDL_Window* window, Camera& camera);

    void handleEvent(const SDL_Event& event); // Call for every event to capture mouse/keyboard input.
    void update(float dt); // Call every frame to update the camera position and orientation.

    // Define Setters
    inline void setSpeed(float speed) { this->speed = speed; } // The default movement speed of the camera.
    inline void setSprintMultiplier(float multiplier) { this->sprint_multiplier = multiplier; } // How much faster we go when Shift is pressed.
    inline void setSensitivity(float sensitivity) { this->sensitivity = sensitivity; } // The sensitivity of the camera rotation to mouse movement.
private:
    SDL_Window* window;
    Camera& camera;
    float pitch, yaw, fovY;
    float speed = 1.0f, sprint_multiplier = 5.0f, sensitivity = 0.01f;
    glm::vec2 mouse_motion = glm::vec2(0.0f);
    float wheel_motion = 0.0f;
    bool keyStates[SDL_SCANCODE_COUNT] = {0};
    bool enabled = false;
};
