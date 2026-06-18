#!/bin/bash

# Get directories relative to the script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DATA_DIR="$PROJECT_ROOT/tests/data/pcap"
RESULTS_DIR="$PROJECT_ROOT/tests/results"
BUILD_DIR="$PROJECT_ROOT/build"
APP="$BUILD_DIR/spifast"

mkdir -p "$RESULTS_DIR"

if [ "$EUID" -ne 0 ]; then
  echo "[ERROR] Please run this benchmark script with sudo (e.g., sudo ./tests/run_project/run_benchmark.sh)"
  exit 1
fi

if [ ! -f "$APP" ]; then
  echo "[ERROR] Application not found at $APP. Please build the project first."
  exit 1
fi

BENCHMARK_TIME=${1:-20}

echo "===================================================="
echo "      SPIFast Automated Benchmark (${BENCHMARK_TIME}s per file)      "
echo "===================================================="

CSV_FILE="$RESULTS_DIR/benchmark_native_summary.csv"
echo "PCAP_File,Throughput_Mbps,Flow_Rate_pps,Master_Drop_Packets,Worker_Drop_Packets" > "$CSV_FILE"

# Copy ONLY required DPDK PMD drivers to a secure directory to bypass EAL 'world-writable' error and avoid duplicate loading
TMP_DRIVER_DIR="/opt/spifast_dpdk_drivers"
rm -rf "$TMP_DRIVER_DIR"
mkdir -p "$TMP_DRIVER_DIR"
chmod 755 "$TMP_DRIVER_DIR"
cp "$PROJECT_ROOT/third_party/dpdk-24.11/build/drivers/librte_net_pcap.so" "$TMP_DRIVER_DIR/" 2>/dev/null || true
cp "$PROJECT_ROOT/third_party/dpdk-24.11/build/drivers/librte_mempool_ring.so" "$TMP_DRIVER_DIR/" 2>/dev/null || true

for pcap in "$DATA_DIR"/*.pcap; do
    if [ ! -f "$pcap" ]; then
        echo "No .pcap files found in $DATA_DIR"
        break
    fi
    
    pcap_name=$(basename "$pcap")
    echo "-> Benchmarking $pcap_name for $BENCHMARK_TIME seconds..."
    
    LOG_FILE="$RESULTS_DIR/${pcap_name}_log.txt"
    export LD_LIBRARY_PATH="$PROJECT_ROOT/third_party/dpdk-24.11/build/lib:$PROJECT_ROOT/third_party/dpdk-24.11/build/drivers:$LD_LIBRARY_PATH"
    # Run application using timeout and send output to log file
    # Ensure Hugepages are active
    timeout --preserve-status $BENCHMARK_TIME $APP -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 --vdev "net_pcap0,rx_pcap=$pcap,infinite_rx=1" -- -r "$PROJECT_ROOT/spi_rules.conf" > "$LOG_FILE" 2>&1
    
    # Extract the last set of printed stats
    # Output format is:
    # Throughput: 123.45 Mbps | Flow Rate: 67890 pps
    # Master Rx: X pkts | Master Drop: Y pkts
    # Worker Rx: A pkts | Worker Drop: B pkts
    
    THROUGHPUT=$(grep "Throughput:" "$LOG_FILE" | awk '{sum+=$2; count++} END {if (count > 0) printf "%.2f", sum/count; else print "0.00"}')
    FLOW_RATE=$(grep "Flow Rate:" "$LOG_FILE" | awk '{sum+=$7; count++} END {if (count > 0) printf "%.0f", sum/count; else print "0"}')
    
    MASTER_DROP=$(grep "Master Drop:" "$LOG_FILE" | tail -n 1 | awk '{print $8}')
    WORKER_DROP=$(grep "Worker Drop:" "$LOG_FILE" | tail -n 1 | awk '{print $8}')
    
    # Fallback to 0 if empty
    THROUGHPUT=${THROUGHPUT:-0}
    FLOW_RATE=${FLOW_RATE:-0}
    MASTER_DROP=${MASTER_DROP:-0}
    WORKER_DROP=${WORKER_DROP:-0}
    
    echo "$pcap_name,$THROUGHPUT,$FLOW_RATE,$MASTER_DROP,$WORKER_DROP" >> "$CSV_FILE"
    
    echo "   [Result] Throughput: $THROUGHPUT Mbps | Flow Rate: $FLOW_RATE pps"
done

echo "===================================================="
echo "Benchmark complete. Results saved in:"
echo "  - Summary: $CSV_FILE"
echo "  - Detailed Logs: $RESULTS_DIR/*_log.txt"
echo "===================================================="
