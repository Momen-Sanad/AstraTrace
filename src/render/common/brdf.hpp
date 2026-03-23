#pragma once

#include "core/color.hpp"
#include <glm/glm.hpp>

// The following are a set of functions that we will use to compute the shading of PBR materials.

// Compute the fresnel term using Schlick's approximation
inline Color computeFresnelSchlick(const glm::vec3& n, const glm::vec3& l, const Color& F0) {
    return glm::mix(F0, Color(1.0f), glm::pow(1.0f - glm::max(0.0f, glm::dot(n, l)), 5.0f));
}

// Compute the Smith Geometry-1 term using Karis's approximation
inline float computeSmithG1Karis(float n_dot_s, float alpha) {
    n_dot_s = glm::max(0.0f, n_dot_s);
    float k = alpha * 0.5f; // Here, we use k = alpha / 2 where alpha = roughness^2 like in the book.
                            // But if you look in Karis's notes: https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
                            // you will see that he recommending using alpha = ((roughness + 1) / 2)^2 instead for the geometry term for analytic light sources (such as directional, point & spot lights).
                            // However, the difference is not huge, so we will stick the book's version for simplicity.
    return n_dot_s / (n_dot_s * (1.0f - k) + k);
}

// Compute the Smith Geometry-2 term using Karis's approximation
inline float computeSmithG2Karis(float n_dot_v, float n_dot_l, float alpha) {
    return computeSmithG1Karis(n_dot_v, alpha) *
           computeSmithG1Karis(n_dot_l, alpha);
}

// Compute the GGX Normal Distribution Function (NDF)
inline float computeGGXNDF(float n_dot_h, float alpha) {
    float a2 = alpha * alpha;
    float k = 1.0f + n_dot_h * n_dot_h * (a2 - 1.0f);
    return a2 / (k * k);
}

// Compute the Cook-Torrance GGX specular term (F is the Fresnel reflectance)
// The output is the Cook-Torrance GGX BRDF multiplied by pi and dot(n, l).
inline Color computeCookTorranceGGXSpecular(const Color& F, float n_dot_l, float n_dot_v, float n_dot_h, float alpha) {
    float G = computeSmithG2Karis(n_dot_v, n_dot_l, alpha);
    float D = computeGGXNDF(n_dot_h, alpha);
    return (F * G * D) / (4.0f * glm::max(0.0001f, glm::abs(n_dot_v)));
}

// Compute the Lambert diffuse term
// The output is the Lambert BRDF multiplied by pi and dot(n, l).
inline Color computeLambertDiffuse(const Color& diffuse_color, float n_dot_l) {
    return diffuse_color * n_dot_l;
}

// Compute the sum of Lambert diffuse and GGX specular terms
// The output is the combined Lambert & Cook-Torrance GGX BRDF multiplied by pi and dot(n, l).
inline Color computeLambertDiffuseAndGGXSpecular(const Color& albedo, const Color& F0, const glm::vec3& n, const glm::vec3& l, const glm::vec3& v, float roughness) {
    glm::vec3 half = glm::normalize(l + v);
    float n_dot_l = glm::dot(n, l);
    if(n_dot_l <= 0.0f) return Color(0.0f);
    float n_dot_h = glm::dot(n, half);
    if(n_dot_h <= 0.0f) return Color(0.0f);
    float n_dot_v = glm::dot(n, v);

    // Convert roughness to alpha and clamp the value to prevent the numerical issues that happen at zero roughness.
    float alpha = glm::max(roughness * roughness, 0.00001f);

    Color F = computeFresnelSchlick(half, l, F0);
    
    Color diffuse = computeLambertDiffuse(albedo * (1.0f - F), n_dot_l);
    Color specular = computeCookTorranceGGXSpecular(F, n_dot_l, n_dot_v, n_dot_h, alpha);
    return diffuse + specular;
}
