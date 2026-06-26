#include "io/gltf/gltf_textures.hpp"

#include <algorithm>

#include <glm/glm.hpp>

namespace io::gltf {

const tinygltf::Image* getTextureImage(const tinygltf::Model& model, int texture_index) {
    if(texture_index < 0 || texture_index >= static_cast<int>(model.textures.size())) return nullptr;
    int image_index = model.textures[texture_index].source;
    if(image_index < 0 || image_index >= static_cast<int>(model.images.size())) return nullptr;
    const tinygltf::Image& image = model.images[image_index];
    if(image.width <= 0 || image.height <= 0 || image.image.empty()) return nullptr;
    return &image;
}

float readImageChannel(const tinygltf::Image& image, int x, int y, int channel) {
    int comp = std::max(1, image.component);
    int clamped_x = std::clamp(x, 0, image.width - 1);
    int clamped_y = std::clamp(y, 0, image.height - 1);
    int clamped_channel = std::clamp(channel, 0, comp - 1);

    std::size_t idx = (static_cast<std::size_t>(clamped_y) * image.width + clamped_x) * comp + clamped_channel;
    if(idx >= image.image.size()) return 0.0f;
    return image.image[idx] / 255.0f;
}

std::shared_ptr<Image<ColorA>> buildBaseColorImage(
    const tinygltf::Image& image,
    bool linearize,
    const std::string& alpha_mode,
    float alpha_cutoff
) {
    auto out = std::make_shared<Image<ColorA>>(image.width, image.height);
    ColorA* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float r = readImageChannel(image, x, y, 0);
            float g = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float b = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);
            float a = image.component > 3 ? readImageChannel(image, x, y, 3) : 1.0f;

            if(linearize) {
                r = convertGammaToLinear(r);
                g = convertGammaToLinear(g);
                b = convertGammaToLinear(b);
            }

            if(alpha_mode == "OPAQUE") {
                a = 1.0f;
            } else if(alpha_mode == "MASK") {
                a = (a >= alpha_cutoff) ? 1.0f : 0.0f;
            }

            pixels[y * image.width + x] = ColorA(r, g, b, a);
        }
    }
    return out;
}

bool materialNameLooksLikeGuide(const std::string& name) {
    std::string lower = toLower(name);
    return lower.find("guide") != std::string::npos ||
           lower.find("label") != std::string::npos ||
           lower.find("annotation") != std::string::npos;
}

bool looksLikeDarkTransparentAnnotationTexture(const tinygltf::Image& image) {
    if(image.component < 4 || image.width <= 0 || image.height <= 0) return false;

    const int step = glm::max(1, glm::min(image.width, image.height) / 256);
    int samples = 0;
    int visible = 0;
    int dark_visible = 0;

    for(int y = 0; y < image.height; y += step) {
        for(int x = 0; x < image.width; x += step) {
            samples++;
            float alpha = readImageChannel(image, x, y, 3);
            if(alpha <= 0.05f) continue;

            visible++;
            float r = readImageChannel(image, x, y, 0);
            float g = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float b = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);
            float luminance = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            if(luminance < 0.18f) dark_visible++;
        }
    }

    if(samples == 0 || visible == 0) return false;
    const float visible_ratio = static_cast<float>(visible) / static_cast<float>(samples);
    const float dark_ratio = static_cast<float>(dark_visible) / static_cast<float>(visible);
    return visible_ratio < 0.35f && dark_ratio > 0.85f;
}

std::shared_ptr<Image<ColorA>> buildReadableGuideImage(
    const tinygltf::Image& image,
    const std::string& alpha_mode,
    float alpha_cutoff
) {
    auto out = buildBaseColorImage(image, false, alpha_mode, alpha_cutoff);
    ColorA* pixels = out->getPixels();
    const int count = out->getWidth() * out->getHeight();
    for(int i = 0; i < count; ++i) {
        pixels[i].r = 0.84f;
        pixels[i].g = 0.88f;
        pixels[i].b = 0.90f;
    }
    return out;
}

std::shared_ptr<Image<Color>> buildGuideEmissionImage(const tinygltf::Image& image) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float alpha = image.component > 3 ? readImageChannel(image, x, y, 3) : 1.0f;
            pixels[y * image.width + x] = alpha * Color(0.84f, 0.88f, 0.90f);
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> buildColorImage(const tinygltf::Image& image, bool linearize) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float r = readImageChannel(image, x, y, 0);
            float g = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float b = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);
            if(linearize) {
                r = convertGammaToLinear(r);
                g = convertGammaToLinear(g);
                b = convertGammaToLinear(b);
            }
            pixels[y * image.width + x] = Color(r, g, b);
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> buildNormalImage(const tinygltf::Image& image, float scale) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            glm::vec3 local(
                2.0f * readImageChannel(image, x, y, 0) - 1.0f,
                2.0f * readImageChannel(image, x, y, image.component > 1 ? 1 : 0) - 1.0f,
                2.0f * readImageChannel(image, x, y, image.component > 2 ? 2 : 0) - 1.0f
            );
            local.x *= scale;
            local.y *= scale;
            if(glm::dot(local, local) > GLTF_EPSILON) local = glm::normalize(local);
            else local = glm::vec3(0.0f, 0.0f, 1.0f);

            pixels[y * image.width + x] = Color(
                0.5f * (local.x + 1.0f),
                0.5f * (local.y + 1.0f),
                0.5f * (local.z + 1.0f)
            );
        }
    }
    return out;
}

std::shared_ptr<Image<float>> buildOcclusionImage(const tinygltf::Image& image, float strength) {
    auto out = std::make_shared<Image<float>>(image.width, image.height);
    float* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float occlusion = readImageChannel(image, x, y, 0);
            pixels[y * image.width + x] = glm::mix(1.0f, occlusion, strength);
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> buildMetalRoughnessImage(
    const tinygltf::Image& image,
    float metallic_factor,
    float roughness_factor
) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float g = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float b = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);
            pixels[y * image.width + x] = Color(
                glm::clamp(b * metallic_factor, 0.0f, 1.0f),
                glm::clamp(g * roughness_factor, 0.0f, 1.0f),
                0.0f
            );
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> buildTransmissionDetailImage(const tinygltf::Image& image) {
    auto out = std::make_shared<Image<Color>>(image.width, image.height);
    Color* pixels = out->getPixels();
    for(int y = 0; y < image.height; ++y) {
        for(int x = 0; x < image.width; ++x) {
            float occlusion = readImageChannel(image, x, y, 0);
            float roughness = readImageChannel(image, x, y, image.component > 1 ? 1 : 0);
            float metallic = readImageChannel(image, x, y, image.component > 2 ? 2 : 0);

            float detail = occlusion * (0.35f + 0.65f * roughness) - 0.10f * metallic;
            float grain = 0.50f + 1.75f * (detail - 0.55f);
            grain = glm::clamp(grain, 0.08f, 0.92f);
            pixels[y * image.width + x] = Color(grain);
        }
    }
    return out;
}

std::shared_ptr<Image<Color>> makeSolidColorImage(const Color& color) {
    auto out = std::make_shared<Image<Color>>(1, 1);
    out->getPixels()[0] = color;
    return out;
}

} // namespace io::gltf
