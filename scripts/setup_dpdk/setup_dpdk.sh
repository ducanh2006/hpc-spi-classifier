#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo "$(cd "$SCRIPT_DIR/../.." && pwd)")"
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"

DPDK_VERSION="24.11"
DPDK_TAR="dpdk-${DPDK_VERSION}.tar.xz"
DPDK_URL="https://fast.dpdk.org/rel/${DPDK_TAR}"
DPDK_DIR="$THIRD_PARTY_DIR/dpdk-${DPDK_VERSION}"

mkdir -p "$THIRD_PARTY_DIR"
cd "$THIRD_PARTY_DIR"

# Download Phase
if [ -d "$DPDK_DIR" ]; then
    echo ">>> Directory $DPDK_DIR already exists. Skipping download."
else
    if [ ! -f "$DPDK_TAR" ]; then
        echo ">>> Downloading DPDK ${DPDK_VERSION}..."
        wget "$DPDK_URL"
    fi
    echo ">>> Extracting DPDK..."
    tar -xf "$DPDK_TAR"
fi

# Build Phase
cd "$DPDK_DIR"
if [ -d "build" ] && [ -f "build/build.ninja" ]; then
    echo ">>> Build directory already exists. Skipping configuration and build."
else
    echo ">>> Configuring DPDK build environment with Meson..."
    meson setup build
    echo ">>> Compiling DPDK with Ninja (this may take a few minutes)..."
    ninja -C build
fi

echo ">>> DPDK setup completed."