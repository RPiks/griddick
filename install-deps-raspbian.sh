#!/usr/bin/env bash
# Raspberry Pi OS / Raspbian packages for firmware + host tools.
# Pi 4, 32- or 64-bit. Does not clone pico-sdk (setup_env.sh).
# Desktop Debian and Fedora: ./install-deps.sh
#
#   ./install-deps-raspbian.sh
set -euo pipefail

need_root() {
    if [[ "$(id -u)" -ne 0 ]]; then
        exec sudo -- "$0" "$@"
    fi
}

apt_install() {
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y --no-install-recommends "$@"
}

# Missing on some Pi images; do not fail the rest of the install.
try_apt() {
    apt-get install -y --no-install-recommends "$@" || \
        echo "install-deps-raspbian: optional package not available: $*" >&2
}

if [[ ! -f /etc/os-release ]]; then
    echo "install-deps-raspbian: /etc/os-release missing" >&2
    exit 1
fi
# shellcheck disable=SC1091
. /etc/os-release

on_pi=0
if [[ "${ID}" == "raspbian" ]]; then
    on_pi=1
elif [[ -r /proc/device-tree/model ]] && tr -d '\0' < /proc/device-tree/model | grep -q 'Raspberry Pi'; then
    on_pi=1
fi
if [[ "${on_pi}" -ne 1 ]]; then
    echo "install-deps-raspbian: not Raspberry Pi OS (ID=${ID}). Use ./install-deps.sh" >&2
    exit 1
fi

need_root "$@"

echo "install-deps-raspbian: ${ID} ($(uname -m))"

# Pico cross tools are the same apt names as Debian, but rpm is not part of
# the Pi image set and must not abort the install.
apt_install \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    python3 \
    git \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    libusb-1.0-0-dev \
    usbutils \
    libssl-dev \
    qt6-base-dev \
    libgl1-mesa-dev \
    dpkg-dev
try_apt netcat-openbsd
try_apt rpm
try_apt patchelf
try_apt picotool

echo "install-deps-raspbian: done"
echo "    cmake $(cmake --version | head -1)"
if command -v arm-none-eabi-gcc >/dev/null; then
    echo "    $(arm-none-eabi-gcc --version | head -1)"
else
    echo "    arm-none-eabi-gcc: missing" >&2
fi
if command -v picotool >/dev/null; then
    echo "    picotool: $(command -v picotool)"
else
    echo "    picotool: not installed (flash.sh can use tools/bin/picotool)"
fi
if command -v dpkg-deb >/dev/null; then
    echo "    dpkg-deb: $(command -v dpkg-deb)"
else
    echo "    dpkg-deb: missing" >&2
fi
if command -v nc >/dev/null; then
    echo "    nc: $(command -v nc)"
else
    echo "    nc: missing (griddickd chat scripts)" >&2
fi
