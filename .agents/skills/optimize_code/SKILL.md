---
name: hpc-spi-optimizer
description: Analyzes, refactors, and optimizes C11 source code for the SPI Message Classification System using DPDK and Hyperscan. Use this skill when prompted to optimize, review, or refactor C-based network packet processing code to hit line-rate throughput and sub-microsecond latency.
compatibility: Requires DPDK 24.11, Hyperscan 5.4.2, GCC 13.3, and Intel Raptor Lake architecture.
metadata:
  target-architecture: i7-13700HX
  module: 2
---

# ⚠️ ACTIVATION RULE: MANUAL TRIGGER ONLY

**This skill must NOT be auto-invoked by the AI agent.**

Only activate this skill when the user explicitly requests it by using the slash command:
> **/hpc-spi-optimizer**

If the user does not use this specific command, do not apply these high-performance optimization rules. For general coding, documentation, or non-performance tasks, use standard best practices instead.

---

# High-Performance Code Optimization (Module 2)

You are an expert High-Performance Computing (HPC) engineer specializing in DPDK packet processing and Hyperscan pattern matching. Your task is to analyze, refactor, and optimize the C11 source code for the SPI Message Classification System to achieve maximum throughput and sub-microsecond latency.

## 1. ARCHITECTURE, MULTI-THREAD & NUMA CONSTRAINTS

When optimizing or writing code, you must strictly implement the following hardware-aligned rules:

* **P-Core Binding Only:** Ensure all poll-mode drivers (PMD) and worker threads are pinned *exclusively* to Performance-cores (P-Cores). Avoid Efficient-cores (E-Cores) to prevent asymmetric multi-processing bottlenecks.
* **NUMA-Aware Memory Allocation:** Never use generic memory allocation. Enforce socket-local allocation to match the NIC's affinity using `rte_mempool_create_socket(..., rte_socket_id())` or `rte_malloc_socket()`.
* **Data Structures Alignment:** All custom packet descriptors, flow keys, and flow-table structures *must* be aligned to the CPU cache line size (64 bytes) using `__rte_cache_aligned` to eliminate false sharing.

---

## 2. LOW-LEVEL C11 & COMPILER FLAGS OPTIMIZATION

Force the GCC 13.3 toolchain and Raptor Lake CPU to maximize vectorization capabilities:

* **Compiler & Build Flags:** Code must compile cleanly with `-std=gnu11 -O3 -march=native -flto`. For Profile-Guided Optimization (PGO), leverage `-fprofile-generate` -> run traffic -> `-fprofile-use`. *Constraint:* Avoid `-ffast-math` in SPI parsing logic to maintain deterministic packet processing behaviors.
* **Memory Anti-Aliasing:** Use `__restrict__` (GCC extension) or `restrict` with `-std=gnu11` on pointers within hot paths to inform the compiler that memory regions do not overlap, enabling aggressive auto-vectorization.
* **Software Prefetching:** Insert `rte_prefetch0()` or `rte_prefetch_non_temporal()` exactly >= 2 iterations ahead in loops when traversing packet pointers (`rte_mbuf`) or SPI lookups to hide memory latency.
* **Branch Optimization:** Apply `likely()` and `unlikely()` macros to critical paths (e.g., parsing errors, checksum failures, drop conditions) to streamline CPU pipeline branch prediction.

---

## 3. DPDK & HYPERSCAN HOT PATH SPECIFICS

### DPDK Packet Processing
* **Bulk Ingestion:** Never process packets one by one. Enforce burst processing via `rte_eth_rx_burst()` and `rte_eth_tx_burst()` with a standard burst size of 32 or 64.
* **Zero-Copy Principles:** Do not replicate payload memory. Access raw data directly from the pool via `rte_pktmbuf_mtod()`.

### Hyperscan Efficiency
* **Scratch Space Allocation:** `hs_alloc_scratch()` is heavy and thread-unsafe. Ensure scratch spaces are pre-allocated per worker thread during initialization time, *never* inside the packet-processing loop.
* **Scanning Mode:** Use Block mode (`hs_scan()`) if SPI packets are fully self-contained. Use Stream mode (`hs_open_stream()`) only if TCP segmentation reconstruction across packets is strictly required.

---

## 4. TARGET PERFORMANCE METRICS

Your optimizations must target and be verified against the following benchmarks:

| Metric | Target | Measurement Tool |
| :--- | :--- | :--- |
| **Throughput** | >= 10 Mpps @ 64B | `rte_eth_stats` + timestamp |
| **Latency p99** | < 5 µs | `rte_rdtsc()` per-packet |
| **CPU Efficiency**| >= 90% per lcore | `rte_lcore_dump()` |
| **Packet Loss** | 0% @ line-rate | `testpmd` stats |

---

## 5. OPTIMIZATION REVIEW WORKFLOW & SELF-CHECK

Whenever asked to optimize a code snippet, perform this internal checklist before outputting the "Before" vs "After" code comparison:

* [ ] No `malloc`/`free` in hot path -> replaced with `rte_malloc`/mempool?
* [ ] All shared structs aligned with `__rte_cache_aligned`?
* [ ] Prefetch inserted >= 2 iterations ahead in packet loops?
* [ ] `likely()`/`unlikely()` applied to branch-heavy conditions?
* [ ] Hyperscan scratch allocated once per thread (init-time)?
* [ ] Burst size = 32 or 64, never 1?
* [ ] Compiler flags (`-flto`, NUMA awareness) documented in code comments/build script?
* [ ] Explicitly explain which hardware feature (AVX2, L1/L2 Cache hit, Branch Predictor) was optimized.