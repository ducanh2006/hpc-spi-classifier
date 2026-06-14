#!/bin/bash

# Copy DPDK PMD drivers to secure directory to bypass EAL 'world-writable' error
TMP_DRIVER_DIR="/opt/spifast_dpdk_drivers"
mkdir -p "$TMP_DRIVER_DIR"
cp ./third_party/dpdk-24.11/build/drivers/librte_net_pcap.so "$TMP_DRIVER_DIR/" 2>/dev/null || true
cp ./third_party/dpdk-24.11/build/drivers/librte_mempool_ring.so "$TMP_DRIVER_DIR/" 2>/dev/null || true

export LD_LIBRARY_PATH="./third_party/dpdk-24.11/build/lib:./third_party/dpdk-24.11/build/drivers:$LD_LIBRARY_PATH"

PCAP_FILE=${1:-"./tests/data/pcap/tls13-rfc8446.pcap"}

./build/spifast -d "$TMP_DRIVER_DIR" -l 0-4 -n 4 \
  --vdev "net_pcap0,rx_pcap=$PCAP_FILE,infinite_rx=1" \
  -- -r "./spi_rules.conf"

