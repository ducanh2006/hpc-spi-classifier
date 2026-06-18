#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

if [ "$EUID" -ne 0 ]; then
  echo "[ERROR] Please run this script with sudo"
  exit 1
fi

TMP_DRIVER_DIR="/opt/spifast_dpdk_drivers"
rm -rf "$TMP_DRIVER_DIR"
mkdir -p "$TMP_DRIVER_DIR"
chmod 755 "$TMP_DRIVER_DIR"
cp "$PROJECT_ROOT/third_party/dpdk-24.11/build/drivers/librte_net_pcap.so" "$TMP_DRIVER_DIR/" 2>/dev/null || true
cp "$PROJECT_ROOT/third_party/dpdk-24.11/build/drivers/librte_mempool_ring.so" "$TMP_DRIVER_DIR/" 2>/dev/null || true

export LD_LIBRARY_PATH="$PROJECT_ROOT/third_party/dpdk-24.11/build/lib:$PROJECT_ROOT/third_party/dpdk-24.11/build/drivers:$LD_LIBRARY_PATH"

echo "Generating functional test data..."
source "$PROJECT_ROOT/venv/bin/activate" 2>/dev/null || true
python "$PROJECT_ROOT/tests/gen_tests/gen_func_test.py"

echo "Compiling the debug executable..."
/usr/bin/meson compile -C "$PROJECT_ROOT/build" spifast_debug

echo "Running spifast_debug..."
"$PROJECT_ROOT/build/spifast_debug" -d "$TMP_DRIVER_DIR" -l 0-1 --vdev "net_pcap0,rx_pcap=$PROJECT_ROOT/tests/data/pcap/func_test.pcap" -- -r "$PROJECT_ROOT/spi_rules.conf"

echo "Evaluating results..."
python "$SCRIPT_DIR/check_correctness.py" "$PROJECT_ROOT/tests/data/csv/func_test_map.csv" "$PROJECT_ROOT/tests/results/actual.csv"
