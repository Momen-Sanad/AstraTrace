#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
TARGET_NAME="Lab4"
CONFIG="${1:-Release}"
case "${CONFIG,,}" in
    debug|release|relwithdebinfo|minsizerel)
        shift || true
        ;;
    *)
        # If first arg is not a known CMake config, treat it as app arg and use Release.
        set -- "${CONFIG}" "${@:2}"
        CONFIG="Release"
        ;;
esac

find_exe() {
    local candidate
    for candidate in \
        "${BUILD_DIR}/bin/${TARGET_NAME}.exe" \
        "${BUILD_DIR}/${TARGET_NAME}.exe" \
        "${BUILD_DIR}/bin/${CONFIG}/${TARGET_NAME}.exe" \
        "${BUILD_DIR}/${CONFIG}/${TARGET_NAME}.exe"; do
        if [[ -f "${candidate}" ]]; then
            echo "${candidate}"
            return 0
        fi
    done
    return 1
}

find_default_scene() {
    local candidate

    candidate="${ROOT_DIR}/assets/scene.gltf"
    if [[ -f "${candidate}" ]]; then
        echo "${candidate}"
        return 0
    fi

    candidate="${ROOT_DIR}/assets/scene.glb"
    if [[ -f "${candidate}" ]]; then
        echo "${candidate}"
        return 0
    fi

    candidate="$(find "${ROOT_DIR}/assets" -type f \( -iname '*.gltf' -o -iname '*.glb' \) 2>/dev/null | head -n 1 || true)"
    if [[ -n "${candidate}" && -f "${candidate}" ]]; then
        echo "${candidate}"
        return 0
    fi

    candidate="${ROOT_DIR}/vendor/tinygltf/models/Cube/Cube.gltf"
    if [[ -f "${candidate}" ]]; then
        echo "${candidate}"
        return 0
    fi

    return 1
}

EXE_PATH="$(find_exe || true)"
CONFIGURED_BUILD_TYPE=""
if [[ -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
    CONFIGURED_BUILD_TYPE="$(grep -m1 '^CMAKE_BUILD_TYPE:STRING=' "${BUILD_DIR}/CMakeCache.txt" | sed 's/^CMAKE_BUILD_TYPE:STRING=//' || true)"
fi

BUILD_TYPE_MISMATCH=0
if [[ -n "${CONFIGURED_BUILD_TYPE}" && "${CONFIGURED_BUILD_TYPE,,}" != "${CONFIG,,}" ]]; then
    echo "Build type mismatch (configured: ${CONFIGURED_BUILD_TYPE}, requested: ${CONFIG}). Reconfiguring..."
    BUILD_TYPE_MISMATCH=1
    EXE_PATH=""
fi

if [[ -z "${EXE_PATH}" ]]; then
    echo "No existing executable found. Rebuilding from scratch..."
    if [[ "${BUILD_TYPE_MISMATCH}" -eq 0 ]]; then
        rm -rf "${BUILD_DIR}"
    fi
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${CONFIG}"
    cmake --build "${BUILD_DIR}" --config "${CONFIG}" --target "${TARGET_NAME}"
    EXE_PATH="$(find_exe || true)"
fi

if [[ -z "${EXE_PATH}" ]]; then
    echo "Could not find ${TARGET_NAME}.exe after fresh build." >&2
    exit 1
fi

if [[ $# -eq 0 ]]; then
    DEFAULT_SCENE="$(find_default_scene || true)"
    if [[ -z "${DEFAULT_SCENE}" ]]; then
        echo "Usage: ./run.sh [Config] <scene.gltf|scene.glb> [extra args]" >&2
        exit 1
    fi
    echo "No scene argument provided. Using ${DEFAULT_SCENE}"
    set -- "${DEFAULT_SCENE}"
fi

# Normalize first argument (scene path) so it still resolves after changing working directory.
if [[ $# -gt 0 ]]; then
    if [[ -f "$1" ]]; then
        set -- "$(cd -- "$(dirname -- "$1")" && pwd)/$(basename -- "$1")" "${@:2}"
    elif [[ -f "${ROOT_DIR}/$1" ]]; then
        set -- "$(cd -- "$(dirname -- "${ROOT_DIR}/$1")" && pwd)/$(basename -- "${ROOT_DIR}/$1")" "${@:2}"
    fi
fi

# Keep a working directory where ../../assets resolves to the project assets path.
RUN_DIR="${BUILD_DIR}/${CONFIG}"
mkdir -p "${RUN_DIR}"

echo "Running ${EXE_PATH}"
(
    cd "${RUN_DIR}"
    "${EXE_PATH}" "$@"
)
