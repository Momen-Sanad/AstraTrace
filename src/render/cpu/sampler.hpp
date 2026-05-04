#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include "render/renderer.hpp"

namespace render::cpu {

class Sampler {
public:
    Sampler(PathSamplerMode mode, glm::ivec2 pixel, uint32_t sample_index)
        : mode(mode), pixel(pixel), sample_index(sample_index), seed(makeSeed(pixel, sample_index)) {}

    float next() {
        float value = 0.0f;
        switch(mode) {
        case PathSamplerMode::Halton:
            value = halton(dimension, sample_index);
            break;
        case PathSamplerMode::Sobol:
            value = sobol(dimension, sample_index);
            break;
        case PathSamplerMode::Random:
        default:
            value = randomFloat();
            break;
        }

        if(mode != PathSamplerMode::Random) {
            value = glm::fract(value + blueNoiseShift(pixel, dimension));
        }
        ++dimension;
        return glm::clamp(value, 0.0f, 0.99999994f);
    }

    glm::vec2 next2() { return glm::vec2(next(), next()); }
    glm::vec3 next3() { return glm::vec3(next(), next(), next()); }

    static glm::vec2 halton2D(uint32_t index) {
        return glm::vec2(haltonBase(2, index), haltonBase(3, index));
    }

private:
    static uint32_t rotl(uint32_t x, uint32_t r) {
        return (x << r) | (x >> (32u - r));
    }

    static uint32_t mix(uint32_t value) {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    }

    static uint32_t makeSeed(glm::ivec2 pixel, uint32_t sample_index) {
        uint32_t h = 0x811c9dc5u;
        h ^= mix(static_cast<uint32_t>(pixel.x) + 0x9e3779b9u);
        h = rotl(h, 13);
        h ^= mix(static_cast<uint32_t>(pixel.y) + 0x85ebca6bu);
        h = rotl(h, 17);
        h ^= mix(sample_index + 0xc2b2ae35u);
        return mix(h);
    }

    float randomFloat() {
        seed ^= seed << 13;
        seed ^= seed >> 17;
        seed ^= seed << 5;
        return static_cast<float>(seed) / 4294967295.0f;
    }

    static float haltonBase(uint32_t base, uint32_t index) {
        float f = 1.0f;
        float r = 0.0f;
        uint32_t i = index + 1u;
        while(i > 0u) {
            f /= static_cast<float>(base);
            r += f * static_cast<float>(i % base);
            i /= base;
        }
        return r;
    }

    static float halton(uint32_t dimension, uint32_t index) {
        static constexpr uint32_t bases[] = {
            2, 3, 5, 7, 11, 13, 17, 19,
            23, 29, 31, 37, 41, 43, 47, 53
        };
        return haltonBase(bases[dimension % 16u], index);
    }

    static uint32_t reverseBits(uint32_t x) {
        x = ((x & 0xffff0000u) >> 16) | ((x & 0x0000ffffu) << 16);
        x = ((x & 0xff00ff00u) >> 8) | ((x & 0x00ff00ffu) << 8);
        x = ((x & 0xf0f0f0f0u) >> 4) | ((x & 0x0f0f0f0fu) << 4);
        x = ((x & 0xccccccccu) >> 2) | ((x & 0x33333333u) << 2);
        x = ((x & 0xaaaaaaaau) >> 1) | ((x & 0x55555555u) << 1);
        return x;
    }

    static float sobol(uint32_t dimension, uint32_t index) {
        uint32_t x = index + 1u;
        x ^= x >> 1;
        uint32_t scramble = mix(dimension * 0x9e3779b9u + 0x243f6a88u);
        uint32_t value = reverseBits(x) ^ scramble;
        value = mix(value + dimension * 0x632be59bu);
        return static_cast<float>(value) / 4294967295.0f;
    }

    static float blueNoiseShift(glm::ivec2 pixel, uint32_t dimension) {
        uint32_t h = makeSeed(glm::ivec2(pixel.x & 63, pixel.y & 63), dimension);
        return (static_cast<float>(h & 0x00ffffffu) + 0.5f) / 16777216.0f;
    }

    PathSamplerMode mode = PathSamplerMode::Sobol;
    glm::ivec2 pixel = glm::ivec2(0);
    uint32_t sample_index = 0;
    uint32_t dimension = 0;
    uint32_t seed = 1;
};

} // namespace render::cpu
