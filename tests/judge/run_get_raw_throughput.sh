#!/bin/bash
set -euo pipefail

# Get directories relative to the script location
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DATA_DIR="$PROJECT_ROOT/tests/data/pcap"
RESULTS_DIR="$PROJECT_ROOT/tests/results"

mkdir -p "$RESULTS_DIR"

if [ "$EUID" -ne 0 ]; then
  echo "[ERROR] Please run this benchmark script with sudo"
  exit 1
fi

BENCHMARK_TIME=${1:-20}

echo "================================================================================"
echo "      Testing Maximum tcpreplay Injection Speed (${BENCHMARK_TIME}s per file)      "
echo "      (Independent of /src - purely measuring veth capacity)                    "
echo "================================================================================"

CSV_FILE="$RESULTS_DIR/benchmark_raw_throughput_summary.csv"
echo "PCAP_File,Throughput_Mbps,Flow_Rate_pps,Master_Drop_Packets,Worker_Drop_Packets" > "$CSV_FILE"

for pcap in "$DATA_DIR"/*.pcap; do
    if [ ! -f "$pcap" ]; then
        echo "No .pcap files found in $DATA_DIR"
        break
    fi
    
    pcap_name=$(basename "$pcap")
    echo "-> Benchmarking injection speed for $pcap_name for $BENCHMARK_TIME seconds..."
    
    # Set up veth pair for tcpreplay
    ip link add veth0 type veth peer name veth1 2>/dev/null || true
    ip link set veth0 up
    ip link set veth1 up
    
    # Run tcpreplay in the background, looping infinitely at top speed
    tcpreplay -t --loop=0 -i veth1 "$pcap" > /dev/null 2>&1 &
    TCPREPLAY_PID=$!
    
    SUM_THROUGHPUT=0
    SUM_FLOW_RATE=0
    
    # Wait a bit for tcpreplay to ramp up
    sleep 1
    
    # Monitor rx_packets and rx_bytes on veth0 every second
    for ((i=0; i<BENCHMARK_TIME; i++)); do
        rx_pkts_1=$(cat /sys/class/net/veth0/statistics/rx_packets)
        rx_bytes_1=$(cat /sys/class/net/veth0/statistics/rx_bytes)
        
        sleep 1
        
        rx_pkts_2=$(cat /sys/class/net/veth0/statistics/rx_packets)
        rx_bytes_2=$(cat /sys/class/net/veth0/statistics/rx_bytes)
        
        pkts_per_sec=$((rx_pkts_2 - rx_pkts_1))
        bytes_per_sec=$((rx_bytes_2 - rx_bytes_1))
        
        # Adjust bytes to include Ethernet overhead (24 bytes: 8 preamble + 12 IFG + 4 FCS)
        wire_bytes_per_sec=$((bytes_per_sec + pkts_per_sec * 24))
        
        # Calculate Mbps
        mbps=$(echo "scale=2; $wire_bytes_per_sec * 8 / 1000000" | bc -l)
        
        SUM_THROUGHPUT=$(echo "$SUM_THROUGHPUT + $mbps" | bc -l)
        SUM_FLOW_RATE=$((SUM_FLOW_RATE + pkts_per_sec))
    done
    
    # Calculate averages
    AVG_THROUGHPUT=$(echo "scale=2; $SUM_THROUGHPUT / $BENCHMARK_TIME" | bc -l)
    AVG_FLOW_RATE=$((SUM_FLOW_RATE / BENCHMARK_TIME))
    
    # Cleanup veth and tcpreplay
    kill -9 $TCPREPLAY_PID 2>/dev/null || true
    ip link delete veth0 2>/dev/null || true
    
    echo "$pcap_name,$AVG_THROUGHPUT,$AVG_FLOW_RATE,N/A,N/A" >> "$CSV_FILE"
    
    echo "   [Result] Avg Throughput: $AVG_THROUGHPUT Mbps | Avg Flow Rate: $AVG_FLOW_RATE pps"
done

echo "================================================================================"
echo "Benchmark complete. Results saved in:"
echo "  - Summary: $CSV_FILE"
echo "================================================================================"
