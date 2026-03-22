#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "ray.hpp"

// Here, we define a camera class. We will make the following assumptions:
// - This will be a pinhole camera (the projection is perspective).
// - We will use right-handed coordinate system. If the rotation is identity, the camera will be looking along the negative Z-axis.
// - We will define the size of the screen in view-space as if it is at a distance of 1 from the camera position (eye).
// You can imagine the camera from a side view as follows:
//  positive-y   /_
//     ^        /| ^
//     |       / | |
//     |      /  | |
//  position *   | 2 * half_size.y     --> negative-z in view space (if multiplied by rotation it becomes in world space)
//            \  | |
//             \ | |
//              \|_v
//               \
//           |-1-| 
class Camera {
public:
    Camera() = default;

    // Define Getters
    inline glm::vec3 getPosition() const { return position; }
    inline glm::vec3 getForward() const { return rotation * glm::vec3(0.0f, 0.0f, -1.0f); }
    inline glm::vec3 getUp() const { return rotation * glm::vec3(0.0f, 1.0f, 0.0f); }
    inline glm::vec3 getRight() const { return rotation * glm::vec3(1.0f, 0.0f, 0.0f); }
    inline glm::vec2 getHalfSize() const { return half_size; }
    inline float getFovX() const { return 2.0f * glm::atan(half_size.x); }
    inline float getFovY() const { return 2.0f * glm::atan(half_size.y); }
    inline float getAspectRatio() const { return half_size.x / half_size.y; }

    // Define Setters
    inline void setPosition(glm::vec3 value) { position = value; }
    inline void setRotation(glm::quat value) { rotation = value; }
    inline void setHalfSize(glm::vec2 value) { half_size = value; }
    
    // Writing the quaternion by hand is not intuitive.
    // An easier way is to supply the direction that the camera is looking and the up direction, then compute the quaternion from them.
    inline void setRotation(glm::vec3 forward, glm::vec3 up) { rotation = glm::quatLookAt(forward, up); }
    // A more intuitive way to define the screen size in view space is to supply the vertical field of view angle and the aspect ratio.
    inline void setHalfSize(float fovY, float aspect_ratio) {
        half_size.y = glm::tan(fovY * 0.5f);
        half_size.x = aspect_ratio * half_size.y;
    }
    
    inline Ray generateRay(glm::vec2 uv) {
        // First, we get the ray direction in view space.
        glm::vec3 view_pos = glm::vec3(
            glm::mix(half_size * glm::vec2(-1, 1), half_size * glm::vec2(1, -1), uv), 
            -1.0f
        );
        // Then we rotate the direction to get its value in world space.
        glm::vec3 direction = glm::normalize(rotation * view_pos);
        return { position, direction };
    }

private:
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::vec2 half_size = glm::vec2(1.0f, 1.0f);
};