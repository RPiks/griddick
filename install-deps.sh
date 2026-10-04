#!/usr/bin/env bash
# Install distro packages needed to build firmware + host tools + packages.
# Debian/Ubuntu and Fedora desktops. Does not clone pico-sdk (setup_env.sh).
# Packaging (griddick-dist/pack.sh): dpkg-deb and rpmbuild.
# Raspberry Pi OS / Raspbian: ./install-deps-raspbian.sh
#
#   ./install-deps.sh
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

dnf_install() {
    dnf install -y "$@"
}

# Package may be missing on older releases.
try_apt() {
    apt-get install -y --no-install-recommends "$@" || \
        echo "install-deps: optional package not available: $*" >&2
}

try_dnf() {
    dnf install -y "$@" || \
        echo "install-deps: optional package not available: $*" >&2
}

if [[ ! -f /etc/os-release ]]; then
    echo "install-deps: /etc/os-release missing" >&2
    exit 1
fi
# shellcheck disable=SC1091
. /etc/os-release

# Raspberry Pi OS Bookworm reports ID=debian. Classic images report raspbian.
# Neither should take the desktop package list.
if [[ "${ID}" == "raspbian" ]]; then
    echo "install-deps: Raspberry Pi OS — run ./install-deps-raspbian.sh" >&2
    exit 1
fi
if [[ -r /proc/device-tree/model ]] && tr -d '\0' < /proc/device-tree/model | grep -q 'Raspberry Pi'; then
    echo "install-deps: Raspberry Pi — run ./install-deps-raspbian.sh" >&2
    exit 1
fi

need_root "$@"

case "${ID}" in
    debian|ubuntu|linuxmint)
        FAMILY=debian
        ;;
    fedora)
        FAMILY=fedora
        ;;
    *)
        LIKE="${ID_LIKE:-}"
        if [[ " ${LIKE} " == *" debian "* ]]; then
            FAMILY=debian
        elif [[ " ${LIKE} " == *" fedora "* ]]; then
            FAMILY=fedora
        else
            echo "install-deps: unsupported distro '${ID}' (want Debian or Fedora)" >&2
            exit 1
        fi
        ;;
esac

echo "install-deps: ${ID} (${FAMILY})"

if [[ "${FAMILY}" == debian ]]; then
    # Pico SDK official apt set + host (gtstation Qt6, griddickd OpenSSL, flash.sh).
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
        netcat-openbsd \
        dpkg-dev \
        rpm \
        patchelf
    try_apt picotool
elif [[ "${FAMILY}" == fedora ]]; then
    dnf_install \
        gcc \
        gcc-c++ \
        make \
        cmake \
        ninja-build \
        pkgconf \
        python3 \
        git \
        arm-none-eabi-gcc-cs \
        arm-none-eabi-gcc-cs-c++ \
        arm-none-eabi-newlib \
        libusb1-devel \
        usbutils \
        openssl-devel \
        qt6-qtbase-devel \
        mesa-libGL-devel \
        nmap-ncat \
        rpm-build \
        rpmdevtools \
        dpkg \
        patchelf
    try_dnf arm-none-eabi-binutils-cs
    try_dnf arm-none-eabi-binutils
    try_dnf picotool
fi

echo "install-deps: done"
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
    echo "    dpkg-deb: missing (griddick-dist .deb)" >&2
fi
if command -v rpmbuild >/dev/null; then
    echo "    rpmbuild: $(command -v rpmbuild)"
else
    echo "    rpmbuild: missing (griddick-dist .rpm)" >&2
fi
