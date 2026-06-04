# Dependencies Setup Guide

This directory contains utility scripts to install and verify **Build Dependencies** required for compiling DPDK and Hyperscan.

## Files

- **`install_dependencies.sh`**: Installs core build tools (`meson`, `ninja`, `cmake`, `ragel`) and low-level libraries (`libnuma`, `libpcap`, `libboost`).
- **`verify_dependencies.sh`**: Checks the system PATH and versions of all required build tools.

## How to Use

### 1. Install Dependencies
Run the installation script with root privileges (`sudo`):
```bash
chmod +x scripts/setup_dependencies/install_dependencies.sh
sudo ./scripts/setup_dependencies/install_dependencies.sh
```

### 2. Verify Dependencies
Run the verification script to ensure everything is installed correctly:
```bash
chmod +x scripts/setup_dependencies/verify_dependencies.sh
./scripts/setup_dependencies/verify_dependencies.sh
```

## Required Tools

| Tool         | Version         | Purpose                            |
|--------------|-----------------|------------------------------------|
| `gcc`        | 13.3.0          | C/C++ Compiler                     |
| `meson`      | 1.3.2       | Build System (DPDK)                |
| `ninja`      | 1.11.1       | Build System (DPDK)                |
| `cmake`      | 3.28.3       | Build System (Hyperscan)           |
| `ragel`      | 6.10          | Parser Generator (Hyperscan)       |
| `pkg-config` | 1.8.1       | Dependency Management (DPDK/Hyperscan) |
| `libnuma-dev` | < 2.0.18-1      | Memory Node Abstraction (DPDK)       |
| `libpcap-dev` | >= 1.10.0       | Packet Capture (DPDK)              |
| `libboost-all-dev` | >= 1.71 | Boost Libraries (DPDK)               |

## Notes
- These scripts are tested on **Ubuntu 22.04 LTS**.
- The installation requires root privileges (`sudo`).