#!/usr/bin/env bash
# Build griddick_tnc.uf2. No command-line arguments.
# Env:
#   BUILD_TYPE   Release (default) or Debug
#   BUILD_GEN    optional CMake generator (Ninja, Unix Makefiles)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FW_BUILD="${ROOT}/build/firmware"
SDK_DIR="${ROOT}/third_party/pico-sdk"
export PICO_SDK_PATH="${PICO_SDK_PATH:-${SDK_DIR}}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

echo "ROOT: ${ROOT}"
echo "PICO_SDK_PATH: ${PICO_SDK_PATH}"
echo "BUILD_TYPE: ${BUILD_TYPE}"

if [[ ! -f "${ROOT}/pico_sdk_import.cmake" ]]; then
    echo "Missing ${ROOT}/pico_sdk_import.cmake" >&2
    exit 1
fi

if [[ ! -f "${PICO_SDK_PATH}/pico_sdk_init.cmake" ]]; then
    echo "Pico SDK not found at ${PICO_SDK_PATH}" >&2
    echo "Run: ${ROOT}/setup_env.sh" >&2
    exit 1
fi

if ! command -v arm-none-eabi-gcc >/dev/null; then
    echo "arm-none-eabi-gcc not found. Run: ${ROOT}/setup_env.sh" >&2
    exit 1
fi

# Debian's ninja-build package installs /usr/bin/ninja only. CMake still
# probes /usr/bin/ninja-build first and fails if a cache remembered it.
cache_make_missing() {
    local cache="$1" prog
    [[ -f "${cache}" ]] || return 1
    prog="$(sed -n 's/^CMAKE_MAKE_PROGRAM:[^=]*=//p' "${cache}" | head -1)"
    [[ -n "${prog}" && ! -x "${prog}" ]]
}

if [[ -f "${FW_BUILD}/CMakeCache.txt" ]]; then
    if cache_make_missing "${FW_BUILD}/CMakeCache.txt" || \
       ! grep -q 'arm-none-eabi-gcc' "${FW_BUILD}/CMakeCache.txt" || \
       grep -q 'INTERPROCEDURAL_OPTIMIZATION:BOOL=ON' "${FW_BUILD}/CMakeCache.txt"; then
        echo "==> Removing stale CMake cache in ${FW_BUILD}"
        rm -rf "${FW_BUILD}"
    fi
fi

CMAKE_ARGS=(
    -S "${ROOT}"
    -B "${FW_BUILD}"
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DPICO_BOARD=pico
    -DPICO_SDK_PATH="${PICO_SDK_PATH}"
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
    -DCMAKE_PREFIX_PATH="${ROOT}/tools"
)
add_generator() {
    # $1 is the cmake-args array name.
    local -n _args="$1"
    local ninja
    if [[ -n "${BUILD_GEN:-}" ]]; then
        _args+=(-G "${BUILD_GEN}")
        if [[ "${BUILD_GEN}" == "Ninja" ]]; then
            ninja="$(command -v ninja || true)"
            [[ -n "${ninja}" && -x "${ninja}" ]] || {
                echo "BUILD_GEN=Ninja but ninja is not executable" >&2
                exit 1
            }
            _args+=(-DCMAKE_MAKE_PROGRAM="${ninja}")
        fi
        return
    fi
    ninja="$(command -v ninja || true)"
    if [[ -n "${ninja}" && -x "${ninja}" ]] && "${ninja}" --version >/dev/null 2>&1; then
        _args+=(-G Ninja -DCMAKE_MAKE_PROGRAM="${ninja}")
    fi
}
add_generator CMAKE_ARGS
if command -v ccache >/dev/null; then
    CMAKE_ARGS+=(-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache)
fi

echo "==> Firmware (Pico)..."
cmake "${CMAKE_ARGS[@]}"
cmake --build "${FW_BUILD}" --target griddick_tnc -j"$(nproc)"

UF2="$(find "${FW_BUILD}" -name 'griddick_tnc.uf2' -print -quit || true)"
if [[ -z "${UF2}" || ! -f "${UF2}" ]]; then
    echo "Build finished but griddick_tnc.uf2 was not produced under ${FW_BUILD}" >&2
    find "${FW_BUILD}" -name '*.uf2' -print >&2 || true
    exit 1
fi

cp -f "${UF2}" "${ROOT}/griddick_tnc.uf2"
ELF="${UF2%.uf2}.elf"
if [[ -f "${ELF}" ]] && command -v arm-none-eabi-size >/dev/null; then
    echo "==> size"
    arm-none-eabi-size "${ELF}"
fi
if [[ -f "${FW_BUILD}/compile_commands.json" ]]; then
    ln -sfn "${FW_BUILD}/compile_commands.json" "${ROOT}/compile_commands.json"
fi

echo "    UF2: ${UF2}"
echo "    copy: ${ROOT}/griddick_tnc.uf2"

HOST_BUILD="${ROOT}/build/host"
HOST_BIN="${ROOT}/host/bin"
echo "==> host (libgtnc + gtlog gttime gttx gtwav gtfeed gtcfg gtdump)..."
HOST_CMAKE_ARGS=(
    -S "${ROOT}/host"
    -B "${HOST_BUILD}"
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
)
if cache_make_missing "${HOST_BUILD}/CMakeCache.txt"; then
    echo "==> Removing stale CMake cache in ${HOST_BUILD}"
    rm -rf "${HOST_BUILD}"
fi
add_generator HOST_CMAKE_ARGS
cmake "${HOST_CMAKE_ARGS[@]}"
cmake --build "${HOST_BUILD}" -j"$(nproc)"
echo "    libgtnc: ${ROOT}/host/lib/libgtnc.so"
echo "    gtlog: ${HOST_BIN}/gtlog"
echo "    gttime: ${HOST_BIN}/gttime"
echo "    gttx: ${HOST_BIN}/gttx"
echo "    gtwav: ${HOST_BIN}/gtwav"
echo "    gtfeed: ${HOST_BIN}/gtfeed"
echo "    gtcfg: ${HOST_BIN}/gtcfg"
echo "    gtdump: ${HOST_BIN}/gtdump"

echo "==> build.sh done."
