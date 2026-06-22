#include "core/image_io.hpp"

#include <exception>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

bool savePng(const Image<Color8>& image, const std::filesystem::path& path, std::string& error) {
    error.clear();
    if(image.getWidth() <= 0 || image.getHeight() <= 0) {
        error = "image dimensions must be positive";
        return false;
    }

    try {
        if(path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
    } catch(const std::exception& e) {
        error = e.what();
        return false;
    }

    const int width = image.getWidth();
    const int height = image.getHeight();
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    const Color8* source = image.getPixels();
    for(int i = 0; i < width * height; ++i) {
        pixels[static_cast<std::size_t>(i) * 4u + 0u] = source[i].r;
        pixels[static_cast<std::size_t>(i) * 4u + 1u] = source[i].g;
        pixels[static_cast<std::size_t>(i) * 4u + 2u] = source[i].b;
        pixels[static_cast<std::size_t>(i) * 4u + 3u] = source[i].a;
    }

    if(stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4) == 0) {
        error = "stb_image_write failed to write PNG";
        return false;
    }
    return true;
}
