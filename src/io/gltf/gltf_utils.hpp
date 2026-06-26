#pragma once

#include <algorithm>
#include <cctype>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <tiny_gltf.h>

#include "core/color.hpp"

namespace io::gltf {

constexpr float GLTF_EPSILON = 1e-8f;

inline float maxChannel(const Color& color) {
    return glm::max(color.r, glm::max(color.g, color.b));
}

inline void appendLine(std::string& text, const std::string& line) {
    if(line.empty()) return;
    if(!text.empty()) text += "\n";
    text += line;
}

inline void appendUniqueLine(std::string& text, const std::string& line) {
    if(line.empty()) return;
    if(text.find(line) != std::string::npos) return;
    appendLine(text, line);
}

inline std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline std::string getExtension(const std::string& path) {
    std::size_t dot = path.find_last_of('.');
    if(dot == std::string::npos) return "";
    return toLower(path.substr(dot));
}

inline glm::mat4 getNodeLocalMatrix(const tinygltf::Node& node) {
    if(node.matrix.size() == 16) {
        glm::mat4 matrix(1.0f);
        for(int col = 0; col < 4; ++col) {
            for(int row = 0; row < 4; ++row) {
                matrix[col][row] = static_cast<float>(node.matrix[col * 4 + row]);
            }
        }
        return matrix;
    }

    glm::vec3 translation(0.0f);
    if(node.translation.size() == 3) {
        translation = glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])
        );
    }

    glm::quat rotation(1.0f, 0.0f, 0.0f, 0.0f);
    if(node.rotation.size() == 4) {
        rotation = glm::quat(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2])
        );
    }

    glm::vec3 scale(1.0f);
    if(node.scale.size() == 3) {
        scale = glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])
        );
    }

    return glm::translate(glm::mat4(1.0f), translation)
        * glm::mat4_cast(rotation)
        * glm::scale(glm::mat4(1.0f), scale);
}

} // namespace io::gltf
