# SPIFast Implementation Plan

This document outlines the architecture, components, and implementation strategy for the SPIFast High-Performance Shallow Packet Inspection system using DPDK.

## User Review Required

> [!IMPORTANT]
> The dynamic load balancing approach will use **Software RSS Hashing (Flow Affinity)**. The Master core will extract the 5-tuple and use a fast hash (`rte_jhash`) modulo the number of workers to determine the target ring, preserving CPU L1/L2 cache hits for packets in the same session.
> The build system will be **Meson/Ninja** as per DPDK standard.
> The configuration will be loaded from a new `spi_rules.conf` file as per the latest feedback.

## Open Questions

> [!NOTE]
> All open questions have been resolved. I will use standard Python scripts to generate synthetic PCAPs and produce CSV output that can be easily imported to Excel for the test cases.

## Proposed Changes

### Build System and Configuration
The project will be built using Meson and Ninja.

#### [NEW] [meson.build](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/meson.build)
Configuration for compiling the DPDK application, linking against `libdpdk`.

#### [NEW] [spi_rules.conf](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/spi_rules.conf)
A sample configuration file containing SPI rules matching the specification.

---

### Core Data Structures
Shared definitions across the application.

#### [NEW] [src/common.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h)
Defines `five_tuple_t`, `spi_rule_t`, and action enumerations. Also defines shared macros like `unlikely` / `likely` for branch prediction.

#### [NEW] [src/stats.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/stats.h) / [src/stats.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/stats.c)
Thread-safe, lock-free statistical counters for packets received, dropped, matched, and per-rule hits. Memory aligned to cache lines.

---

### Master Node (Rx & Dispatcher)
Handles receiving packets from the vdev and dispatching them to workers.

#### [NEW] [src/master.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.h) / [src/master.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c)
Implements the `master_loop()`.
- Fetches mbufs using `rte_eth_rx_burst()`.
- Extracts 5-tuple and implements **Software RSS Hashing (Flow Affinity)** using `rte_jhash` modulo the number of workers to choose the worker ring.
- Enqueues mbufs via `rte_ring_enqueue_burst()`.

---

### Worker Node (Parser & Matcher)
Handles packet classification and freeing.

#### [NEW] [src/parser.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/parser.h) / [src/parser.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/parser.c)
Zero-copy 5-tuple extraction from DPDK mbuf. Uses `rte_pktmbuf_mtod()`.

#### [NEW] [src/matcher.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.h) / [src/matcher.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c)
Rule reading (from file) and matching logic. Since the rule set is small, a linear scan or a simple hash-based first-match algorithm will be implemented for high speed.

#### [NEW] [src/worker.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.h) / [src/worker.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c)
Implements the `worker_loop()`.
- Dequeues mbufs using `rte_ring_dequeue_burst()`.
- Calls parser to extract 5-tuple.
- Calls matcher to find rule.
- Updates statistics.
- **Batching Memory Free**: Accumulates processed mbuf pointers into a local array and frees them in bulk (chunks of 32 or 64) using `rte_pktmbuf_free_bulk()` to minimize mempool lock contention.

---

### Initialization and Main
Entry point of the application.

#### [NEW] [src/main.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/main.c)
- DPDK EAL Initialization (`rte_eal_init()`).
- Mempool creation (`rte_pktmbuf_pool_create()`).
- Port initialization and configuration (for the PCAP vdev).
- Ring creation for Master-Worker IPC.
- Launching lcores using `rte_eal_remote_launch()`.
- Running the statistics printer in the main thread (1-second intervals).

## Verification Plan

### Automated Tests
- Create a Python script in `tests/` to generate a synthetic `.pcap` file for testing.
- Run the compiled application against the `.pcap` and parse the console output.
- Generate a `.csv` file for the benchmark results.

### Manual Verification
- Run `./build/spifast -l 0-4 -n 4 --vdev "net_pcap0,rx_pcap=traffic_sample.pcap,tx_pcap=out_drop.pcap" -- -r spi_rules.conf`.
- Observe the 1-second statistics to ensure ≥ 700 Mbps throughput, > 500k pps flow rate, and 0% missing packets.
