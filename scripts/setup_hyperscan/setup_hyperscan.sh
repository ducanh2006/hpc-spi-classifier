#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo "$(cd "$SCRIPT_DIR/../.." && pwd)")"
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"

HS_VERSION="5.4.2"
HS_TAR="hyperscan-${HS_VERSION}.tar.gz"
HS_URL="https://github.com/intel/hyperscan/archive/refs/tags/v${HS_VERSION}.tar.gz"
HS_DIR="$THIRD_PARTY_DIR/hyperscan-${HS_VERSION}"

mkdir -p "$THIRD_PARTY_DIR"
cd "$THIRD_PARTY_DIR"

# Download Phase
if [ -d "$HS_DIR" ]; then
    echo ">>> Directory $HS_DIR already exists. Skipping download."
else
    if [ ! -f "$HS_TAR" ]; then
        echo ">>> Downloading Hyperscan ${HS_VERSION}..."
        wget "$HS_URL" -O "$HS_TAR"
    fi
    echo ">>> Extracting Hyperscan..."
    tar -xf "$HS_TAR"
fi

# Build Phase
cd "$HS_DIR"
mkdir -p build
cd build

if [ -f "lib/libhs.so" ] || [ -f "lib/libhs.so.5" ] || [ -f "lib/libhs.a" ]; then
    echo ">>> Hyperscan library already built. Skipping build phase."
else
    echo ">>> Configuring Hyperscan with CMake (Bypassing strict GCC 13 warnings)..."
    cmake -DCMAKE_CXX_FLAGS="-Wno-error" -DCMAKE_C_FLAGS="-Wno-error" -DBUILD_SHARED_LIBS=on ..
    
    # Calculate build threads: min(cores / 4, 4) with a minimum of 1
    THREADS=$(nproc | awk '{
        threads=int($1/4); 
        if(threads < 1) threads=1; 
        if(threads > 4) threads=4; 
        print threads
    }')
    
    echo ">>> Compiling Hyperscan with $THREADS threads..."
    make -j"$THREADS"
fi

echo ">>> Hyperscan setup completed."