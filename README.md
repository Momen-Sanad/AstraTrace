# AstraTrace

AstraTrace is a physically based rendering engine built in C++ for glTF 2.0 scenes.  
It combines BVH-accelerated software ray tracing, production-style path tracing features, and real-time GPU software ray tracing in one unified project.

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
- Fallback normal/tangent generation for incomplete mesh data
- First-person camera controls and screenshot capture (`P` key)
- BVH-accelerated traversal for scalable scene intersection

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
- SVGF denoising (spatiotemporal variance guided filtering)

### 3) GPU Software Path Tracing
- GPU execution of the same physically based light transport model
- Software ray tracing (no hardware RTX dependency)
- Interactive target throughput (`>=30 FPS` at `1280x720`)
- CPU preprocessing support (including BVH build)

## Tech Stack

- C++20
- CMake
- SDL3
- GLM
- tinygltf + nlohmann/json
- stb
- OpenMP

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
cmake --build build --config Release --target check_include_boundaries
```

Backends:
- `cpu-whitted` (default)
- `cpu-path`
- `gpu-path`

## Runtime Controls

- `W/A/S/D` or arrow keys: move
- `Q/E`: vertical movement
- `Left mouse button + move`: look around
- `Mouse wheel`: zoom (FOV)
- `Left Shift`: sprint
- `P`: save screenshot

## Example Scenes

- `scenes/Duck/Duck.gltf`
- `scenes/Lantern/Lantern.gltf`
- `scenes/Buggy/Buggy.gltf`
- `cornell-box-1.glb`
- `cornell-box-2.glb`
- `sponza.glb`

## Repository Layout

```text
include/    Public API headers
src/app/    App bootstrap, frame loop, input/runtime orchestration
src/platform/sdl/ SDL-specific screenshot/window helpers
src/core/   Foundational types (color, ray, image)
src/scene/  Camera, world, geometry, materials, lights
src/io/gltf/ glTF loading/parsing/build pipeline
src/render/ Rendering interfaces and CPU/GPU backends
scenes/     Sample glTF scenes
assets/     Runtime assets and textures
vendor/     Third-party dependencies
build/      Build output (generated)
```

Detailed per-file responsibility map:
- See [STRUCTURE_README.md](STRUCTURE_README.md)
