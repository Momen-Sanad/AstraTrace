#pragma once

#include <glm/glm.hpp>
#include <memory>

template<typename T>
class Image {
public:
    Image(int width, int height) : width(width), height(height) {
        pixels = new T[width * height];
    }

    ~Image() {
        delete[] pixels;
    }

    // Clear the image using the given value
    void clear(T value) {
        for (int i = 0; i < width * height; ++i) {
            pixels[i] = value;
        }
    }

    // Some getters
    int getWidth() const { return width; } // Return the width of the image
    int getHeight() const { return height; } // Return the height of the image
    T* getPixels() const { return pixels; } // Return a pointer to the pixel data

    T getPixel(int x, int y) const { return pixels[y * width + x]; } // Get the pixel at (x,y) coordinate
    T getPixel(glm::ivec2 coord) const { return pixels[coord.y * width + coord.x]; } // Get the pixel at (x,y) coordinate

    Image(const Image& other) = delete;
    Image& operator=(const Image& other) = delete;
private:
    int width = 0, height = 0;
    T* pixels = nullptr;
};

// Load an image from a file. T can either be Color (Without an alpha channel) or ColorA (With an alpha channel).
// If linearize is true, we apply a gamma curve (sRGB to linear) to the loaded pixels. This is usually used for albedo textures.
// For normal maps and other maps (metalness, roughness, etc), we shouldn't linearize the loaded pixels.
template<typename T>
std::shared_ptr<Image<T>> loadImage(const char* filename, bool linearize = true);

// Generate a square checkerboard image from the colors c1 & c2.
// Each dimension will have "size" pixels, and it will have "repeats" tiles in each dimension.
template<typename T>
std::shared_ptr<Image<T>> checkerboard(int size, int repeats, T c1, T c2) {
    auto img = std::make_shared<Image<T>>(size, size);
    T* pixels = img->getPixels();
    int tile_size = size / repeats;
    for(int y = 0, i = 0; y < size; y++) {
        for(int x = 0; x < size; x++, i++) {
            pixels[i] = ((x/tile_size) & 1) == ((y/tile_size) & 1) ? c1 : c2;
        }
    }
    return img;
}

// Sample an image using Bilinear Filtering with a Repeat Wrapping mode.
template<typename T>
T sampleImage(const std::shared_ptr<Image<T>>& img, glm::vec2 uv) {
    glm::ivec2 size = glm::ivec2(img->getWidth(), img->getHeight());
    glm::vec2 pixel = uv * glm::vec2(size) - 0.5f; // Transform UVs to pixel coordinates

    glm::vec2 fract = glm::fract(pixel); // Where are the coordinates standing between the neighboring pixels.
    // Get the coordinates of the 4 neighboring pixels, after applying the repeat wrapping.
    glm::ivec2 p00 = pixel - fract, p11 = p00 + 1;
    // Repeat Wrapping
    p00 = (p00 % size + size) % size;
    p11 = (p11 % size + size) % size;
    glm::ivec2 p01(p00.x, p11.y);
    glm::ivec2 p10(p11.x, p00.y);

    // Get the colors of the 4 neighboring pixels.
    T c00 = img->getPixel(p00);
    T c10 = img->getPixel(p10);
    T c01 = img->getPixel(p01);
    T c11 = img->getPixel(p11);

    // Apply Bilinear Filtering
    T c0 = glm::mix(c00, c10, fract.x);
    T c1 = glm::mix(c01, c11, fract.x);
    return glm::mix(c0, c1, fract.y);
}