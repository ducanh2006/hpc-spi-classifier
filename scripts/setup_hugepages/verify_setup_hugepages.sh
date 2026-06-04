#!/bin/bash
# ==============================================================================
# Purpose: Verify DPDK Hugepages allocation and mount status
# Usage:
#   1. Make execution:  chmod +x scripts/setup_hugepages/verify_setup_hugepages.sh
#   2. Run from root:   ./scripts/setup_hugepages/verify_setup_hugepages.sh
# ==============================================================================

set -euo pipefail

echo "=================================================="
echo "🔍 VERIFYING HUGEPAGES CONFIGURATION"
echo "=================================================="

# Check meminfo for hugepages configuration
echo "📊 Hugepages Memory Info:"
grep -i -E "HugePages_Total|HugePages_Free|Hugepagesize" /proc/meminfo || true

# Check mount point
echo -e "\n📂 Checking hugetlbfs mount point..."
if mount | grep -q "hugetlbfs"; then
    mount | grep "hugetlbfs"
    echo "✅ hugetlbfs is active and mounted correctly."
else
    echo "❌ Error: hugetlbfs is NOT mounted!"
fi
echo "=================================================="
