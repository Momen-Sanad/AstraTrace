#include "render/renderer.hpp"

#include "render/cpu/cpu_path_renderer.hpp"
#include "render/cpu/cpu_whitted_renderer.hpp"
#include "render/gpu/gpu_path_renderer.hpp"

namespace render {

std::unique_ptr<IRenderer> createRenderer(RenderBackend backend) {
    switch(backend) {
    case RenderBackend::CpuPath:
        return std::make_unique<cpu::CpuPathRenderer>();
    case RenderBackend::GpuPath:
        return std::make_unique<gpu::GpuPathRenderer>();
    case RenderBackend::CpuWhitted:
    default:
        return std::make_unique<cpu::CpuWhittedRenderer>();
    }
}

} // namespace render
