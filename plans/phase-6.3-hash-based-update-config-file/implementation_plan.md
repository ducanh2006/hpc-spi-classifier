# Implementation Plan - Config Conversion & DPDK ACL Integration

This plan describes the steps to parse the new section-based `spi_rules.conf` file format and refactor the SPIFast classification logic from a linear loop to DPDK Access Control Lists (ACL, `librte_acl`) for high-performance classification.

## User Review Required

> [!IMPORTANT]
> The new configuration file `spi_rules.conf` uses section headers `[GROUPS_SECTION]` and `[FILTERS_SECTION]`. The rule matching order is dictated by the Group's precedence (lower precedence value maps to higher matching priority). We will sort rules in Python and configure DPDK ACL priority as `1000 - precedence` to preserve the correct first-match behavior.

## Proposed Changes

---

### Configuration Files

#### [MODIFY] [spi_rules.conf](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/spi_rules.conf)
- Convert the 2 Markdown tables into the new configuration file structure:
  - **[GROUPS_SECTION]**: Defines Group Name, Precedence/Priority, and Action.
  - **[FILTERS_SECTION]**: Defines Filter Name, Group Name, Protocol, Src IP/Mask, Dst IP/Mask, Src Port, and Dst Port.
  - Ensure all `NA`/`any` values are normalized to `*`.
  - Fix row 5 duplicate name to `f_l34_facebook_5`.

---

### Core Data Plane Application (C)

#### [MODIFY] [common.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h)
- Include `<rte_acl.h>`.
- Declare external global `g_active_acl_ctx` atomic pointer: `extern _Atomic(struct rte_acl_ctx *) g_active_acl_ctx;`.

#### [MODIFY] [matcher.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.h)
- Refactor/add `match_rule` as an inline wrapper around `rte_acl_classify` for single-packet lookups.
- Alternatively, support burst classification directly in `worker.c`.

#### [MODIFY] [matcher.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c)
- Define 5-tuple field layout array `struct rte_acl_field_def ipv4_defs[5]` to match the layout of `five_tuple_t`.
- Define global atomic pointer `_Atomic(struct rte_acl_ctx *) g_active_acl_ctx;`.
- Rewrite `parse_rules_into` to:
  1. Parse the new `[GROUPS_SECTION]` and map group names to their actions and precedence.
  2. Parse the `[FILTERS_SECTION]`, lookup the group details, normalize values, and store parsed rules.
- Update `matcher_init` and `matcher_reload` to:
  1. Create a new DPDK ACL context (`rte_acl_create`) with a dynamically incremented name (to avoid EAL naming conflicts).
  2. Map the parsed rules to DPDK ACL rule format (`struct rte_acl_rule` fields). Set rule priority to `1000 - precedence`. Set rule `userdata` to `rule_idx + 1` (where 0 means no match).
  3. Call `rte_acl_add_rules` and `rte_acl_build`.
  4. Perform an atomic release store to update `g_active_acl_ctx` along with `g_active_rules` and `g_active_num_rules`.
  5. For `matcher_reload`, wait for a short grace period (e.g., 50ms) before calling `rte_acl_free` on the old ACL context to ensure worker threads safely transition to the new context.

#### [MODIFY] [worker.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c)
- Refactor the worker processing loop to classify packets in bursts using `rte_acl_classify` on `g_active_acl_ctx`.
- Collect the indices of valid packets, run `rte_acl_classify` on them, and map the results (`res > 0` translates to `rule_idx = res - 1`).
- Keep stats increment, drop counts, and debug logging intact.

---

### Python Scripts & Tests

#### [MODIFY] [gen_func_test.py](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/tests/gen_tests/gen_func_test.py)
- Refactor the Python parser to read the new sectioned `spi_rules.conf`.
- Resolve filter actions and precedence via the parsed groups.
- Sort the rules array by priority descending so that the Python first-match replica (`simulate_first_match`) returns the highest-priority rule, matching DPDK ACL.
- Update targeted tests (e.g., replace `HTTP_TRAFFIC` with `f_l34_http_all`, replace overlap tests with `f_l34_youtube_1` vs `f_l34_https_all`).

#### [MODIFY] [analyze_pcap.py](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/tests/analyze_pcap/analyze_pcap.py)
- Update the rule parser to read the new format.
- Implement CIDR prefix checking in `match_packet` to support IP/Mask rules.

---

## Verification Plan

### Automated Tests
1. **Compilation Check**: Run `wsl meson compile -C build` to ensure the project compiles cleanly.
2. **Correctness Verification**:
   - Run Python test generator to generate the virtual PCAP and expected CSV baseline:
     `wsl python3 tests/gen_tests/gen_func_test.py`
   - Run correctness checker:
     `wsl ./tests/judge/run_check_correctness.sh`
   - Verify that there are no mismatches between actual processing outputs and the baseline.
3. **Performance Benchmarking**:
   - Run the benchmark script:
     `wsl ./tests/judge/run_benchmark_native.sh`
   - Ensure throughput (>= 700 Mbps), pps (>= 500,000 pps), and drop rate (0%) KPIs are satisfied.
