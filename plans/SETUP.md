# Plan for Downloading and Installing DPDK and Hyperscan

This plan outlines the steps to verify dependencies, prepare the environment, download DPDK and Intel Hyperscan, and install them for our High-Performance SPI Message Classification System.

---

## 1. Pre-requisite Check Results
We checked the required packages on the system:
*   **`build-essential`**: **Installed** (GCC version 13.3.0).
*   **`meson`**: **Missing** (Required for DPDK build).
*   **`ninja-build`**: **Missing** (Required for DPDK build).
*   **`cmake`**: **Missing** (Required for Hyperscan build).
*   **`ragel`**: **Missing** (Required for compiling Hyperscan state machines).
*   **`pkg-config`**: **Missing** (Required for building with DPDK & Hyperscan).

---

## 2. Using Intel Hyperscan
As requested by project requirements, we will use **Intel Hyperscan** instead of the Vectorscan fork. 
*   **Version**: Hyperscan 5.4.2 (Latest official release from Intel).
*   **Note on Compiler Compatibility**: Since GCC 13.3.0 is a modern compiler, classic Hyperscan 5.4.2 may encounter strict compiler warnings/errors (e.g., `-Werror`, C++17 deprecations) during the build. We will handle these by configuring CMake with appropriate compiler flags (e.g., passing `-DCMAKE_CXX_FLAGS="-Wno-error"` or adjusting flags in the build script) to ensure a smooth build process.

---

## 3. Detailed Step-by-Step Plan

### Phase 1: Dependency Installation
Before starting any downloads or builds, we must install the build dependencies.
We will try to install:
```bash
sudo apt update
sudo apt install -y meson ninja-build cmake ragel pkg-config libnuma-dev libpcap-dev libboost-all-dev
```

### Phase 2: Source Code Acquisition
We will create a `third_party` directory in our workspace to keep the source files and builds isolated.

1.  **Create directory**:
    ```bash
    mkdir -p /media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party
    ```

2.  **Download DPDK**:
    *   **Version**: DPDK 24.11 (Latest LTS).
    *   **Source**: Tarball from fast.dpdk.org.
    *   **Commands**:
        ```bash
        cd /media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party
        wget https://fast.dpdk.org/rel/dpdk-24.11.tar.xz
        tar -xf dpdk-24.11.tar.xz
        ```

3.  **Download Intel Hyperscan**:
    *   **Version**: 5.4.2.
    *   **Source**: GitHub release tarball.
    *   **Commands**:
        ```bash
        cd /media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party
        wget https://github.com/intel/hyperscan/archive/refs/tags/v5.4.2.tar.gz -O hyperscan-5.4.2.tar.gz
        tar -xf hyperscan-5.4.2.tar.gz
        ```

4.  **Install Build Tools and Dependencies (Host Machine Mandatory)**:
    * **Description**: This step installs the core build automation tools (`meson`, `ninja`, `cmake`), state-machine compilers (`ragel`), and low-level development libraries (`libnuma`, `libpcap`) required to compile DPDK and Hyperscan.
    * **Security Notice**: This process requires root privileges (`sudo`). Because AI Agents operate in a restricted non-sudo environment for system safety, **you (User) must execute this command manually** in your local host terminal.
    * **Command**:
        ```bash
        sudo apt update && sudo apt install -y meson ninja-build cmake ragel pkg-config libnuma-dev libpcap-dev libboost-all-dev
        ```

5.  **Compile DPDK locally**:
        1. Navigate to the extracted DPDK directory:
           ```bash
           cd /media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/dpdk-24.11
           ```
        2. Configure the build environment using Meson:
           ```bash
           meson setup build
           ```
        3. Compile DPDK with Ninja:
           ```bash
           ninja -C build
           ```

6.  **Compile Intel Hyperscan locally**:
        1. Navigate to the extracted Hyperscan directory:
           ```bash
           cd /media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2
           ```
        2. Create and enter a dedicated build directory:
           ```bash
           mkdir -p build && cd build
           ```
        3. Configure the build with CMake. We disable strict warnings treated as errors using `-DCMAKE_CXX_FLAGS="-Wno-error"` and `-DCMAKE_C_FLAGS="-Wno-error"` to ensure GCC 13 compatibility:
           ```bash
           cmake -DCMAKE_CXX_FLAGS="-Wno-error" -DCMAKE_C_FLAGS="-Wno-error" -DBUILD_SHARED_LIBS=on ..
           ```
        4. Compile Hyperscan (Using conservative min(core/4, 4) threads):
           ```bash
           make -j$(nproc | awk '{threads=$1/4; printf "%d\n", (threads < 4 ? threads : 4)}')
           ```

