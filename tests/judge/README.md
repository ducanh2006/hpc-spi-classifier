# SPIFast Benchmarking Guide

This directory contains the automation scripts to run the SPIFast DPDK application against the provided test data and collect performance metrics.

## 📂 Directory Structure

- **`../data/pcap/`**: Place your input `.pcap` files here. These are the network captures that will be cycled through by the benchmarking scripts.
- **`run_benchmark_native.sh`**: Runs benchmarks natively using DPDK's `infinite_rx=1` parameter. **Purpose**: Ideal for small PCAP files (under 8,192 packets). It loads all packets directly into DPDK's zero-copy memory pool (`mbuf_pool`) for ultra-low overhead benchmarking. Will crash on large PCAP files.
- **`run_benchmark_tcpreplay.sh`**: Runs benchmarks using `tcpreplay` and virtual network interfaces (`veth`). **Purpose**: Required for testing large PCAP files (e.g., 1,000,000+ packets). It streams packets into DPDK at line rate over a virtual interface, completely bypassing DPDK's memory pool size limits.
- **`../results/`**: This directory will be automatically created. It stores the generated benchmark CSVs and raw console output logs.

## 🚀 How to Run the Tests

1. **Ensure the Project is Built**
   Make sure you have compiled the project successfully using Meson and Ninja. The executable `spifast` must exist in the `build/` directory at the project root.
   ```bash
   ninja -C build
   ```

2. **Check Your Test Data**
   Ensure your `.pcap` files are inside the `tests/data/pcap/` folder.

3. **Make Scripts Executable**
   ```bash
   chmod +x tests/judge/*.sh
   ```

4. **Execute the Benchmark Script**
   Because DPDK requires access to system Hugepages to allocate the zero-copy memory pool, the scripts **must be run with `sudo`**.
   
   ```bash
   # For small pcaps (<8K packets):
   sudo ./tests/judge/run_benchmark_native.sh
   
   # For large pcaps (e.g., 1M+ packets):
   sudo ./tests/judge/run_benchmark_tcpreplay.sh
   ```

   By default, the scripts will run each PCAP for 20 seconds. You can override this by passing the desired number of seconds as an argument:
   ```bash
   # Run with default 20 seconds per file
   sudo ./tests/judge/run_benchmark_native.sh

   # Run with 30 seconds per file
   sudo ./tests/judge/run_benchmark_tcpreplay.sh 30
   ```

## 📊 Viewing the Results

Once the script finishes executing, check the `tests/results/` directory:

- **`benchmark_summary.csv`**: A consolidated Excel-compatible CSV file detailing the `Throughput (Mbps)`, `Flow Rate (pps)`, and `Drop Packets` for every tested PCAP.
- **`*_log.txt`**: Detailed, raw stdout logs from the DPDK application for each specific PCAP run. Use this to verify rule hit distributions and individual core behavior.
