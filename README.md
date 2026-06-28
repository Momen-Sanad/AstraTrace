# AstraTrace

AstraTrace is a C++20 physically based software renderer for glTF 2.0 scenes.
It combines Whitted-style ray tracing, progressive CPU path tracing, textured PBR materials, and an interactive SDL/ImGui viewer.

## Core Capabilities

- glTF 2.0 scene ingestion (`.gltf` and `.glb`) through CLI scene arguments
- Scene graph transform support with camera and punctual light import
- Physically based shading:
  - Lambert diffuse BRDF
  - Cook-Torrance GGX specular BRDF
- Full PBR texture workflow:
  - Base color, metallic, roughness, emissive, occlusion, normal
  - `KHR_materials_emissive_strength`
  - `KHR_lights_punctual`
  - Conservative smooth-glass mapping for simple `KHR_materials_transmission` + `KHR_materials_ior`
  - Softened tinted preview fallback for `KHR_materials_iridescence`
- Fallback normal/tangent generation for incomplete mesh data
- First-person camera controls and screenshot capture (`P` key)
- BVH-accelerated traversal:
  - Top-level object BVH for scene objects with diagnostics and optional refit hooks
  - Per-mesh triangle BVH using median splits on the longest centroid axis
- Built-in material showcase scene for glass, smooth mirror/metal, rough metal, and dielectric PBR
- Non-interactive PNG export for reproducible portfolio captures, with async CPU path progress and Ctrl+C cancellation
- Optional image-based environment lighting from LDR/HDR equirectangular maps

## Rendering Pipeline

### 1) CPU Whitted Ray Tracing
- Recursive reflection/refraction paths
- PBR local lighting with hard/transparent shadow handling
- Texture-aware material evaluation and normal mapping

### 2) CPU Path Tracing
- BRDF importance sampling
- Next Event Estimation (NEE)
- Russian Roulette termination
- Low-discrepancy sampling (LDS)
- Emissive-material light transport
- Temporal accumulation and simplified SVGF-style denoising with history reprojection
- Tiled CPU path rendering with progress/status reporting and cancelable export batches
- Uniform, power-based, and contribution-weighted light selection
- Optional OIDN denoiser hook behind `ASTRATRACE_ENABLE_OIDN=ON`

## Feature Notes

- `SVGF` in the UI is a compact SVGF-style preview denoiser, not a full paper implementation.
- `OIDN` is optional and unavailable in default builds unless Open Image Denoise is enabled and found by CMake.
- `Contribution` light sampling estimates local light contribution per shading point; it is not a persistent light-tree hierarchy.
- `KHR_materials_iridescence` is approximated as tinted rough PBR for visibility; true thin-film interference is deferred.
- Compressed sample variants that require `KHR_texture_basisu`/KTX or `KHR_draco_mesh_compression` are not supported in this CPU-only portfolio pass; use the regular `.gltf` or uncompressed `.glb` scenes.
- The project is CPU-only. Build output or third-party dependency logs may mention graphics APIs internally, but AstraTrace does not expose a GPU renderer.

## Tech Stack

- C++20
- CMake
- SDL3
- GLM
- tinygltf + nlohmann/json
- stb
- OpenMP
- Optional: Intel Open Image Denoise

## Build and Run

### Quick Run

```bash
./run.sh Release ./scenes/Duck/Duck.gltf
```

```powershell
./run.ps1 Release .\scenes\Duck\Duck.gltf
```

If no scene is provided, the scripts try an automatic default scene.

### Manual CMake

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
./build/bin/astratrace_app ./scenes/Duck/Duck.gltf --backend cpu-whitted
./build/bin/astratrace_app --showcase material --backend cpu-path
./build/bin/astratrace_app --showcase material --backend cpu-path --export screenshots/portfolio/material-showcase.png --width 1280 --height 720 --samples 64 --bounces 5 --denoiser temporal
./build/bin/astratrace_app --showcase material --backend cpu-path --environment <env.hdr> --environment-strength 1.0
cmake --build build --config Release --target check_include_boundaries
cmake --build build --config Release --target check_path_phase2_sanity
cmake --build build --config Release --target benchmark_bvh
```

Backends:
- `cpu-whitted` (default)
- `cpu-path`

Export options:
- `--export <out.png>`
- `--width <pixels>` / `--height <pixels>`
- `--samples <spp>` and `--bounces <count>` for `cpu-path`; `none` and `temporal` exports accumulate the requested total samples
- `--denoiser none|temporal|svgf|oidn`; `svgf` is a preview-style export path, `oidn` requires an OIDN-enabled build
- `--sampler random|halton|sobol`
- `--light-sampler uniform|power|contribution`
- `--environment <image.hdr|png|jpg>` and `--environment-strength <float>`

CPU path export runs as an asynchronous job in small sample batches and logs progress. Press `Ctrl+C` to cancel before the PNG is written.

## Runtime Controls

- `W/A/S/D` or arrow keys: move
- `Q/E`: vertical movement
- `Left mouse button + move`: look around
- `Mouse wheel`: zoom (FOV)
- `Left Shift`: sprint
- `P`: save screenshot

## Example Scenes

- Built-in: `--showcase material`
- `scenes/Duck/Duck.gltf`
- `scenes/Lantern/Lantern.gltf`
- `scenes/Buggy/Buggy.gltf`
- `cornell-box-1.glb`
- `cornell-box-2.glb`
- `sponza.glb`

## Portfolio Capture Recipe

```powershell
.\build_phase2_mingw\bin\astratrace_app.exe --showcase material --backend cpu-path --export screenshots\portfolio\material-showcase.png --width 1280 --height 720 --samples 64 --bounces 5 --denoiser temporal
.\build_phase2_mingw\bin\astratrace_app.exe scenes\cornell-box-1.glb --backend cpu-path --export screenshots\portfolio\cornell.png --width 1280 --height 720 --samples 64 --bounces 5 --denoiser temporal
.\build_phase2_mingw\bin\astratrace_app.exe scenes\Lantern\Lantern.gltf --backend cpu-whitted --export screenshots\portfolio\lantern.png --width 1280 --height 720
.\build_phase2_mingw\bin\astratrace_app.exe scenes\sponza.glb --backend cpu-whitted --export screenshots\portfolio\sponza.png --width 1280 --height 720
```

## Repository Layout

```text
include/    Public API headers
src/app/    App bootstrap, frame loop, input/runtime orchestration
src/platform/sdl/ SDL-specific screenshot/window helpers
src/core/   Foundational types (color, ray, image)
src/scene/  Camera, world, geometry, materials, lights
src/io/gltf/ glTF loading/parsing/build pipeline
src/render/ Rendering interfaces and CPU backends
scenes/     Sample glTF scenes
assets/     Runtime assets and textures
vendor/     Third-party dependencies
build/      Build output (generated)
```

Detailed per-file responsibility map:
- See [STRUCTURE_README.md](STRUCTURE_README.md)
