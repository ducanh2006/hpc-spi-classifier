# SPIFast - High-Performance DPDK Packet Classifier

SPIFast is a high-performance Shallow Packet Inspection (SPI) system built with C11 and DPDK. It classifies network traffic based on 5-tuple header fields (Source IP, Destination IP, Source Port, Destination Port, Protocol) at Gigabit line-rates using a lock-free multi-core pipeline architecture.

## 🚀 Features
- **Zero-Copy Architecture:** Parses packets strictly in-place using DPDK memory pools.
- **Lock-Free Multi-Threading:** Master (Rx/Dispatch) and Worker cores communicate via `rte_ring`.
- **Dynamic Load Balancing:** Software RSS (Jenkins Hash) ensures deterministic flow affinity.
- **Hot-Reloadable Rules:** Update classification rules in real-time without restarting the application via Unix Domain Sockets.

---

## 🛠️ System Requirements
- Linux OS (Ubuntu 20.04/22.04/24.04 recommended)
- GCC, Make, Python 3, `tcpreplay`
- Meson (>= 1.3.2) and Ninja
- DPDK 24.11 (installed locally via provided script)

---

## ⚙️ 1. Project Setup & Installation

Follow these steps exactly to set up the environment, compile DPDK, and allocate Hugepages.

### Step 1.1: Install Dependencies
```bash
sudo apt update
sudo apt install -y build-essential python3 python3-pip python3-venv tcpreplay pkg-config cmake
pip3 install meson ninja
```

### Step 1.2: Set Up Python Virtual Environment (For Test Generators)
```bash
python3 -m venv venv
source venv/bin/activate
pip install scapy
```

### Step 1.3: Download and Build DPDK
The project includes an automated script to download and compile DPDK locally inside the `third_party/` directory.
```bash
chmod +x scripts/setup_dpdk/setup_dpdk.sh
./scripts/setup_dpdk/setup_dpdk.sh
```

### Step 1.4: Allocate Hugepages (Required)
DPDK requires Hugepages for its zero-copy memory pool. **You must run this after every system reboot.**
```bash
chmod +x scripts/setup_hugepages/*.sh
sudo ./scripts/setup_hugepages/setup_hugepages.sh
```

---

## 🏗️ 2. Compiling the Project

Configure the build directory using `meson` and point it to the locally compiled DPDK using `PKG_CONFIG_PATH`.

```bash
# Export the PKG_CONFIG_PATH so meson finds our local DPDK
export PKG_CONFIG_PATH="$PWD/third_party/dpdk-24.11/build/meson-uninstalled"

# Setup the build directory
meson setup build

# Compile the project
ninja -C build
```
This will compile three executables inside `build/`:
- `spifast`: The highly optimized production SPI application.
- `spifast_debug`: A version compiled with debug flags for functional testing.
- `spi_cli`: The command-line tool for live configuration reloading.

---

## 🏃 3. Running the Project

### Running in Production/Native Mode
Run the application directly using the testing scripts. The scripts automatically handle DPDK EAL arguments and bind the PCAP `vdev` properly.
```bash
sudo ./tests/judge/run_project_native.sh
```

### Running with `tcpreplay` (For large PCAPs)
```bash
sudo ./tests/judge/run_project_tcpreplay.sh
```

### Hot-Reloading Rules
While the `spifast` application is running, open a new terminal window. You can edit `spi_rules.conf` and inject the new rules in real-time without dropping packets:
```bash
sudo ./build/spi_cli reload_rules ./spi_rules.conf
```

---

## 🧪 4. Benchmarking & Correctness Testing

The `tests/judge/` directory contains automated test suites. 
*Note: Make sure your Python `venv` is activated before running the correctness test, as it relies on `scapy` to generate test data.*

### Verify Functional Correctness
This generates 240 deterministic packets spanning all edge cases, runs them through the SPI pipeline, and compares the results against expected output.
```bash
# Ensure venv is active first!
source venv/bin/activate
sudo ./tests/judge/run_check_correctness.sh
```

### Run Performance Benchmarks
We offer two benchmark modes depending on your PCAP sizes:

```bash
# 1. Native Mode (For small PCAPs < 8K packets)
# Highest performance, bypasses Linux kernel entirely.
sudo ./tests/judge/run_benchmark_native.sh

# 2. TCPReplay Mode (For large PCAPs 1M+ packets)
# Streams via virtual ethernet (veth). Avoids DPDK memory pool exhaustion.
sudo ./tests/judge/run_benchmark_tcpreplay.sh
```

### Results Output
All logs and benchmarking results are automatically exported to the `tests/results/` directory as `.csv` and `_log.txt` files.

For full details on the test framework, refer to the [Benchmarking Guide](tests/judge/README.md).
