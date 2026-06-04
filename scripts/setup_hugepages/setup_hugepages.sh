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
HUGEPAGES_DIR="/mnt/huge"

echo "=================================================="
echo "🚀 CONFIGURING DPDK HUGEPAGES RESOURCE"
echo "=================================================="

# 1. Allocate 2MB Hugepages
echo "⚙️  Allocating ${HUGEPAGES_COUNT} pages of 2MB hugepages..."
sysctl -w vm.nr_hugepages=${HUGEPAGES_COUNT}

# 2. Check allocation status
allocated=$(cat /proc/meminfo | grep -i HugePages_Free | awk '{print $2}')
echo "📊 Current free hugepages: ${allocated} / ${HUGEPAGES_COUNT}"

# 3. Setup mount point if not already mounted
if ! mount | grep -q "hugetlbfs"; then
    echo "📂 Creating and mounting hugetlbfs at ${HUGEPAGES_DIR}..."
    mkdir -p "${HUGEPAGES_DIR}"
    mount -t hugetlbfs nodev "${HUGEPAGES_DIR}"
    echo "✅ Successfully mounted hugetlbfs."
else
    echo "✅ hugetlbfs is already mounted."
fi

echo "--------------------------------------------------"
echo "🎉 Hugepages configuration completed!"
echo "=================================================="
