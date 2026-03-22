#include "camera_controller.hpp"

CameraController::CameraController(SDL_Window* window, Camera& camera) : 
    window(window), camera(camera) {
    glm::vec3 direction = camera.getForward();
    yaw = SDL_atan2f(-direction.z, direction.x);
    pitch = SDL_asinf(direction.y);
    fovY = camera.getFovY();
}

void CameraController::handleEvent(const SDL_Event& event) {
    if(event.type == SDL_EVENT_KEY_DOWN) {
        keyStates[event.key.scancode] = true;
    } else if(event.type == SDL_EVENT_KEY_UP) {
        keyStates[event.key.scancode] = false;
    } else if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if(event.button.button == SDL_BUTTON_LEFT) {
            SDL_SetWindowRelativeMouseMode(window, true);
            enabled = true;
        }
    } else if(event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        if(event.button.button == SDL_BUTTON_LEFT) {
            SDL_SetWindowRelativeMouseMode(window, false);
            enabled = false;
            mouse_motion = glm::vec2(0.0f);
        }
    } else if(event.type == SDL_EVENT_MOUSE_MOTION) {
        if(enabled) {
            mouse_motion += glm::vec2(event.motion.xrel, event.motion.yrel);
        }
    } else if(event.type == SDL_EVENT_MOUSE_WHEEL) {
        wheel_motion += event.wheel.y;
    }
}

void CameraController::update(float dt) {
    if(enabled && mouse_motion != glm::vec2(0.0f)) {
        
        yaw -= mouse_motion.x * sensitivity;
        pitch -= mouse_motion.y * sensitivity;
        pitch = glm::clamp(pitch, -glm::radians(89.0f), glm::radians(89.0f));
        
        camera.setRotation(glm::vec3(
            glm::cos(yaw) * glm::cos(pitch),
            glm::sin(pitch),
            -glm::sin(yaw) * glm::cos(pitch)
        ), glm::vec3(0.0f, 1.0f, 0.0f));

        mouse_motion = glm::vec2(0.0f);
    }

    if(wheel_motion != 0.0f) {
        fovY += wheel_motion * glm::radians(1.0f);
        fovY = glm::clamp(fovY, glm::radians(1.0f), glm::radians(179.0f));
        camera.setHalfSize(fovY, camera.getAspectRatio());

        wheel_motion = 0.0f;
    }

    glm::vec3 motion = glm::vec3(0.0f);
    if(keyStates[SDL_SCANCODE_W] || keyStates[SDL_SCANCODE_UP])    motion.z++;
    if(keyStates[SDL_SCANCODE_S] || keyStates[SDL_SCANCODE_DOWN])  motion.z--;
    if(keyStates[SDL_SCANCODE_A] || keyStates[SDL_SCANCODE_LEFT])  motion.x--;
    if(keyStates[SDL_SCANCODE_D] || keyStates[SDL_SCANCODE_RIGHT]) motion.x++;
    if(keyStates[SDL_SCANCODE_E]) motion.y--;
    if(keyStates[SDL_SCANCODE_Q]) motion.y++;
    if(motion != glm::vec3(0.0f)) {
        float distance = dt * speed * (keyStates[SDL_SCANCODE_LSHIFT] ? sprint_multiplier : 1.0f);
        motion = glm::normalize(motion) * distance;
        glm::vec3 position = camera.getPosition();
        position += motion.x * camera.getRight();
        position += motion.z * camera.getForward();
        position += motion.y * glm::vec3(0.0f, 1.0f, 0.0f); // Easier
        camera.setPosition(position);
    }
};