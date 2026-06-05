#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}====================================================${NC}"
echo -e "${YELLOW}   ENVIRONMENT VALIDATION SYSTEM   ${NC}"
echo -e "${YELLOW}====================================================${NC}"

if [ "$EUID" -ne 0 ]; then
  echo -e "${RED}[ERROR] Please run this script with root privileges (sudo).${NC}"
  exit 1
fi

check_cmd() {
    command -v "$1" >/dev/null 2>&1
}

echo -e "\n${YELLOW}[1/5] HARDWARE & OS CHECK${NC}"
arch=$(uname -m)
echo -e " - CPU Architecture: $arch"
if [ "$arch" != "x86_64" ]; then
    echo -e "${RED}   [WARNING] Hyperscan and DPDK perform best on x86_64.${NC}"
else
    echo -e "${GREEN}   [OK] Valid architecture.${NC}"
fi

echo -n " - Vector SIMD Support (AVX2/AVX512): "
if grep -q avx2 /proc/cpuinfo; then
    if grep -q avx512 /proc/cpuinfo; then
        echo -e "${GREEN}AVX512 Found (Optimal performance)${NC}"
    else
        echo -e "${GREEN}AVX2 Found (Good performance)${NC}"
    fi
else
    echo -e "${YELLOW}AVX2/AVX512 not found. Fallback mode will be used.${NC}"
fi

cores=$(nproc)
echo -e " - Available CPU Cores: $cores"
if [ "$cores" -lt 4 ]; then
    echo -e "${YELLOW}   [WARNING] Low core count (< 4 cores). Performance may be limited.${NC}"
fi

echo -e "\n${YELLOW}[2/5] KERNEL CONFIGURATION & HUGEPAGES${NC}"
hp_total=$(cat /proc/sys/vm/nr_hugepages)
hp_size=$(grep Hugepagesize /proc/meminfo | awk '{print $2 " " $3}')
echo -e " - Default Hugepage Size: $hp_size"
echo -e " - Total Active Hugepages: $hp_total"

if [ "$hp_total" -eq 0 ]; then
    echo -e "${RED}   [CRITICAL] Hugepages not configured! DPDK will fail to initialize.${NC}"
else
    echo -e "${GREEN}   [OK] Hugepages activated.${NC}"
fi

if [ -d "/sys/kernel/iommu_groups" ] && [ "$(ls -A /sys/kernel/iommu_groups)" ]; then
    echo -e "${GREEN} - IOMMU: Enabled (Ready for VFIO-PCI)${NC}"
else
    echo -e "${YELLOW} - IOMMU: Disabled or not supported.${NC}"
fi

echo -e "\n${YELLOW}[3/5] BUILD TOOLS CHECK${NC}"
tools=("gcc" "g++" "make" "meson" "ninja" "pkg-config")
for tool in "${tools[@]}"; do
    if check_cmd "$tool"; then
        ver=$($tool --version 2>&1 | head -n 1)
        echo -e " - ${GREEN}[INSTALLED]${NC} $tool: $ver"
    else
        echo -e " - ${RED}[MISSING]${NC} $tool is not installed."
    fi
done

echo -e "\n${YELLOW}[4/5] CORE DEPENDENCIES${NC}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || echo "$(cd "$SCRIPT_DIR/.." && pwd)")"

LOCAL_DPDK_PC="$PROJECT_ROOT/third_party/dpdk-24.11/build/meson-uninstalled"
if [ -d "$LOCAL_DPDK_PC" ]; then
    echo -e " - ${GREEN}[INFO] Local DPDK build detected at third_party/dpdk-24.11${NC}"
    export PKG_CONFIG_PATH="$LOCAL_DPDK_PC:$PKG_CONFIG_PATH"
fi

LOCAL_HS_BUILD="$PROJECT_ROOT/third_party/hyperscan-5.4.2/build"
LOCAL_HS_SRC="$PROJECT_ROOT/third_party/hyperscan-5.4.2/src"
if [ -d "$LOCAL_HS_BUILD" ]; then
    export PKG_CONFIG_PATH="$LOCAL_HS_BUILD:$LOCAL_HS_BUILD/lib/pkgconfig:$PKG_CONFIG_PATH"
fi

if check_cmd pkg-config; then
    if pkg-config --exists libdpdk; then
        dpdk_ver=$(pkg-config --modversion libdpdk)
        echo -e " - DPDK Library: ${GREEN}Found (Version: $dpdk_ver)${NC}"
    else
        echo -e " - DPDK Library: ${RED}NOT found via pkg-config.${NC}"
    fi
    
    if pkg-config --exists libhs; then
        hs_ver=$(pkg-config --modversion libhs)
        echo -e " - Hyperscan Library: ${GREEN}Found via pkg-config (Version: $hs_ver)${NC}"
    else
        if [ -f "$LOCAL_HS_SRC/hs.h" ] && [ -d "$LOCAL_HS_BUILD/lib" ]; then
            echo -e " - Hyperscan Library: ${GREEN}Found local build at third_party/hyperscan-5.4.2${NC}"
        elif [ -f "/usr/local/include/hs/hs.h" ] || [ -f "/usr/include/hs/hs.h" ]; then
            echo -e " - Hyperscan Library: ${GREEN}Found system headers (hs.h)${NC}"
        else
            echo -e " - Hyperscan Library: ${RED}NOT found. Please check third_party directory.${NC}"
        fi
    fi
else
    echo -e "${RED}[ERROR] Cannot verify libraries due to missing pkg-config.${NC}"
fi

if [ -f "/usr/include/pcap.h" ] || [ -f "/usr/include/x86_64-linux-gnu/pcap.h" ]; then
    echo -e " - Libpcap Library: ${GREEN}Installed (PCAP PMD Ready)${NC}"
else
    echo -e " - Libpcap Library: ${RED}NOT installed! libpcap-dev is required for PCAP mode.${NC}"
fi

echo -e "\n${YELLOW}[5/5] NETWORK INTERFACES & DRIVERS${NC}"
echo -e " ${GREEN}[INFO] This project uses virtual device (PCAP PMD). Physical NIC setup is NOT required.${NC}"
echo -e " - Kernel Modules Status (Reference Only):"
drivers=("vfio-pci" "uio_pci_generic" "igb_uio")
for drv in "${drivers[@]}"; do
    if lsmod | grep -q "$drv"; then
        echo -e "   + $drv: ${GREEN}LOADED${NC}"
    else
        echo -e "   + $drv: ${NC}Not loaded (Safe to ignore)"
    fi
done

echo -e "\n${YELLOW}====================================================${NC}"
echo -e "${YELLOW}               VALIDATION COMPLETE                  ${NC}"
echo -e "${YELLOW}====================================================${NC}"