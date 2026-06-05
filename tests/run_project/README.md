# SPIFast Benchmarking Guide

This directory contains the automation scripts to run the SPIFast DPDK application against the provided test data and collect performance metrics.

## 📂 Directory Structure

- **`../data/`**: Place your input `.pcap` files here. These are the network captures that will be cyclically replayed by the DPDK PCAP Virtual Device.
- **`run_benchmark.sh`**: The automated bash script that executes the tests.
- **`../results/`**: This directory will be automatically created. It stores the generated benchmark CSVs and raw console output logs.

## 🚀 How to Run the Tests

1. **Ensure the Project is Built**
   Make sure you have compiled the project successfully using Meson and Ninja. The executable `spifast` must exist in the `build/` directory at the project root.

2. **Check Your Test Data**
   Ensure your `.pcap` files (e.g., `http.pcap`, `tls13-rfc8446.pcap`) are inside the `tests/data/` folder.

3. **Execute the Benchmark Script**
   Because DPDK requires access to system Hugepages to allocate the zero-copy memory pool, the script **must be run with `sudo`**.

   Navigate to the project root directory and run:
   ```bash
   sudo ./tests/run_project/run_benchmark.sh
   ```

## 📊 Viewing the Results

Once the script finishes executing, check the `tests/results/` directory:

- **`benchmark_summary.csv`**: A consolidated Excel-compatible CSV file detailing the `Throughput (Mbps)`, `Flow Rate (pps)`, and `Drop Packets` for every tested PCAP.
- **`*_log.txt`**: Detailed, raw stdout logs from the DPDK application for each specific PCAP run. Use this to verify rule hit distributions and individual core behavior.
