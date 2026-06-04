#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo "$(cd "$SCRIPT_DIR/../.." && pwd)")"
DPDK_BUILD_DIR="$PROJECT_ROOT/third_party/dpdk-24.11/build"

echo ">>> Verifying DPDK build..."

if [ -d "$DPDK_BUILD_DIR" ]; then
    echo " - [OK] DPDK build directory exists: $DPDK_BUILD_DIR"
    
    if ls "$DPDK_BUILD_DIR"/lib/x86_64-linux-gnu/librte_eal.* >/dev/null 2>&1 || [ -f "$DPDK_BUILD_DIR/lib/librte_eal.a" ] || [ -f "$DPDK_BUILD_DIR/lib/x86_64-linux-gnu/librte_eal.so" ]; then
        echo " - [OK] Core EAL library was successfully built."
    else
        echo " - [WARNING] EAL library not found. Build might be incomplete."
    fi
else
    echo " - [ERROR] DPDK build directory does not exist. Please run setup_dpdk.sh."
    exit 1
fi