# Build Environment Specification
## 1. Language: Pure C11
*   **Standard:** ISO/IEC 9899:2011.
*   **Rationale:** Native ABI compatibility with DPDK v24.11 & Hyperscan v5.4.2; zero overhead (no RTTI/exceptions); enables `<stdatomic.h>` for lock-free stats and `restrict` pointers for SIMD optimization.
## 2. Build System: Meson + Ninja
*   **Workflow:** Unified `meson.build` configuration generating `build.ninja` for parallel execution.
*   **Dependencies:**
    *   **DPDK v24.11:** Built natively via Meson/Ninja.
    *   **Hyperscan v5.4.2:** Pre-compiled via CMake/Make (isolated build).
*   **Integration:** Meson resolves `libdpdk` (pkg-config) and `libhs` (`cc.find_library`) to link pre-built `.so` artifacts.
## 3. Toolchain: GCC 13.3 (Ubuntu 24.04 LTS)
*   **Flags:**
    *   `-std=gnu11`: C11 + Linux syscall extensions.
    *   `-O3`: Aggressive optimization (unrolling, inlining).
    *   `-march=native`: Targets i7-13700HX (Raptor Lake); enables AVX2, AVX-VNNI, FMA3 for Hyperscan SIMD.
    *   `-Wno-error`: Suppresses strict warnings during Hyperscan build (GCC 13 compatibility).