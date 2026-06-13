# SPIFast v2 — Change Walkthrough

## What Was Changed

### 1. Simplified Rule Actions

[spi_rules.conf](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/spi_rules.conf) now uses only `FORWARD` or `DROP`. All `FORWARD_WORKER_X` labels have been removed.

```diff
-HTTP_TRAFFIC,TCP,*,*,*,80,FORWARD_WORKER_0
+HTTP_TRAFFIC,TCP,*,*,*,80,FORWARD
```

Workers have always been dynamically load-balanced by hash — the worker designation in the config file was meaningless to the runtime. This change makes the spec and the config consistent.

---

### 2. Real-time Config Reload Architecture

#### Double-buffered Rule Tables (matcher.c)

Two statically allocated, cache-line-aligned rule arrays live in [matcher.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c):

```c
spi_rule_t g_rule_table_a[MAX_RULES] __rte_cache_aligned;
spi_rule_t g_rule_table_b[MAX_RULES] __rte_cache_aligned;
```

An **atomic pointer** selects which table is live:

```c
_Atomic(spi_rule_t *) g_active_rules;
_Atomic uint32_t      g_active_num_rules;
```

On startup, `matcher_init()` fills `table_a` and stores its address with `memory_order_release`.  
On reload, `matcher_reload()` detects the inactive table, fills it, then does the atomic swap — all without touching the data path.

#### Lock-free Reader Path (matcher.h)

`match_rule()` remains `always_inline` and uses `__restrict__`. It now takes a single `atomic_load_explicit(..., memory_order_acquire)` snapshot at the top of each call — one extra load instruction on x86, negligible overhead:

```c
const spi_rule_t *rules =
    atomic_load_explicit(&g_active_rules, memory_order_acquire);
uint32_t num_rules =
    atomic_load_explicit(&g_active_num_rules, memory_order_acquire);
```

#### Worker Burst Consistency (worker.c)

Each worker takes the atomic snapshot **once per burst** rather than once per packet, so all 64 packets in a burst see the same rule table. The next burst may transparently see a new table after a hot-reload.

#### Control Plane Thread (control.c / control.h)

A `pthread` (NOT a DPDK lcore) listens on `CTRL_SOCKET_PATH = "/tmp/spifast_ctrl.sock"`:

```
spi_cli reload_rules /path/to/new.conf
        │
        └─► connect() to UDS
            send(path)
            recv(response)  ←── "OK: loaded 6 rules from ..."
```

The control thread never touches the ring buffers or mbuf pools — zero data-path contention.

#### CLI Tool (spi_cli.c)

```bash
# Hot-reload rules while spifast is running
./build/spi_cli reload_rules spi_rules.conf
# → OK: loaded 6 rules from spi_rules.conf
```

Exits with status 0 on OK, 1 on ERROR. No DPDK dependency.

---

## Files Changed

| File | Change |
|:---|:---|
| [spi_rules.conf](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/spi_rules.conf) | `FORWARD_WORKER_X` → `FORWARD` |
| [common.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h) | Replaced static globals with atomic pointer declarations + `CTRL_SOCKET_PATH` |
| [matcher.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.h) | `match_rule()` reads from atomic pointer; `matcher_reload()` declared |
| [matcher.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c) | Defines both tables + atomic vars; `parse_rules_into()` helper; `matcher_reload()` |
| [worker.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c) | Burst-level atomic snapshot; action read from snapshot pointer |
| [stats.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/stats.c) | Rule names/count read from atomic active pointer |
| [main.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/main.c) | Calls `control_thread_start()` / `control_thread_stop()` |
| [meson.build](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/meson.build) | Adds `control.c`; adds `spi_cli` target |
| **[NEW]** [control.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/control.h) | Control thread API |
| **[NEW]** [control.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/control.c) | UDS listener + `matcher_reload()` dispatch |
| **[NEW]** [spi_cli.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/spi_cli.c) | Standalone CLI tool |

## Preserved Optimizations

All existing performance work is intact:
- `rte_prefetch0` look-ahead pipeline (master + worker)
- `__rte_cache_aligned` on `spi_rule_t`, `worker_stats_t`, and both rule tables
- `always_inline` + `__restrict__` on `match_rule()` and `parse_five_tuple()`
- `RING_F_SP_ENQ | RING_F_SC_DEQ` single-producer/consumer rings
- XOR hash-based worker distribution
- `rte_ring_enqueue_burst` batch enqueue
- `rte_pktmbuf_free_bulk` batch free
- Local stat accumulation before global write
- `likely` / `unlikely` branch prediction hints

## Build & Verify

```bash
# On Linux (where DPDK is installed)
meson setup build --wipe
ninja -C build

# Run spifast
sudo ./build/spifast -l 0-4 -n 4 \
  --vdev "net_pcap0,rx_pcap=tests/data/traffic_sample.pcap,tx_pcap=out_drop.pcap" \
  -- -r spi_rules.conf

# In another terminal — hot-reload rules without restart
./build/spi_cli reload_rules spi_rules.conf
```