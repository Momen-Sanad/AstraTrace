#include "core/image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "core/color.hpp"

template<>
std::shared_ptr<Image<Color>> loadImage<Color>(const char* filename, bool linearize) {
    // We expect the first pixel in the loaded data to be the bottom-left pixel which is not the common standard in stored images.
    // So we ask stb_image to flip the image vertically on load.
    stbi_set_flip_vertically_on_load(true);
    
    // Load the image from the file.
    int x, y, comp;
    stbi_uc* data = stbi_load(filename, &x, &y, &comp, 3);
    if(!data) return nullptr;
    
    // Create an Image object and decode the pixel data into it. 
    auto img = std::make_shared<Image<Color>>(x, y);
    Color* pixels = img->getPixels();
    if(linearize) { // Used for color images such as albedo
        for(int i = 0, s = x * y; i < s; i++) {
            pixels[i] = Color(
                convertGammaToLinear(data[i * 3 + 0] / 255.0f), 
                convertGammaToLinear(data[i * 3 + 1] / 255.0f), 
                convertGammaToLinear(data[i * 3 + 2] / 255.0f)
            );
        }
    } else { // Used for normals and other textures
        for(int i = 0, s = x * y; i < s; i++) {
            pixels[i] = Color(
                data[i * 3 + 0] / 255.0f, 
                data[i * 3 + 1] / 255.0f, 
                data[i * 3 + 2] / 255.0f
            );
        }
    }
    
    STBI_FREE(data);
    
    return img;
}

template<>
std::shared_ptr<Image<ColorA>> loadImage<ColorA>(const char* filename, bool linearize) {
    // We expect the first pixel in the loaded data to be the bottom-left pixel which is not the common standard in stored images.
    // So we ask stb_image to flip the image vertically on load.
    stbi_set_flip_vertically_on_load(true);
    
    // Load the image from the file.
    int x, y, comp;
    stbi_uc* data = stbi_load(filename, &x, &y, &comp, 4);
    if(!data) return nullptr;
    
    // Create an Image object and decode the pixel data into it. 
    auto img = std::make_shared<Image<ColorA>>(x, y);
    ColorA* pixels = img->getPixels();
    if(linearize) { // Used for color images such as albedo
        for(int i = 0, s = x * y; i < s; i++) {
            pixels[i] = ColorA(
                convertGammaToLinear(data[i * 4 + 0] / 255.0f), 
                convertGammaToLinear(data[i * 4 + 1] / 255.0f), 
                convertGammaToLinear(data[i * 4 + 2] / 255.0f), 
                convertGammaToLinear(data[i * 4 + 3] / 255.0f)
            );
        }
    } else { // Used for normals and other textures
        for(int i = 0, s = x * y; i < s; i++) {
            pixels[i] = ColorA(
                data[i * 4 + 0] / 255.0f, 
                data[i * 4 + 1] / 255.0f, 
                data[i * 4 + 2] / 255.0f, 
                data[i * 4 + 3] / 255.0f
            );
        }
    }
    
    STBI_FREE(data);
    
    return img;
}

template<>
std::shared_ptr<Image<float>> loadImage<float>(const char* filename, bool linearize) {
    // We expect the first pixel in the loaded data to be the bottom-left pixel which is not the common standard in stored images.
    // So we ask stb_image to flip the image vertically on load.
    stbi_set_flip_vertically_on_load(true);
    
    // Load the image from the file.
    int x, y, comp;
    stbi_uc* data = stbi_load(filename, &x, &y, &comp, 1);
    if(!data) return nullptr;
    
    // Create an Image object and decode the pixel data into it. 
    auto img = std::make_shared<Image<float>>(x, y);
    float* pixels = img->getPixels();
    if(linearize) { // Used for color images such as albedo
        for(int i = 0, s = x * y; i < s; i++) {
            pixels[i] = convertGammaToLinear(data[i] / 255.0f);
        }
    } else { // Used for normals and other textures
        for(int i = 0, s = x * y; i < s; i++) {
            pixels[i] = data[i] / 255.0f;
        }
    }
    
    STBI_FREE(data);
    
    return img;
}
