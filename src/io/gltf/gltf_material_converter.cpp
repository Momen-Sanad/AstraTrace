#include "io/gltf/gltf_material_converter.hpp"

#include <cmath>

#include <glm/glm.hpp>

#include "io/gltf/gltf_textures.hpp"
#include "io/gltf/gltf_utils.hpp"

namespace io::gltf {
namespace {

float getEmissiveStrength(const tinygltf::Material& material) {
    auto ext_it = material.extensions.find("KHR_materials_emissive_strength");
    if(ext_it == material.extensions.end()) return 1.0f;
    const tinygltf::Value& ext = ext_it->second;
    if(!ext.IsObject()) return 1.0f;
    if(!ext.Has("emissiveStrength")) return 1.0f;
    const tinygltf::Value& value = ext.Get("emissiveStrength");
    if(!value.IsNumber()) return 1.0f;
    return static_cast<float>(value.GetNumberAsDouble());
}

const tinygltf::Value* findExtension(const tinygltf::Material& material, const char* name) {
    auto ext_it = material.extensions.find(name);
    if(ext_it == material.extensions.end()) return nullptr;
    if(!ext_it->second.IsObject()) return nullptr;
    return &ext_it->second;
}

Color getExtensionColor3(
    const tinygltf::Material& material,
    const char* extension_name,
    const char* property_name,
    Color fallback
) {
    const tinygltf::Value* ext = findExtension(material, extension_name);
    if(!ext || !ext->Has(property_name)) return fallback;
    const tinygltf::Value& value = ext->Get(property_name);
    if(!value.IsArray() || value.ArrayLen() < 3) return fallback;

    Color out = fallback;
    for(int i = 0; i < 3; ++i) {
        const tinygltf::Value& channel = value.Get(static_cast<size_t>(i));
        if(!channel.IsNumber()) return fallback;
        out[i] = static_cast<float>(channel.GetNumberAsDouble());
    }
    return out;
}

Color computeIridescencePreviewTint(const tinygltf::Material& material) {
    constexpr float two_pi = 6.28318530718f;
    const float ior = getMaterialExtensionNumber(material, "KHR_materials_iridescence", "iridescenceIor", 1.3f);
    const float thickness = getMaterialExtensionNumber(
        material,
        "KHR_materials_iridescence",
        "iridescenceThicknessMaximum",
        300.0f
    );

    const float phase = glm::fract(0.11f + 0.17f * ior + 0.0017f * thickness);
    const Color wave(
        0.5f + 0.5f * std::cos(two_pi * (phase + 0.00f)),
        0.5f + 0.5f * std::cos(two_pi * (phase + 0.33f)),
        0.5f + 0.5f * std::cos(two_pi * (phase + 0.67f))
    );

    return Color(
        0.18f + 0.68f * wave.r,
        0.18f + 0.68f * wave.g,
        0.18f + 0.68f * wave.b
    );
}

} // namespace

float getMaterialExtensionNumber(
    const tinygltf::Material& material,
    const char* extension_name,
    const char* property_name,
    float fallback
) {
    const tinygltf::Value* ext = findExtension(material, extension_name);
    if(!ext || !ext->Has(property_name)) return fallback;
    const tinygltf::Value& value = ext->Get(property_name);
    if(!value.IsNumber()) return fallback;
    return static_cast<float>(value.GetNumberAsDouble());
}

bool hasAuthoredEmission(const tinygltf::Material& material) {
    if(material.emissiveTexture.index >= 0) return true;
    for(double channel : material.emissiveFactor) {
        if(channel > GLTF_EPSILON) return true;
    }
    return false;
}

std::shared_ptr<Material> createMaterialFromGLTF(
    const tinygltf::Model& model,
    const tinygltf::Material& material,
    std::string& warnings
) {
    const tinygltf::PbrMetallicRoughness& mr = material.pbrMetallicRoughness;
    const float metallic_factor = static_cast<float>(mr.metallicFactor);
    const float roughness_factor = static_cast<float>(mr.roughnessFactor);
    const tinygltf::Value* iridescence_ext = findExtension(material, "KHR_materials_iridescence");
    const tinygltf::Value* transmission_ext = findExtension(material, "KHR_materials_transmission");
    const float transmission_factor = transmission_ext
        ? getMaterialExtensionNumber(material, "KHR_materials_transmission", "transmissionFactor", 0.0f)
        : 0.0f;
    const bool has_transmission_texture =
        transmission_ext &&
        transmission_ext->Has("transmissionTexture") &&
        transmission_ext->Get("transmissionTexture").IsObject();
    const tinygltf::Value* volume_ext = findExtension(material, "KHR_materials_volume");
    const tinygltf::Image* mr_img = getTextureImage(model, mr.metallicRoughnessTexture.index);
    const bool has_metal_roughness_texture = mr_img != nullptr;

    const bool can_import_as_smooth_glass =
        transmission_factor >= 0.95f &&
        roughness_factor <= 0.05f &&
        metallic_factor <= 0.05f &&
        !has_metal_roughness_texture &&
        !has_transmission_texture;
    const bool can_import_as_transmission_glass_preview =
        transmission_factor >= 0.95f &&
        (has_metal_roughness_texture || metallic_factor <= 0.05f) &&
        !has_transmission_texture;

    if(
        transmission_ext &&
        transmission_factor > 0.0f &&
        !can_import_as_transmission_glass_preview
    ) {
        appendUniqueLine(
            warnings,
            "KHR_materials_transmission material kept as PBR because transmission textures or metallic transmission are not implemented."
        );
    }

    if(can_import_as_transmission_glass_preview) {
        auto glass = std::make_shared<SmoothGlassMaterial>();
        Color tint(
            static_cast<float>(mr.baseColorFactor.size() > 0 ? mr.baseColorFactor[0] : 1.0),
            static_cast<float>(mr.baseColorFactor.size() > 1 ? mr.baseColorFactor[1] : 1.0),
            static_cast<float>(mr.baseColorFactor.size() > 2 ? mr.baseColorFactor[2] : 1.0)
        );
        if(volume_ext) {
            tint *= getExtensionColor3(
                material,
                "KHR_materials_volume",
                "attenuationColor",
                Color(1.0f)
            );
            appendUniqueLine(
                warnings,
                "KHR_materials_volume approximated as smooth glass attenuation tint; volume scattering is not implemented."
            );
        }
        if(!can_import_as_smooth_glass) {
            appendUniqueLine(
                warnings,
                "KHR_materials_transmission rough/textured material approximated as smooth textured glass preview; rough transmission scattering is not implemented."
            );
            glass->surface_detail_strength = glm::clamp(0.45f + 0.45f * roughness_factor, 0.45f, 0.90f);
        }
        glass->tint = tint;
        glass->refractive_index = glm::max(
            1.0f,
            getMaterialExtensionNumber(material, "KHR_materials_ior", "ior", 1.5f)
        );

        if(const tinygltf::Image* base_color_img = getTextureImage(model, mr.baseColorTexture.index)) {
            glass->base_color = buildColorImage(*base_color_img, true);
            glass->base_color_mapping = getTextureMapping(mr.baseColorTexture, warnings);
        } else if(mr_img) {
            glass->base_color = buildTransmissionDetailImage(*mr_img);
            glass->base_color_mapping = getTextureMapping(mr.metallicRoughnessTexture, warnings);
        }
        if(const tinygltf::Image* normal_img = getTextureImage(model, material.normalTexture.index)) {
            glass->normal = buildNormalImage(*normal_img, static_cast<float>(material.normalTexture.scale));
            glass->normal_mapping = getTextureMapping(material.normalTexture, warnings);
        }
        return glass;
    }

    auto pbr = std::make_shared<PBRMaterial>();
    pbr->double_sided = material.doubleSided;

    pbr->tint = ColorA(
        static_cast<float>(mr.baseColorFactor.size() > 0 ? mr.baseColorFactor[0] : 1.0),
        static_cast<float>(mr.baseColorFactor.size() > 1 ? mr.baseColorFactor[1] : 1.0),
        static_cast<float>(mr.baseColorFactor.size() > 2 ? mr.baseColorFactor[2] : 1.0),
        static_cast<float>(mr.baseColorFactor.size() > 3 ? mr.baseColorFactor[3] : 1.0)
    );

    const tinygltf::Image* readable_guide_img = nullptr;
    if(const tinygltf::Image* base_color_img = getTextureImage(model, mr.baseColorTexture.index)) {
        const bool readable_guide =
            materialNameLooksLikeGuide(material.name) &&
            looksLikeDarkTransparentAnnotationTexture(*base_color_img);
        pbr->base_color = readable_guide
            ? buildReadableGuideImage(
                *base_color_img,
                material.alphaMode,
                static_cast<float>(material.alphaCutoff)
            )
            : buildBaseColorImage(
                *base_color_img,
                true,
                material.alphaMode,
                static_cast<float>(material.alphaCutoff)
            );
        pbr->base_color_mapping = getTextureMapping(mr.baseColorTexture, warnings);
        if(readable_guide) {
            readable_guide_img = base_color_img;
            appendUniqueLine(
                warnings,
                "Dark transparent guide/label texture remapped to light annotations for dark preview backgrounds."
            );
        }
    } else if(material.alphaMode == "OPAQUE") {
        pbr->tint.a = 1.0f;
    } else if(material.alphaMode == "MASK") {
        pbr->tint.a = (pbr->tint.a >= static_cast<float>(material.alphaCutoff)) ? 1.0f : 0.0f;
    }

    if(const tinygltf::Image* normal_img = getTextureImage(model, material.normalTexture.index)) {
        pbr->normal = buildNormalImage(*normal_img, static_cast<float>(material.normalTexture.scale));
        pbr->normal_mapping = getTextureMapping(material.normalTexture, warnings);
    }

    if(const tinygltf::Image* occlusion_img = getTextureImage(model, material.occlusionTexture.index)) {
        pbr->occlusion = buildOcclusionImage(*occlusion_img, static_cast<float>(material.occlusionTexture.strength));
        pbr->occlusion_mapping = getTextureMapping(material.occlusionTexture, warnings);
    }

    float emissive_strength = getEmissiveStrength(material);
    pbr->emissive_power = emissive_strength * Color(
        static_cast<float>(material.emissiveFactor.size() > 0 ? material.emissiveFactor[0] : 0.0),
        static_cast<float>(material.emissiveFactor.size() > 1 ? material.emissiveFactor[1] : 0.0),
        static_cast<float>(material.emissiveFactor.size() > 2 ? material.emissiveFactor[2] : 0.0)
    );
    if(const tinygltf::Image* emissive_img = getTextureImage(model, material.emissiveTexture.index)) {
        pbr->emissive = buildColorImage(*emissive_img, true);
        pbr->emissive_mapping = getTextureMapping(material.emissiveTexture, warnings);
    }
    if(readable_guide_img) {
        pbr->emissive = buildGuideEmissionImage(*readable_guide_img);
        pbr->emissive_power = Color(1.8f);
        pbr->emissive_mapping = pbr->base_color_mapping;
        pbr->contributes_emission_to_lighting = false;
        pbr->casts_shadows = false;
    }

    if(mr_img) {
        pbr->metal_roughness = buildMetalRoughnessImage(*mr_img, metallic_factor, roughness_factor);
        pbr->metal_roughness_mapping = getTextureMapping(mr.metallicRoughnessTexture, warnings);
    } else if(glm::abs(metallic_factor) > GLTF_EPSILON || glm::abs(roughness_factor - 1.0f) > GLTF_EPSILON) {
        pbr->metal_roughness = makeSolidColorImage(Color(
            glm::clamp(metallic_factor, 0.0f, 1.0f),
            glm::clamp(roughness_factor, 0.0f, 1.0f),
            0.0f
        ));
    }

    if(iridescence_ext) {
        appendUniqueLine(
            warnings,
            "KHR_materials_iridescence is approximated as softened tinted PBR; true thin-film interference is not implemented."
        );
        ColorA base = pbr->tint;
        if(pbr->base_color) {
            base = pbr->sampleBaseColor(glm::vec2(0.5f));
        }
        const Color base_rgb(base);
        if(base.a > 0.0f) {
            const Color preview_tint = computeIridescencePreviewTint(material);
            const Color preview_color = maxChannel(base_rgb) < 0.08f
                ? 0.80f * preview_tint
                : glm::mix(base_rgb, preview_tint, 0.28f);
            pbr->tint = ColorA(preview_color, base.a);
            pbr->base_color = nullptr;
            const float preview_metallic = glm::min(glm::clamp(metallic_factor, 0.0f, 1.0f), 0.55f);
            const float preview_roughness = glm::max(glm::clamp(roughness_factor, 0.0f, 1.0f), 0.58f);
            pbr->metal_roughness = makeSolidColorImage(Color(preview_metallic, preview_roughness, 0.0f));
        }
    }

    return pbr;
}

std::shared_ptr<Material> createDefaultMaterial() {
    auto pbr = std::make_shared<PBRMaterial>();
    pbr->tint = ColorA(0.8f, 0.8f, 0.8f, 1.0f);
    pbr->metal_roughness = makeSolidColorImage(Color(0.0f, 0.75f, 0.0f));
    return pbr;
}

std::shared_ptr<Material> convertMaterialFromGltf(
    const tinygltf::Model& model,
    int material_index
) {
    if(material_index < 0 || material_index >= static_cast<int>(model.materials.size())) {
        return createDefaultMaterial();
    }
    std::string warnings;
    return createMaterialFromGLTF(model, model.materials[material_index], warnings);
}

} // namespace io::gltf
