## 1) glTF Loading
Files:
- `src/gltf_loader.hpp`
- `src/gltf_loader.cpp`

What was added:
- New API: `loadSceneFromGLTF(path, scene, camera, fallback_aspect_ratio)`.
- Supports both `.gltf` and `.glb`.
- Parses full node hierarchy and applies world transforms.
- Imports meshes as `TriangleMesh` objects (triangle topology).
- Imports materials into `PBRMaterial`:
  - base color (+ alpha mode/cutoff)
  - normal
  - occlusion
  - metallic-roughness
  - emissive (+ `KHR_materials_emissive_strength`)
- Imports glTF camera (perspective) when present.
- Imports glTF punctual lights (`directional`, `point`, `spot`) when present.
- If scene has no glTF lights, adds fallback key+fill directional lights and ambient/background defaults.
- Returns detailed warnings/errors and loaded counts (`object_count`, `light_count`).

Fixes applied during integration:
- Explicit tinygltf + nlohmann/json wiring:
  - `TINYGLTF_NO_INCLUDE_JSON`
  - include `<nlohmann/json.hpp>` from vendored json.
- Corrected base-color alpha handling (no gamma conversion on alpha).
- Corrected glTF texture orientation path in loader image sampling.

## 2) Main App Flow / Render Loop
File:
- `src/main.cpp`

What changed:
- App now expects a glTF path argument (`<scene.gltf | scene.glb>`).
- Hardcoded demo scene setup was removed; scene now comes from glTF loader.
- Rendering loop changed to chunked rows (`RENDER_ROW_CHUNK`) with event pumping between chunks.
- This keeps the window responsive during very long frame times.
- OpenMP scheduling updated to `schedule(static)` for lower overhead.

## 3) Performance Upgrades (pre-BVH)
Files:
- `src/shape.hpp`
- `src/shape.cpp`

Triangle/ray optimizations:
- Triangle intersection switched to Moller-Trumbore (removed per-triangle matrix inverse).
- Added tighter hit epsilon handling to reduce self-intersection artifacts.

Broad-phase scene culling:
- Added shape bounds API (`AABB getBounds()`).
- Triangle mesh and sphere now provide bounds.
- Scene objects cache a world-space bounding sphere.
- Added fast `mayIntersect()` check before expensive shape intersection.
- Closest-hit traversal now shrinks candidate distance as soon as better hits are found.

Transform fast-path:
- If object transform is identity, skip ray transform into local space and sample shape directly.
- This avoids many matrix ops in common glTF cases.

## 4) CMake / Build System
File:
- `CMakeLists.txt`

What changed:
- Added `src/gltf_loader.cpp` to executable sources.
- Added include dirs for vendored tinygltf + json.
- Replaced hardcoded OpenMP/link flags with portable:
  - `find_package(OpenMP)`
  - `target_link_libraries(... OpenMP::OpenMP_CXX)` when found.
- Default build type is now `Release` for single-config generators (when not explicitly set).

## 5) Run Scripts (Windows + Bash)
Files:
- `run.ps1`
- `run.sh`

What changed:
- Behavior: if executable exists, run it; otherwise rebuild from scratch.
- Default config moved to `Release`.
- Supports calling with scene directly:
  - PowerShell: `.\run.ps1 .\scenes\Duck\Duck.gltf`
  - Bash: `./run.sh ./scenes/Duck/Duck.gltf`
- First arg is treated as config only if it matches known CMake configs.
- Detects build-type mismatch from `CMakeCache.txt` and reconfigures cleanly.
- Auto-selects a default scene if none is passed.
- Normalizes scene path to absolute before changing working directory.

## 6) Current State / Known Limits
- Quality is prioritized (full fallback key+fill lighting restored for no-light glTF scenes).
- `Buggy` now opens and remains responsive, but still very slow (~software ray tracing + no BVH).
- Next major performance step is BVH