#!/usr/bin/env bash
# Fetch the Pico SDK and build picotool. No root. Idempotent.
# Called by install-deps.sh. build.sh expects the result.
#
#   third_party/pico-sdk
#   third_party/picotool
#   tools/bin/picotool
#   pico_sdk_import.cmake
#
# Override: PICO_SDK_REF (default 2.3.1), PICOTOOL_REF (default 2.3.1)
# picotool is configured with CMAKE_POLICY_VERSION_MINIMUM=3.5 so an older mbedtls still configures on CMake 4.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_REF="${PICO_SDK_REF:-2.3.1}"
TOOL_REF="${PICOTOOL_REF:-2.3.1}"
SDK="${ROOT}/third_party/pico-sdk"
PICO_SRC="${ROOT}/third_party/picotool"
TOOLS="${ROOT}/tools"

die() { echo "setup_env: $*" >&2; exit 1; }

command -v git >/dev/null || die "git missing"
command -v cmake >/dev/null || die "cmake missing"
command -v arm-none-eabi-gcc >/dev/null || die "arm-none-eabi-gcc missing"

mkdir -p "${ROOT}/third_party" "${TOOLS}"

# Firmware links pico_usb_reset (SDK 2.3.0+). A 2.1.1 tree configures and
# then fails in usb_descriptors.c. Replace it.
if [[ ! -f "${SDK}/pico_sdk_init.cmake" || ! -f "${SDK}/src/rp2_common/pico_usb_reset/include/pico/usb_reset.h" ]]; then
    echo "setup_env: clone pico-sdk ${SDK_REF}"
    rm -rf "${SDK}"
    git clone --depth 1 --branch "${SDK_REF}" \
        https://github.com/raspberrypi/pico-sdk.git "${SDK}"
fi
echo "setup_env: pico-sdk submodules"
git -C "${SDK}" submodule update --init

if [[ ! -f "${ROOT}/pico_sdk_import.cmake" ]]; then
    cp "${SDK}/external/pico_sdk_import.cmake" "${ROOT}/pico_sdk_import.cmake"
    echo "setup_env: wrote ${ROOT}/pico_sdk_import.cmake"
fi

if [[ ! -x "${TOOLS}/bin/picotool" ]]; then
    if [[ ! -f "${PICO_SRC}/CMakeLists.txt" ]]; then
        echo "setup_env: clone picotool ${TOOL_REF}"
        git clone --depth 1 --branch "${TOOL_REF}" \
            https://github.com/raspberrypi/picotool.git "${PICO_SRC}"
    fi
    echo "setup_env: build picotool"
    rm -rf "${PICO_SRC}/build"
    cmake -S "${PICO_SRC}" -B "${PICO_SRC}/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPICO_SDK_PATH="${SDK}" \
        -DCMAKE_INSTALL_PREFIX="${TOOLS}" \
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
        -Wno-dev
    cmake --build "${PICO_SRC}/build" -j"$(nproc)"
    cmake --install "${PICO_SRC}/build"
fi

[[ -f "${SDK}/pico_sdk_init.cmake" ]] || die "pico-sdk missing"
[[ -f "${ROOT}/pico_sdk_import.cmake" ]] || die "pico_sdk_import.cmake missing"
[[ -x "${TOOLS}/bin/picotool" ]] || die "tools/bin/picotool missing"
echo "setup_env: ${SDK}"
echo "setup_env: ${TOOLS}/bin/picotool"
