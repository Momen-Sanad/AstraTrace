#pragma once

#include <glm/glm.hpp>

// Color8: This will be sent to the surface. It is in sRGB space with an alpha channel. Each channel is 8-bits.
typedef glm::tvec4<uint8_t> Color8;

// Color: This will be used to simulate lighting. It is in linear RGB space with no alpha channel. Each channel is a float.
typedef glm::vec3 Color;
// ColorA: This will be used for materials that have an alpha channel. It is in linear RGB space with an alpha channel. Each channel is a float.
typedef glm::vec4 ColorA;

// Converts one channel from linear sRGB to gamma (non-linear) sRGB
inline float convertLinearToGamma(float x) {
    return x <= 0.0031308f ? 
        12.92f * x : 
        1.055f * glm::pow(x,1.0f / 2.4f ) - 0.055f;
};

// Converts one channel from gamma (non-linear) sRGB to linear sRGB
inline float convertGammaToLinear(float x) {
    return x <= 0.04045f ? 
        x / 12.92f : 
        glm::pow((x + 0.055f) / 1.055f,2.4f );
};

// This function encodes a linear sRGB color to an non-linear sRGB color, which is what the surface can display.
// The input is expected to be linear display radiance (the range is [0,1] for each channel).
// But we will still apply clamp to ensure that the input is within the valid range.
inline Color8 encodeColor(Color color) {
    color = glm::clamp(color, Color(0.0f), Color(1.0f));
    color.r = convertLinearToGamma(color.r) * 255.0f;
    color.g = convertLinearToGamma(color.g) * 255.0f;
    color.b = convertLinearToGamma(color.b) * 255.0f;
    return Color8(color.r, color.g, color.b, 255);
}

inline Color8 encodeColor(ColorA color) {
    color = glm::clamp(color, ColorA(0.0f), ColorA(1.0f));
    color.r = convertLinearToGamma(color.r) * 255.0f;
    color.g = convertLinearToGamma(color.g) * 255.0f;
    color.b = convertLinearToGamma(color.b) * 255.0f;
    color.a = convertLinearToGamma(color.a) * 255.0f;
    return Color8(color);
}

// When we are simulating lighting, we will be working with linear scene radiance, not linear display radiance (which is what encodeColor expects).
// To convert from linear scene radiance to linear display radiance, we will need to use a tonemapping function.
// One simple tonemapping function is Reinhard's method, which is applied in the following function.
inline Color tonemap_reinhard(Color color) {
    return color / (1.0f + color);
}

// Another tonemapping function is the Academy Color Encoding System (ACES) cinematic tonemapping curve.
// The fit used in this code was created by Stephen Hill.
inline Color tonemap_aces(Color color) {
    // This matrix combines the colorspace transformations: sRGB => XYZ => D65_2_D60 => AP1 => RRT_SAT
    const glm::mat3 SRGB_TO_ACESRRT = glm::mat3(
        0.59719f, 0.07600f, 0.02840f, 
        0.35458f, 0.90834f, 0.13383f, 
        0.04823f, 0.01566f, 0.83777f
    );

    // This matrix combines the colorspace transformations: ODT_SAT => XYZ => D60_2_D65 => sRGB
    glm::mat3 ACESODT_TO_SRGB = glm::mat3(
        1.60475f, -0.10208f, -0.00327f, 
        -0.53108f, 1.10813f, -0.07276f, 
        -0.07367f, -0.00605f, 1.07602f
    );

    // Transform from sRGB to ACES AP1, then apply Reference Rendering Transform (RRT) with saturation control.
    color = SRGB_TO_ACESRRT * color;
    // Apply the fitted tonemapping curve.
    color = (color * (color + 0.0245786f) - 0.000090537f)/(color * (0.983729f * color + 0.4329510f) + 0.238081f);
    // Apply Output Device Transform (ODT) with saturation control, then transform back to sRGB.
    return ACESODT_TO_SRGB * color;
}