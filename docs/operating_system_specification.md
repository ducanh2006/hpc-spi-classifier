# Operating System Specification
## Distribution
- **Name:** Ubuntu Desktop
- **Version:** 24.04.x LTS (Noble Numbat)
- **Architecture:** amd64 (x86_64)
- **Kernel Version:** Linux Kernel >= 6.8 (with HWE Stack for Intel Hybrid Architecture support)
- **Init System:** systemd
## Key features support to DPDK/Hyperscan project
- **Hugepages Support:** Built-in support via `/sys/kernel/mm/hugepages/` (Will allocate 2MB pages).
- **VFIO/IOMMU Enabled:** Required for secure DMA with VFIO drivers in DPDK.
- **Modern GCC Toolchain:** GCC 13.2+ available by default -> Supports C++17/C++20 and AVX2 intrinsics.
- **Up-to-date Libraries:** DPDK 24.11+ and Vectorscan (Hyperscan modern fork compatible with GCC 13+) via source build.
- **systemd Integration:** Can create custom services for auto-starting the DPI application at boot.