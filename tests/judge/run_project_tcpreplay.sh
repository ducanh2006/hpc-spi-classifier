#!/bin/bash

# Copy DPDK PMD drivers to secure directory to bypass EAL 'world-writable' error
TMP_DRIVER_DIR="/opt/spifast_dpdk_drivers"
mkdir -p "$TMP_DRIVER_DIR"
cp ./third_party/dpdk-24.11/build/drivers/librte_net_pcap.so "$TMP_DRIVER_DIR/" 2>/dev/null || true
cp ./third_party/dpdk-24.11/build/drivers/librte_mempool_ring.so "$TMP_DRIVER_DIR/" 2>/dev/null || true

# Set up veth pair
ip link add veth0 type veth peer name veth1 2>/dev/null || true
ip link set veth0 up
ip link set veth1 up

# Cleanup handler on exit
cleanup() {
  kill -9 $TCPREPLAY_PID 2>/dev/null
  ip link delete veth0 2>/dev/null
}
trap cleanup EXIT INT TERM
#PCAP_FILE=${1:-"./tests/data/pcap/tls13-rfc8446.pcap"}
PCAP_FILE=${1:-"./tests/data/pcap/balanced_traffic.pcap"}

# Run tcpreplay in background
tcpreplay -t --loop=0 -i veth1 "$PCAP_FILE" >/dev/null 2>&1 &
TCPREPLAY_PID=$!


export LD_LIBRARY_PATH="./third_party/dpdk-24.11/build/lib:./third_party/dpdk-24.11/build/drivers:$LD_LIBRARY_PATH"

./build/spifast -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 \
  --vdev "net_pcap0,rx_iface=veth0" \
  -- -r "./spi_rules.conf"
