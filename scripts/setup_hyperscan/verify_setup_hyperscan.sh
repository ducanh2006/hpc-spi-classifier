#!/usr/bin/env bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo "$(cd "$SCRIPT_DIR/../.." && pwd)")"
HS_LIB_DIR="$PROJECT_ROOT/third_party/hyperscan-5.4.2/build/lib"
HS_INCLUDE_DIR="$PROJECT_ROOT/third_party/hyperscan-5.4.2/src"

echo ">>> Verifying Hyperscan build..."

if [ -f "$HS_LIB_DIR/libhs.so" ] || [ -f "$HS_LIB_DIR/libhs.so.5" ] || [ -f "$HS_LIB_DIR/libhs.a" ]; then
    echo " - [OK] Hyperscan library (libhs) was successfully built at: $HS_LIB_DIR"
else
    echo " - [ERROR] libhs not found. Please run setup_hyperscan.sh."
    exit 1
fi

if [ -f "$HS_INCLUDE_DIR/hs.h" ]; then
    echo " - [OK] Header file hs.h is available."
fi