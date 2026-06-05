# Environment Validation Guide

## Overview

The `test_setup.sh` script performs an automated, end-to-end audit of your hardware configuration, kernel runtime parameters, build tools, and local/system dependencies to ensure maximum high-performance capability.

### Monitored Subsystems

1. **Hardware & OS Alignment**: Verifies `x86_64` architecture, CPU core counts ($\ge 4$ cores required for multi-thread pipelining), and hardware-level SIMD instruction support (`AVX2`/`AVX512`).
2. **Memory & Isolation**: Checks active Hugepages state and IOMMU groups.
3. **Build Toolchain**: Validates existence of correct compilers and generation tools (`gcc`, `g++`, `make`, `meson`, `ninja`, `pkg-config`).
4. **Linker Dependencies**: Dynamically maps and tests connectivity for local `third_party/dpdk-24.11` builds, `third_party/hyperscan-5.4.2` engines, and system-wide `libpcap-dev`.
5. **Drivers & Virtual PMD**: Confirms environment status while validating that the system is properly tailored for offline, file-backed simulation (`net_pcap` virtual poll-mode driver).

## How to Use

### Run Environment Validation

The script interfaces directly with lower-level kernel endpoints and local environment properties; therefore, it **must** be executed with root privileges (`sudo`):

```bash
chmod +x scripts/test_setup/test_setup.sh
sudo ./scripts/test_setup/test_setup.sh
```