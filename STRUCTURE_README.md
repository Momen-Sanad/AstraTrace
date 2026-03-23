# AstraTrace Structure README

This file documents the role of each source file in the current modular layout.

## Top-Level Architecture

- `include/astratrace/*`: lightweight public include surface.
- `src/app/*`: executable startup and runtime orchestration.
- `src/platform/*`: platform-specific helpers (currently SDL).
- `src/core/*`: core data types and low-level utilities.
- `src/scene/*`: scene data model and geometry/material/light systems.
- `src/io/*`: scene loading and import pipeline.
- `src/render/*`: renderer interfaces and backend implementations.
- `tools/*`: dev tooling and architecture checks.

## Public API Includes (`include/astratrace`)

- `include/astratrace/core/color.hpp`: public color API include.
- `include/astratrace/core/image.hpp`: public image API include.
- `include/astratrace/core/ray.hpp`: public ray API include.
- `include/astratrace/scene/camera.hpp`: public camera API include.
- `include/astratrace/scene/scene.hpp`: public scene API include.
- `include/astratrace/render/renderer.hpp`: public renderer interface include.

## Application Layer (`src/app`)

- `src/app/main.cpp`: app entry point; launches `Application`.
- `src/app/application.hpp`: `Application` interface.
- `src/app/application.cpp`: program bootstrap (CLI parse, SDL init, scene load, backend select).
- `src/app/frame_loop.hpp`: frame loop interface binding app subsystems.
- `src/app/frame_loop.cpp`: event/update/render loop + FPS title + screenshot trigger.
- `src/app/input/camera_controller.hpp`: camera input controller interface.
- `src/app/input/camera_controller.cpp`: WASD/mouse/wheel camera control logic.
- `src/app/runtime/fps_tracker.hpp`: FPS and frame-time tracker utility.
- `src/app/runtime/timer.hpp`: scoped/simple timing utility (`TimeIt`).

## Platform Layer (`src/platform`)

- `src/platform/sdl/screenshot.hpp`: SDL screenshot capture helper.

## Core Layer (`src/core`)

- `src/core/color.hpp`: color types (`Color`, `ColorA`, `Color8`) + gamma/tonemap helpers.
- `src/core/ray.hpp`: `Ray` and `RayHit` definitions.
- `src/core/image.hpp`: `Image<T>` container + sampling/loading declarations.
- `src/core/image.cpp`: stb-based image loading specializations.

## Scene Layer (`src/scene`)

### Camera
- `src/scene/camera/camera.hpp`: camera transform/projection and ray generation.

### Geometry
- `src/scene/geometry/shape_base.hpp`: common geometry interfaces/types (`Shape`, `SurfaceData`, `AABB`).
- `src/scene/geometry/triangle.hpp`: triangle primitive declaration.
- `src/scene/geometry/triangle.cpp`: triangle intersection/surface interpolation.
- `src/scene/geometry/triangle_mesh.hpp`: triangle-mesh declaration.
- `src/scene/geometry/triangle_mesh.cpp`: mesh bounds/intersection traversal.
- `src/scene/geometry/sphere.hpp`: sphere primitive declaration.
- `src/scene/geometry/sphere.cpp`: sphere intersection/surface mapping.
- `src/scene/geometry/obj_loader.hpp`: procedural mesh + OBJ loader declarations.
- `src/scene/geometry/obj_loader.cpp`: cuboid/rect generation + tinyobj OBJ loading.
- `src/scene/geometry/geometry.hpp`: umbrella include for geometry module.

### Materials
- `src/scene/materials/material_base.hpp`: abstract `Material` base.
- `src/scene/materials/pbr_material.hpp`: PBR material data + texture sampling.
- `src/scene/materials/smooth_mirror_material.hpp`: mirror material data/sampling.
- `src/scene/materials/smooth_glass_material.hpp`: glass material data/sampling.
- `src/scene/materials/materials.hpp`: umbrella include for material module.

### Lights
- `src/scene/lights/light_base.hpp`: `Light` base and `LightEvaluation`.
- `src/scene/lights/directional_light.hpp`: directional light declaration.
- `src/scene/lights/directional_light.cpp`: directional light evaluation.
- `src/scene/lights/point_light.hpp`: point light declaration.
- `src/scene/lights/point_light.cpp`: point light evaluation.
- `src/scene/lights/spot_light.hpp`: spotlight declaration.
- `src/scene/lights/spot_light.cpp`: spotlight evaluation.
- `src/scene/lights/lights.hpp`: umbrella include for light module.

### World
- `src/scene/world/scene_object.hpp`: scene object transform/material/shape binding.
- `src/scene/world/scene_object.cpp`: object transform updates and broad-phase checks.
- `src/scene/world/scene.hpp`: scene container API (objects/lights/intersection queries).
- `src/scene/world/scene.cpp`: scene traversal/stats implementation.
- `src/scene/world/world.hpp`: umbrella include for world module.

## IO Layer (`src/io/gltf`)

- `src/io/gltf/gltf_loader.hpp`: public glTF scene-load API (`io::gltf::loadSceneFromGLTF`).
- `src/io/gltf/gltf_loader.cpp`: main glTF load pipeline implementation.
- `src/io/gltf/gltf_parser.hpp`: glTF parse abstraction declaration.
- `src/io/gltf/gltf_parser.cpp`: tinygltf file parsing implementation.
- `src/io/gltf/gltf_scene_builder.hpp`: scene-build abstraction declaration.
- `src/io/gltf/gltf_scene_builder.cpp`: scene-build placeholder module.
- `src/io/gltf/gltf_material_converter.hpp`: material conversion abstraction declaration.
- `src/io/gltf/gltf_material_converter.cpp`: material conversion placeholder module.
- `src/io/gltf/gltf_texture_cache.hpp`: texture cache abstraction declaration.
- `src/io/gltf/gltf_texture_cache.cpp`: texture cache placeholder module.

## Render Layer (`src/render`)

### Renderer Interface
- `src/render/renderer.hpp`: renderer abstraction (`IRenderer`) and backend enum/factory.
- `src/render/renderer.cpp`: backend factory implementation.

### Shared Rendering Utilities
- `src/render/common/brdf.hpp`: BRDF/Fresnel/GGX/Lambert shading helpers.
- `src/render/common/normal_mapping.hpp`: tangent-space normal-to-world helper.
- `src/render/common/shadow_utils.hpp`: shadow utility API.
- `src/render/common/shadow_utils.cpp`: scene shadowing implementation.

### CPU Backends
- `src/render/cpu/whitted_integrator.hpp`: Whitted integrator declaration.
- `src/render/cpu/whitted_integrator.cpp`: recursive Whitted tracing implementation.
- `src/render/cpu/path_integrator.hpp`: CPU path integrator declaration.
- `src/render/cpu/path_integrator.cpp`: CPU path integrator placeholder/fallback implementation.
- `src/render/cpu/cpu_whitted_renderer.hpp`: CPU Whitted renderer declaration.
- `src/render/cpu/cpu_whitted_renderer.cpp`: image rendering using Whitted integrator.
- `src/render/cpu/cpu_path_renderer.hpp`: CPU path renderer declaration.
- `src/render/cpu/cpu_path_renderer.cpp`: image rendering using path integrator.

### GPU Backend
- `src/render/gpu/gpu_path_renderer.hpp`: GPU path renderer declaration.
- `src/render/gpu/gpu_path_renderer.cpp`: GPU backend placeholder (currently routes to CPU path renderer).

## Tooling (`tools`)

- `tools/check_include_boundaries.py`: validates include-direction constraints between modules.
