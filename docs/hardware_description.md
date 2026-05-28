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
*   **Type:** DDR5 SODIMM (Laptop Form Factor)
*   **Speed:** 4800 MT/s (MHz)
*   **Configuration:** 2 Slots Used (Dual Channel Mode Enabled)
*   **Total Capacity:** *(Derived from context, typically 16GB/32GB depending on stick size)*
*   **Hardware Reserved:** 264 MB (Allocated for BIOS/iGPU)
> **Note:** Dual Channel configuration is active, providing optimal memory bandwidth for high-throughput packet processing tasks.

## 3. Storage (System Disk)
*   **Model:** Samsung MZVL21T0HCLR-00B (PM9A1 Series)
*   **Interface:** NVMe PCIe Gen4 x4
*   **Capacity:** 1 TB (Physical: ~954 GB usable)
*   **Role:** System Drive & Page File Host

## 4. Network Interface (Wireless)
*   **Adapter:** Killer(R) Wi-Fi 6E AX1675i 160MHz (211NGW)
*   **Standard:** Wi-Fi 6E (802.11ax)
*   **Bandwidth:** Up to 160 MHz channel width

