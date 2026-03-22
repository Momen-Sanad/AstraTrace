# Phase 1 Comparison Report

## Overall Status
- Core renderer + interaction loop: **Implemented**
- glTF ingestion pipeline: **Implemented**
- Acceleration structure (BVH/TLAS): **Not implemented yet**
- Practical pre-BVH performance work: **Implemented**

## Requirement-by-Requirement

| Phase 1 requirement | Status | Current evidence |
|---|---|---|
| Support ASCII + Binary glTF 2.0 loading; executable takes glTF filename | Implemented | `src/main.cpp:23`, `src/main.cpp:28`, `src/main.cpp:59`; `src/gltf_loader.cpp:777`, `src/gltf_loader.cpp:779` |
| Compute normals/tangents if missing | Implemented | Fallback regeneration in loader: `src/gltf_loader.cpp:541`, `src/gltf_loader.cpp:548` |
| PBR shading (Lambert + Cook-Torrance GGX) | Implemented | Used in ray trace path (`computeLambertDiffuseAndGGXSpecular` call): `src/scene.cpp:247` |
| BaseColor / Metallic / Roughness / Emissive / AO / Normal mapping | Implemented | Material mapping path in loader: `src/gltf_loader.cpp:376` through material texture assignment blocks |
| `KHR_materials_emissive_strength` | Implemented | Parsed and applied: `src/gltf_loader.cpp:366` |
| Punctual lights from glTF (`KHR_lights_punctual`) | Implemented | Node light import path: `src/gltf_loader.cpp:718`, `src/gltf_loader.cpp:847` |
| Load initial camera from glTF (when available) | Implemented | Camera import and apply: `src/gltf_loader.cpp:691`, `src/gltf_loader.cpp:822` |
| First-person camera controls | Implemented | WASD + mouse + wheel in controller: `src/camera_controller.cpp:61`, `src/camera_controller.cpp:27`, `src/camera_controller.cpp:31` |
| Screenshot on `P` key | Implemented | Hotkey handling in main loop: `src/main.cpp:94`, `src/main.cpp:135` |
| BVH acceleration | Missing | No BVH/TLAS structure yet; triangle loop still brute-force: `src/shape.cpp:81`, `src/shape.cpp:84` |

---
### Added
- Dedicated glTF loader (`src/gltf_loader.hpp/.cpp`) with scene graph transforms, camera import, lights import, and PBR material translation.
- CLI-driven scene loading in `main` instead of hardcoded scene content.
- CMake wiring for `tinygltf + nlohmann/json` and portable OpenMP.
- Better run scripts (`run.ps1`, `run.sh`) for configure/build/run with scene argument handling.

### Performance Upgrades (pre-BVH)
- Faster triangle test (Moller-Trumbore): `src/shape.cpp:31`
- Per-object broad-phase culling via bounding sphere: `src/scene.cpp:7`, `src/scene.cpp:48`, `src/scene.cpp:138`
- Identity-transform fast path (skip per-ray transforms): `src/scene.cpp:33`, `src/scene.cpp:53`, `src/scene.cpp:83`
- Static OpenMP scheduling + chunked row rendering to keep app responsive: `src/main.cpp:117`, `src/main.cpp:113`

## Remaining Phase 1 Gap
- **Main blocker still pending:** BVH/TLAS acceleration.
- Current state has meaningful pre-BVH optimizations, but complex scenes remain slow