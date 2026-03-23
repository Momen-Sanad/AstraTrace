#include "render/cpu/path_integrator.hpp"

namespace render::cpu {

Color PathIntegrator::trace(const Scene& scene, const Ray& ray) const {
    // Placeholder path integrator implementation during backend split.
    // Keeps architecture and interface ready while preserving current output behavior.
    return fallback.trace(scene, ray);
}

} // namespace render::cpu
