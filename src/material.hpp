#pragma once

#include <glm/glm.hpp>
#include "color.hpp"
#include "image.hpp"

// A base class for all the materials
class Material {
public:
    virtual ~Material() = default;
};

// A generic PBR material that defines an base color, emissive, normal, metalness, roughness and ambient occlusion for the object.
class PBRMaterial : public Material {
public:
    std::shared_ptr<Image<ColorA>> base_color = nullptr; // The base color map that specifies the surface color.
    std::shared_ptr<Image<Color>> emissive = nullptr; // The emissive map that specifies the surface normal. 
    std::shared_ptr<Image<Color>> normal = nullptr; // The normal map that specifies the surface normal.
    std::shared_ptr<Image<float>> occlusion = nullptr; // The occlusion map that specifies the ambient occlusion.
    std::shared_ptr<Image<Color>> metal_roughness = nullptr; // A packed map that specifies the metalness (red channel) and roughness (green channel).
                                                             // It is common to pack parameters into one texture for memory and compute efficiency.
    ColorA tint = ColorA(1.0f); // A tint that is multiplied by the base color map.
    Color emissive_power = Color(0.0f); // A power that is multiplied by the emissive map.

    // A set of functions to sample the texture maps

    inline ColorA sampleBaseColor(glm::vec2 uv) const {
        if(base_color) return tint * sampleImage(base_color, uv);
        else return tint; 
    }

    inline Color sampleEmissive(glm::vec2 uv) const {
        if(emissive) return emissive_power * sampleImage(emissive, uv);
        else return emissive_power;
    }

    inline Color sampleNormal(glm::vec2 uv) const {
        if(normal) return sampleImage(normal, uv);
        else return Color(0.5f, 0.5f, 1.0f); // The default normal value is in the z-direction.
    }

    inline float sampleOcclusion(glm::vec2 uv) const {
        if(occlusion) return sampleImage(occlusion, uv);
        else return 1.0f; // Default is white (no occlusion).
    }

    inline Color sampleMetalRoughness(glm::vec2 uv) const {
        if(metal_roughness) return sampleImage(metal_roughness, uv);
        else return Color(0.0f, 1.0f, 0.0f); // Default is a rough non-metallic material.
    }
};

// A simple material that refracts light (and reflects too based on the Fresnel effect).
// Note: the color defined by base_color & tint is assumed to be the color of a thin layer on the surface of the object.
// If we wanted to make it the color of the object volume itself, we would need to support volumetric materials which is out of this lab's scope.
// We won't support rough glass since reflections & rafractions on rough surfaces are computationally expensive with Whitted-style ray tracing.
class SmoothGlassMaterial : public Material {
public:
    std::shared_ptr<Image<Color>> base_color = nullptr; // The base color map that specifies the surface color.
    std::shared_ptr<Image<Color>> normal = nullptr; // The normal map that specifies the surface normal.
    Color tint = Color(1.0f); // A tint that is multiplied by the base color map.
    float refractive_index = 1.5f; // The IOR of the material inside the object.

    // A set of functions to sample the texture maps

    inline Color sampleBaseColor(glm::vec2 uv) const {
        if(base_color) return tint * sampleImage(base_color, uv);
        else return tint;
    }

    inline Color sampleNormal(glm::vec2 uv) const {
        if(normal) return sampleImage(normal, uv);
        else return Color(0.5f, 0.5f, 1.0f); // The default normal value is in the z-direction.
    }
};

// A simple material that reflects light based on the Fresnel effect.
// Note: This is a special case of the generic PBR material where the surface is 100% metallic & 100% smooth.
// The reason it is separate from the generic PBR material is that we will support reflections for this material only, 
// since reflections on rough surfaces are computationally expensive with Whitted-style ray tracing.
class SmoothMirrorMaterial : public Material {
public:
    std::shared_ptr<Image<Color>> base_color = nullptr; // The base color map that specifies the surface color (F0).
    std::shared_ptr<Image<Color>> normal = nullptr; // The normal map that specifies the surface normal.
    Color tint = Color(1.0f); // A tint that is multiplied by the base color map.

    // A set of functions to sample the texture maps

    inline Color sampleBaseColor(glm::vec2 uv) const {
        if(base_color) return tint * sampleImage(base_color, uv);
        else return tint;
    }

    inline Color sampleNormal(glm::vec2 uv) const {
        if(normal) return sampleImage(normal, uv);
        else return Color(0.5f, 0.5f, 1.0f); // The default normal value is in the z-direction.
    }
};