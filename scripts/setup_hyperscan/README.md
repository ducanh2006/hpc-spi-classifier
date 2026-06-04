# Hyperscan Setup Guide

This directory contains utility scripts to download, compile, and verify **Intel Hyperscan 5.4.2**.

## Files

- **`setup_hyperscan.sh`**: Downloads the Hyperscan 5.4.2 source tarball, extracts it into `third_party/`, and compiles it locally using `cmake` and `make`. It automatically bypasses strict GCC 13 warnings (`-Wno-error`) to ensure a smooth build.
- **`verify_setup_hyperscan.sh`**: Inspects the `third_party/hyperscan-5.4.2/build/lib` directory to verify that `libhs.so` and `hs.h` headers were successfully generated.

## How to Use

### 1. Setup Hyperscan
Run the setup script to download and build Hyperscan:
```bash
chmod +x scripts/setup_hyperscan/setup_hyperscan.sh
./scripts/setup_hyperscan/setup_hyperscan.sh
```

### 2. Verify setup Hyperscan
Run verify script:
```bash
chmod +x scripts/setup_hyperscan/verify_setup_hyperscan.sh
./scripts/setup_hyperscan/verify_setup_hyperscan.sh
```