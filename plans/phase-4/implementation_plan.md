# SPIFast v2: Simplified Actions + Real-time Config Reload

Update the project to align with the v2 specification while preserving all existing optimizations (prefetch pipeline, cache-line aligned structs, lock-free ring distribution, burst enqueue/dequeue, hash-based worker dispatch).

## User Review Required

> [!IMPORTANT]
> **Unix Domain Socket path:** The control socket will be created at `/tmp/spifast_ctrl.sock`. Confirm this is acceptable for your environment.

> [!IMPORTANT]
> **CLI tool (`spi_cli`):** Will be a separate executable built from `src/spi_cli.c`. It connects to the UDS, sends the config file path, and receives a status response. Confirm this approach vs. integrating into the main binary.

## Open Questions

> [!IMPORTANT]
> **Hit counter preservation on reload:** When rules are hot-swapped, the `hit_count` counters in `spi_rule_t` reset to zero since the shadow table is freshly parsed. Should we attempt to carry over hit counts for rules that match by name, or is a clean reset acceptable?

> [!IMPORTANT]
> **Worker stats `rule_hits[]` array on reload:** Workers index into `rule_hits[rule_idx]` based on the rule index. After an atomic swap, rule indices may change (rules reordered, added, or removed). The current `worker_stats_t.rule_hits[MAX_RULES]` array will become stale for the old rule mapping. Should we reset all worker stats on reload, or accept potential brief inaccuracy?

## Proposed Changes

### Change 1: Simplify Rule Actions (FORWARD / DROP only)

---

#### [MODIFY] [spi_rules.conf](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/spi_rules.conf)
Replace all `FORWARD_WORKER_X` actions with plain `FORWARD`:
```text
HTTP_TRAFFIC,TCP,*,*,*,80,FORWARD
HTTPS_TRAFFIC,TCP,*,*,*,443,FORWARD
DNS_TRAFFIC,UDP,*,*,*,53,FORWARD
GTPU_TRAFFIC,UDP,*,*,*,2152,FORWARD
SSH_BLOCK,TCP,*,*,*,22,DROP
DEFAULT,*,*,*,*,*,DROP
```

#### [MODIFY] [matcher.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c)
The action parser already handles this correctly — `strcmp(action_str, "DROP") == 0` → `ACTION_DROP`, else `ACTION_FORWARD`. The existing `FORWARD_WORKER_X` strings already fall through to `ACTION_FORWARD`. **No logic change needed** in the parser for this, but we will be rewriting this file anyway for Change 2.

---

### Change 2: Real-time Config Reload (Double Buffering + Atomic Swap)

This is the major architectural change. The design uses **two statically allocated rule tables** (double buffer) and an **atomic pointer** to switch between them, ensuring lock-free reads on the data path.

#### Architecture Overview

```
┌─────────────┐   Unix Domain Socket   ┌──────────────────┐
│  spi_cli    │ ──────────────────────► │  Control Thread  │
│  (reload)   │   sends config path     │  (spifast main)  │
└─────────────┘                         └────────┬─────────┘
                                                 │
                                    1. Parse new config into shadow_table
                                    2. Atomic swap: active_rules ← shadow_table
                                    3. Send ACK to cli
                                                 │
                         ┌───────────────────────┼───────────────────────┐
                         ▼                       ▼                       ▼
                   ┌──────────┐            ┌──────────┐            ┌──────────┐
                   │ Worker 0 │            │ Worker 1 │            │ Worker N │
                   │ reads    │            │ reads    │            │ reads    │
                   │ active_  │            │ active_  │            │ active_  │
                   │ rules    │            │ rules    │            │ rules    │
                   └──────────┘            └──────────┘            └──────────┘
```

**Key design decisions:**
- **Two static tables** (`rule_table_a[MAX_RULES]` and `rule_table_b[MAX_RULES]`) — no `malloc` on the hot path.
- **C11 `<stdatomic.h>`** for `atomic_store`/`atomic_load` of the active pointer and rule count — fully lock-free.
- **Dedicated `pthread` control thread** (not on an lcore) listens on the UDS — zero impact on DPDK data path.
- **Existing optimizations preserved:** Prefetch pipeline, `__restrict__` pointers, `always_inline`, `__rte_cache_aligned`, burst ring enqueue, hash-based distribution — all untouched.

---

#### [MODIFY] [common.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h)
- Remove `extern spi_rule_t g_rules[MAX_RULES]` and `extern uint32_t g_num_rules`.
- Add `extern` declarations for atomic rule pointers:
  ```c
  #include <stdatomic.h>

  // Double-buffered rule tables (defined in matcher.c)
  extern spi_rule_t g_rule_table_a[MAX_RULES];
  extern spi_rule_t g_rule_table_b[MAX_RULES];

  // Atomic pointers for lock-free access
  extern _Atomic(spi_rule_t *) g_active_rules;
  extern _Atomic uint32_t g_active_num_rules;
  ```
- Add `#define CTRL_SOCKET_PATH "/tmp/spifast_ctrl.sock"`.

#### [MODIFY] [matcher.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.h)
- Modify `match_rule()` to read from the atomic `g_active_rules` pointer:
  ```c
  static inline __attribute__((always_inline))
  int match_rule(const five_tuple_t *__restrict__ tuple)
  {
      // Lock-free atomic load of active rule set
      const spi_rule_t *rules = atomic_load_explicit(
          &g_active_rules, memory_order_acquire);
      uint32_t num_rules = atomic_load_explicit(
          &g_active_num_rules, memory_order_acquire);

      for (uint32_t i = 0; i < num_rules; i++) {
          // ... existing first-match logic unchanged ...
      }
      return -1;
  }
  ```
- Add declaration: `int matcher_reload(const char *rule_file);`
- **Preserved:** `always_inline`, `__restrict__`, first-match algorithm, field comparison order.

#### [MODIFY] [matcher.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c)
- Define the two static rule tables and atomic pointers:
  ```c
  spi_rule_t g_rule_table_a[MAX_RULES] __rte_cache_aligned;
  spi_rule_t g_rule_table_b[MAX_RULES] __rte_cache_aligned;
  _Atomic(spi_rule_t *) g_active_rules;
  _Atomic uint32_t g_active_num_rules = 0;
  ```
- Extract parsing logic into a static helper `parse_rules_into(const char *file, spi_rule_t *table, uint32_t *count)` that populates any given table.
- `matcher_init()`: Calls `parse_rules_into()` to fill `g_rule_table_a`, then atomically sets `g_active_rules = g_rule_table_a`.
- `matcher_reload()`: Determines the **inactive** (shadow) table, fills it via `parse_rules_into()`, then performs atomic swap:
  ```c
  int matcher_reload(const char *rule_file)
  {
      // Determine shadow table
      spi_rule_t *current = atomic_load_explicit(
          &g_active_rules, memory_order_relaxed);
      spi_rule_t *shadow = (current == g_rule_table_a)
          ? g_rule_table_b : g_rule_table_a;

      uint32_t new_count = 0;
      if (parse_rules_into(rule_file, shadow, &new_count) < 0)
          return -1;

      // Atomic swap — workers see new rules on next iteration
      atomic_store_explicit(&g_active_num_rules,
          new_count, memory_order_release);
      atomic_store_explicit(&g_active_rules,
          shadow, memory_order_release);

      return 0;
  }
  ```

#### [NEW] [control.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/control.h)
- Declare the control thread interface:
  ```c
  #pragma once
  // Start the control thread listening on CTRL_SOCKET_PATH.
  // Returns 0 on success, -1 on failure.
  int control_thread_start(void);
  // Stop the control thread and clean up the socket.
  void control_thread_stop(void);
  ```

#### [NEW] [control.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/control.c)
- Implements a `pthread_create`-based control thread (NOT on a DPDK lcore — zero data-path impact).
- The thread:
  1. Creates and `bind()`s a `AF_UNIX` / `SOCK_STREAM` socket at `CTRL_SOCKET_PATH`.
  2. `listen()`s with backlog 1 (single CLI client at a time).
  3. Loops on `accept()`, reads a config file path from the client (max 256 bytes).
  4. Calls `matcher_reload(path)`.
  5. Sends back `"OK\n"` or `"ERROR: ...\n"` to the client.
  6. Closes the client socket and loops back to `accept()`.
- `control_thread_stop()`: Sets a flag, `unlink()`s the socket, `pthread_join()`s.
- **Not in the fast-path** — uses standard POSIX I/O (`recv`, `send`, `fprintf` for logging).

#### [NEW] [spi_cli.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/spi_cli.c)
- Standalone CLI tool:
  ```bash
  ./build/spi_cli reload_rules /path/to/new_spi_rules.conf
  ```
- Connects to `CTRL_SOCKET_PATH`, sends the config path, prints the response, exits.
- Simple ~60 lines of C. No DPDK dependency.

#### [MODIFY] [worker.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c)
- Update line 45: Change `g_rules[rule_idx].action_mask` to read from the atomic active pointer:
  ```c
  const spi_rule_t *rules = atomic_load_explicit(
      &g_active_rules, memory_order_acquire);
  // ...
  if (rules[rule_idx].action_mask == ACTION_DROP) {
  ```
- **All existing optimizations preserved:** Prefetch pipeline, burst dequeue, batch `rte_pktmbuf_free_bulk`, local stat accumulation.

#### [MODIFY] [stats.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/stats.c)
- Update `stats_print_periodic()` to read rule names from the atomic active pointer:
  ```c
  const spi_rule_t *rules = atomic_load_explicit(
      &g_active_rules, memory_order_acquire);
  uint32_t num_rules = atomic_load_explicit(
      &g_active_num_rules, memory_order_acquire);
  ```
- Replace `g_rules[i].name` → `rules[i].name` and `g_num_rules` → `num_rules`.

#### [MODIFY] [main.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/main.c)
- Add `#include "control.h"`.
- After `matcher_init()` and before entering `master_loop()`, call `control_thread_start()`.
- After `rte_eal_mp_wait_lcore()`, call `control_thread_stop()`.

#### [MODIFY] [meson.build](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/meson.build)
- Add `'src/control.c'` to the main `spifast` sources list.
- Add a new `executable('spi_cli', 'src/spi_cli.c', install: false)` target (no DPDK dependency).

---

## Summary of Preserved Optimizations

| Optimization | Location | Status |
|:---|:---|:---|
| `rte_prefetch0` pipeline (look-ahead 4) | [master.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c) L33-36, [worker.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c) L30-33 | ✅ Preserved |
| `__rte_cache_aligned` on `spi_rule_t` | [common.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h) L34 | ✅ Preserved |
| `__rte_cache_aligned` on `worker_stats_t` | [stats.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/stats.h) L14 | ✅ Preserved |
| `always_inline` + `__restrict__` on `match_rule` | [matcher.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.h) L9 | ✅ Preserved |
| `always_inline` + `__restrict__` on `parse_five_tuple` | [parser.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/parser.h) L13 | ✅ Preserved |
| `RING_F_SP_ENQ \| RING_F_SC_DEQ` single-producer/consumer rings | [main.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/main.c) L104 | ✅ Preserved |
| Hash-based worker distribution (XOR hash) | [master.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c) L45-48 | ✅ Preserved |
| `rte_ring_enqueue_burst` batch enqueue | [master.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c) L63 | ✅ Preserved |
| `rte_pktmbuf_free_bulk` batch free | [worker.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c) L67 | ✅ Preserved |
| `likely` / `unlikely` branch hints | All fast-path files | ✅ Preserved |
| Local stat accumulation before global write | [worker.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c) L24-27 | ✅ Preserved |

## Verification Plan

### Build Verification
```bash
cd /path/to/hpc-spi-classifier
meson setup build --wipe
ninja -C build
```
- Both `spifast` and `spi_cli` must compile cleanly with `-Wall -Wextra` (warning level 3).

### Functional Verification
1. **Start spifast** with the updated `spi_rules.conf` (FORWARD/DROP only).
2. **Verify initial rule load** — check console output shows 6 rules loaded.
3. **Modify `spi_rules.conf`** (e.g., change SSH_BLOCK to FORWARD, add a new rule).
4. **Run `spi_cli reload_rules spi_rules.conf`** — verify:
   - CLI prints `OK`.
   - Stats output reflects the new rule set (names, hit counters reset).
   - No crash, no packet loss spike during the swap.
5. **Verify DROP works** — SSH packets (port 22) increment drop counter.
6. **Verify FORWARD works** — HTTP/HTTPS/DNS/GTPU packets are forwarded to workers.

### Performance Verification
- Run before/after benchmarks to confirm throughput, pps, and drop rates remain within KPI thresholds.
- The atomic loads (`memory_order_acquire`) add negligible overhead (single instruction on x86).
