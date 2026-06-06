# System Hardware Specifications
## 1. Processor (CPU)
### Basic Specifications
* **Model:** Intel Core i7-13700HX (13th Gen)
* **Code Name:** Raptor Lake
* **Technology:** Intel 7 (10 nm)
* **Socket:** LGA1700
* **Topology:** 16 Cores (8 Performance-cores + 8 Efficient-cores) / 24 Threads
* **TDP:** 55 W (Base) | 157 W (Max Turbo)
* **Instruction Sets:** MMX, SSE (1–4.2), SSSE3, EM64T, AES-NI, AVX, AVX2, AVX-VNNI, FMA3, SHA
### Clock Frequencies
* **Max Turbo Frequency (P-Cores):** 5.00 GHz
* **Max Turbo Frequency (E-Cores):** 3.70 GHz
* **Base Clock (Bus Speed):** ~100 MHz *(System reference heartbeat for TSC/cycles measurement)*
### Cache Hierarchy
* **L1 Data Cache:** $8 \times 48\text{ KB}$ (P-Core) + $8 \times 32\text{ KB}$ (E-Core)
* **L1 Instruction Cache:** $8 \times 32\text{ KB}$ (P-Core) + $8 \times 64\text{ KB}$ (E-Core)
* **Level 2 (L2) Cache:** $8 \times 1.25\text{ MB}$ (P-Core) + $2 \times 2\text{ MB}$ (E-Core Cluster)
* **Level 3 (L3) Cache:** 30 MB (Shared Intel® Smart Cache)

## 2. Memory (RAM)
* **Capacity**: 16 GB
* **Type:** DDR5 SODIMM (Laptop Form Factor)
* **Speed:** 4800 MT/s (MHz)
* **Configuration:** 2 Slots Used (Dual Channel Mode Enabled)
* **Total Capacity:** *(Derived from context, typically 16GB/32GB depending on stick size)*
* **Hardware Reserved:** 264 MB (Allocated for BIOS/iGPU)
* **Verification Command:** `sudo dmidecode -t memory | grep Speed`
> **Note:** Dual Channel configuration is active, providing optimal memory bandwidth for high-throughput packet processing tasks.

## 3. Storage (System Disk)
* **Model:** Samsung MZVL21T0HCLR-00B (PM9A1 Series)
* **Interface:** NVMe PCIe Gen4 x4
* **Capacity:** 1 TB (Physical: ~954 GB usable)
* **Role:** System Drive & Page File Host

### Hardware Identification
* **Device Name:** `nvme0n1`
* **Exact Model String:** `SAMSUNG MZVL21T0HCLR-00B07`
* **Command Used:** `lsblk -d -o NAME,ROTA,TRAN,MODEL`

### Performance Benchmarks (fio)
* **Max Sequential Read (Peak Performance):** ~9048 MB/s (8629 MiB/s)
    * *Command Used:* `sudo fio --name=max_read --ioengine=io_uring --rw=read --bs=1m --direct=1 --size=4g --numjobs=4 --iodepth=32 --runtime=15 --time_based --group_reporting`
* **Max Sequential Write (Peak Performance):** ~2441 MB/s (2328 MiB/s)
    * *Command Used:* `sudo fio --name=max_write --ioengine=io_uring --rw=write --bs=1m --direct=1 --size=4g --numjobs=4 --iodepth=32 --runtime=15 --time_based --group_reporting`
* **Mixed Sequential (Read/Write 50/50 - 1 Job / Queue Depth 1):** ~1196 MB/s Read / ~1189 MB/s Write
    * *Command Used:* `fio --name=seq_test --ioengine=libaio --rw=rw --bs=1m --direct=1 --size=2g --numjobs=1 --runtime=10 --time_based --output-format=normal`

## 4. Network Interface (Wireless)
* **Adapter:** Killer(R) Wi-Fi 6E AX1675i 160MHz (211NGW)
* **Standard:** Wi-Fi 6E (802.11ax)
* **Bandwidth:** Up to 160 MHz channel width