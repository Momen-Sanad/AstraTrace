#pragma once

#include <memory>
#include "scene/camera/camera.hpp"
#include "scene/world/scene.hpp"
#include "core/color.hpp"
#include "core/image.hpp"

namespace render {

enum class RenderBackend {
    CpuWhitted,
    CpuPath,
    GpuPath,
};

class IRenderer {
public:
    virtual ~IRenderer() = default;
    virtual void render(const Scene& scene, const Camera& camera, Image<Color8>& output) = 0;
};

std::unique_ptr<IRenderer> createRenderer(RenderBackend backend);

} // namespace render
