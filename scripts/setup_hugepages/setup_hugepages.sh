#!/bin/bash
# ==============================================================================
# Purpose: Configure and allocate 2MB Hugepages for DPDK (2GB total)
# Usage:
#   1. Make execution:  chmod +x scripts/setup_hugepages/setup_hugepages.sh
#   2. Run from root:   sudo ./scripts/setup_hugepages/setup_hugepages.sh
# ==============================================================================

# Ensure the script is run as root
if [ "$EUID" -ne 0 ]; then
    echo "❌ Error: Please run as root (using sudo)." >&2
    exit 1
fi

set -euo pipefail

HUGEPAGES_COUNT=1024
# Đổi thành đường dẫn mặc định của Ubuntu để tránh xung đột
HUGEPAGES_DIR="/dev/hugepages" 

echo "=================================================="
echo "🚀 CONFIGURING DPDK HUGEPAGES RESOURCE"
echo "=================================================="

echo "⚙️  Allocating ${HUGEPAGES_COUNT} pages of 2MB hugepages..."
sysctl -w vm.nr_hugepages=${HUGEPAGES_COUNT}

echo "⏳ Waiting for Kernel to allocate memory..."
sleep 1

allocated=$(grep -i "HugePages_Total" /proc/meminfo | awk '{print $2}')
free_hp=$(grep -i "HugePages_Free" /proc/meminfo | awk '{print $2}')
echo "📊 Current allocated hugepages: ${allocated} / ${HUGEPAGES_COUNT}"
echo "📊 Current free hugepages:      ${free_hp} / ${HUGEPAGES_COUNT}"

if ! mount | grep -q "on ${HUGEPAGES_DIR} type hugetlbfs"; then
    echo "📂 Creating and mounting hugetlbfs at ${HUGEPAGES_DIR}..."
    mkdir -p "${HUGEPAGES_DIR}"
    mount -t hugetlbfs nodev "${HUGEPAGES_DIR}"
    echo "✅ Successfully mounted hugetlbfs."
else
    echo "✅ hugetlbfs is already mounted correctly at ${HUGEPAGES_DIR}."
fi

echo "--------------------------------------------------"
echo "🎉 Hugepages configuration completed!"
echo "=================================================="