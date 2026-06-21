# SPIFast Benchmarking Guide

This directory contains the automation scripts to run the SPIFast DPDK application against the provided test data and collect performance metrics.

## 📂 Directory Structure

- **`../data/pcap/`**: Place your input `.pcap` files here. These are the network captures that will be cycled through by the benchmarking scripts.
- **`run_benchmark_native.sh`**: Runs benchmarks natively using DPDK's `infinite_rx=1` parameter. **Purpose**: Ideal for small PCAP files (under 8,192 packets). It loads all packets directly into DPDK's zero-copy memory pool (`mbuf_pool`) for ultra-low overhead benchmarking. Will crash on large PCAP files.
- **`run_benchmark_tcpreplay.sh`**: Runs benchmarks using `tcpreplay` and virtual network interfaces (`veth`). **Purpose**: Required for testing large PCAP files (e.g., 1,000,000+ packets). It streams packets into DPDK at line rate over a virtual interface, completely bypassing DPDK's memory pool size limits.
- **`run_get_raw_throughput.sh`**: Measures the maximum injection speed of `tcpreplay` over `veth`, independent of the DPDK application. **Purpose**: Establishes a baseline theoretical maximum throughput capacity of the system.
- **`run_check_correctness.sh`**: Fully automated functional testing script. **Purpose**: Generates deterministic test PCAPs using Python, runs the `spifast_debug` executable to log output, and verifies the packet actions against an expected baseline to ensure logical correctness.
- **`../results/`**: This directory will be automatically created. It stores the generated benchmark CSVs and raw console output logs.

## 🚀 How to Run the Tests

1. **Ensure the Project is Built**
   Make sure you have compiled the project successfully using Meson and Ninja. The executable `spifast` must exist in the `build/` directory at the project root.
   ```bash
   ninja -C build
   ```
   *Note: Every time you modify the `meson.build` file, you must clean, re-configure, and re-compile the project:*
   ```bash
   rm -rf build
   meson setup build
   ninja -C build
   ```

2. **Check Your Test Data**
   Ensure your `.pcap` files are inside the `tests/data/pcap/` folder.

3. **Make Scripts Executable**
   ```bash
   chmod +x tests/judge/*.sh
   ```

4. **Configure Hugepages (Required after every reboot)**
   DPDK requires Hugepages to allocate its zero-copy memory pool. This configuration is lost upon system reboot, so you must allocate them before running benchmarks.
   ```bash
   chmod +x scripts/setup_hugepages/*.sh
   sudo ./scripts/setup_hugepages/setup_hugepages.sh
   ```

5. **Execute the Benchmark Script**
   Because DPDK requires access to system Hugepages to allocate the zero-copy memory pool, the scripts **must be run with `sudo`**.
   ```bash
   # 🟢 Native Mode (Direct memory loop).
   # STRICTLY for small pcaps (<8K pkts). Delivers kịch trần line-rate performance by completely bypassing the Linux kernel, but will crash on large datasets due to DPDK mempool exhaustion:
   sudo ./tests/judge/run_benchmark_native.sh

   # 🔵 TCPReplay Mode (veth streaming, bypasses DPDK memory limits).
   # REQUIRED for large pcaps (1M+ pkts). Prevents memory crash but is strictly limited by the Linux kernel's veth stack overhead:
   sudo ./tests/judge/run_benchmark_tcpreplay.sh

   # 🟣 To verify functional correctness of the parsing and rules logic:
   sudo ./tests/judge/run_check_correctness.sh

   # 🟡 To measure the raw veth/tcpreplay environment capacity independently:
   sudo ./tests/judge/run_get_raw_throughput.sh
   ```

6. **Optional. Run the project directly**
```bash
#  Run native mode (read directly from PCAP in infinite loop)
sudo ./tests/judge/run_project_native.sh

#  Run tcpreplay mode (create veth and use tcpreplay to replay packets)
sudo ./tests/judge/run_project_tcpreplay.sh

```

7. **Change Rule**
```bash
sudo ./build/spi_cli reload_rules ./spi_rules.conf
```

## 📊 Viewing the Results

Once the script finishes executing, check the `tests/results/` directory:

- **`benchmark_native_summary.csv`** & **`benchmark_tcpreplay_summary.csv`**: Consolidated Excel-compatible CSV files detailing the `Avg Throughput (Mbps)`, `Avg Flow Rate (pps)`, and cumulative `Drop Packets` for every tested PCAP under the respective benchmarking modes.
- **`benchmark_raw_throughput_summary.csv`**: Baseline injection speed measurements showing the environment's theoretical maximum capacity.
- **`actual.csv`**: Functional testing results output by `spifast_debug`, mapped packet-by-packet to verify rules parsing.
- **`*_log.txt`**: Detailed, raw stdout logs from the DPDK application for each specific PCAP run. Use this to verify rule hit distributions and individual core behavior.
