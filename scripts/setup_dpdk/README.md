# DPDK Setup Guide

This directory contains utility scripts to download, compile, and verify **DPDK (Data Plane Development Kit) 24.11 LTS**.

## Files

- **`setup_dpdk.sh`**: Downloads the DPDK 24.11 source tarball, extracts it into `third_party/`, and compiles it locally using `meson` and `ninja`.
- **`verify_setup_dpdk.sh`**: Inspects the `third_party/dpdk-24.11/build` directory to verify that core EAL libraries were successfully generated.

## How to Use

### 1. Setup DPDK
Run the setup script to download and build DPDK:
```bash
chmod +x scripts/setup_dpdk/setup_dpdk.sh
./scripts/setup_dpdk/setup_dpdk.sh
```

### 2. Verify setup DPDK
Run verify script: 
```bash
chmod +x scripts/setup_dpdk/verify_setup_dpdk.sh
./scripts/setup_dpdk/verify_setup_dpdk.sh
```