# Optimize SPIFast DPDK Performance

This plan outlines the changes to apply the `hpc-spi-optimizer` skill's optimizations, pushing the SPI classifier's performance to its limits.

## User Review Required

- Fast-path functions (`parse_five_tuple` and `match_rule`) will be moved to headers as `static inline` functions to completely eliminate function call overhead and allow GCC to better auto-vectorize and optimize the loops.

## Proposed Changes

### Headers & Definitions

#### [MODIFY] [common.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h)
- Add `__rte_cache_aligned` to `spi_rule_t` to prevent any false sharing or cache line crossing during rule iteration.

#### [MODIFY] [parser.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/parser.h)
- Move the `parse_five_tuple` implementation from `parser.c` to `parser.h` as a `static inline __attribute__((always_inline))` function.
- Add `__restrict__` to pointers where appropriate to hint the compiler for auto-vectorization.

#### [DELETE] [parser.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/parser.c)
- Delete this file as its content is moved to the header for inlining.

#### [MODIFY] [matcher.h](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.h)
- Move `match_rule` implementation from `matcher.c` to `matcher.h` as `static inline __attribute__((always_inline))`.
- Use `__restrict__` on the tuple pointer.

#### [MODIFY] [matcher.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c)
- Remove `match_rule` (moved to header).
- Fix the bug where `src_ip` and `dst_ip` are hardcoded to 0 instead of parsed from the rule line.

### Core Processing Loops

#### [MODIFY] [master.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c)
- **Prefetching**: Add `rte_prefetch0(bufs[i+4])` and `rte_prefetch0(rte_pktmbuf_mtod(bufs[i+4], void*))` in the RX loop to hide memory latency.
- **Rdtsc Throttling**: Throttle `stats_print_periodic()` using a burst counter so the master core doesn't query `rte_get_timer_cycles()` on every single burst, saving precious cycles.
- **Hashing**: Fix the 5-tuple hash to correctly XOR the protocol without clobbering the `src_port` bits.

#### [MODIFY] [worker.c](file:///media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c)
- **Prefetching**: Add `rte_prefetch0` for the mbuf and packet data ahead of processing.
- With inlined `parse_five_tuple` and `match_rule`, the compiler will flatten the worker loop, greatly increasing the instructions per cycle (IPC).

## Verification Plan

### Automated Tests
- Run `sudo ./tests/run_project/run_benchmark.sh` to measure the throughput and flow rate improvements. Expectation is reaching closer to 1 Gbps / 1.488 Mpps limits or better, with 0% dropped/missing packets.
