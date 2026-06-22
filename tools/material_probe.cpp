#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

#include <glm/glm.hpp>

#include "core/color.hpp"
#include "io/gltf/gltf_loader.hpp"
#include "scene/camera/camera.hpp"
#include "scene/materials/materials.hpp"
#include "scene/world/scene.hpp"

namespace {

float halton(std::uint32_t index, std::uint32_t base) {
    float f = 1.0f;
    float r = 0.0f;
    while(index > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(index % base);
        index /= base;
    }
    return r;
}

float maxChannel(const Color& color) {
    return std::max(color.r, std::max(color.g, color.b));
}

void expandMinMax(Color value, Color& min_value, Color& max_value) {
    min_value = glm::min(min_value, value);
    max_value = glm::max(max_value, value);
}

}

int main(int argc, char** argv) {
    if(argc < 2) {
        std::cerr << "Usage: astratrace_material_probe <scene.gltf|scene.glb> [samples]\n";
        return 2;
    }

    const std::string scene_path = argv[1];
    const int samples = argc >= 3 ? std::max(1, std::atoi(argv[2])) : 256;

    Scene scene;
    Camera camera;
    GltfSceneLoadResult loaded = io::gltf::loadSceneFromGLTF(scene_path, scene, camera, 16.0f / 9.0f);
    if(!loaded.success) {
        std::cerr << "Failed to load scene: " << loaded.error << "\n";
        return 1;
    }
    if(!loaded.warning.empty()) {
        std::cout << "Warnings:\n" << loaded.warning << "\n";
    }

    scene.update();

    int textured_glass_count = 0;
    for(const auto& object : scene.getObjects()) {
        auto glass = std::dynamic_pointer_cast<SmoothGlassMaterial>(object->getMaterial());
        if(!glass || glass->surface_detail_strength <= 0.0f) continue;
        ++textured_glass_count;

        Color min_color(std::numeric_limits<float>::max());
        Color max_color(std::numeric_limits<float>::lowest());
        Color sum_color(0.0f);
        Color min_normal(std::numeric_limits<float>::max());
        Color max_normal(std::numeric_limits<float>::lowest());
        glm::vec2 min_uv(std::numeric_limits<float>::max());
        glm::vec2 max_uv(std::numeric_limits<float>::lowest());
        int valid_samples = 0;

        for(int i = 0; i < samples; ++i) {
            glm::vec3 u(
                halton(static_cast<std::uint32_t>(i + 1), 2),
                halton(static_cast<std::uint32_t>(i + 1), 3),
                halton(static_cast<std::uint32_t>(i + 1), 5)
            );
            glm::vec3 point;
            glm::vec3 normal;
            glm::vec2 uv;
            float pdf = 0.0f;
            object->samplePoint(u, point, normal, uv, pdf);
            if(pdf <= 0.0f) continue;

            Color color = glass->sampleBaseColor(uv);
            Color normal_sample = glass->sampleNormal(uv);
            expandMinMax(color, min_color, max_color);
            expandMinMax(normal_sample, min_normal, max_normal);
            sum_color += color;
            min_uv = glm::min(min_uv, uv);
            max_uv = glm::max(max_uv, uv);
            ++valid_samples;
        }

        Color avg_color = valid_samples > 0 ? sum_color / static_cast<float>(valid_samples) : Color(0.0f);
        Color color_range = max_color - min_color;
        Color normal_range = max_normal - min_normal;
        AABB bounds = object->getBounds();
        glm::vec3 center = 0.5f * (bounds.min + bounds.max);
        glm::vec3 size = bounds.max - bounds.min;

        std::cout << std::fixed << std::setprecision(4)
                  << "textured_glass object_id=" << object->getID()
                  << " samples=" << valid_samples
                  << " detail_strength=" << glass->surface_detail_strength
                  << " detail_texture=" << (glass->base_color ? "yes" : "no")
                  << " normal_texture=" << (glass->normal ? "yes" : "no")
                  << " uv_min=(" << min_uv.x << "," << min_uv.y << ")"
                  << " uv_max=(" << max_uv.x << "," << max_uv.y << ")"
                  << " color_avg=(" << avg_color.r << "," << avg_color.g << "," << avg_color.b << ")"
                  << " color_range_max=" << maxChannel(color_range)
                  << " normal_range_max=" << maxChannel(normal_range)
                  << " bounds_center=(" << center.x << "," << center.y << "," << center.z << ")"
                  << " bounds_size=(" << size.x << "," << size.y << "," << size.z << ")"
                  << "\n";
    }

    if(textured_glass_count == 0) {
        std::cout << "No textured glass preview materials found.\n";
    }

    return 0;
}
