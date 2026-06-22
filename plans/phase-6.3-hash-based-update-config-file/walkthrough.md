# Walkthrough - Config Conversion & DPDK ACL Integration

We have successfully migrated the SPIFast application to utilize the new configuration rules format and refactored the fast-path packet matching logic to leverage DPDK ACL (`librte_acl`) in burst mode.

## Changes Made

### 1. Configuration & Rules

- **[spi_rules.conf](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/spi_rules.conf)**: Converted all data from the Table 1 and Table 2 input tables into the new double-section format:
  - `[GROUPS_SECTION]` mapping group names to priority/precedence and action (`FORWARD` or `DROP`).
  - `[FILTERS_SECTION]` mapping filter names to group, protocol, IP subnet masks, and ports.
  - Normalized all `NA` or `any` values to wildcard `*`.
  - Resolved duplicate filter name `f_l34_facebook_4` (row 5) to `f_l34_facebook_5`.
  - Maintained duplicate group name `fg_l34_dns_sdf1005` with precedence 104 and 105.

### 2. Core C Source Code

- **[common.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h)**: Forward declared `struct rte_acl_ctx` and declared the global atomic pointer `g_active_acl_ctx`.
- **[matcher.h](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.h)**: Re-defined the inline `match_rule` wrapper to execute lookups via `rte_acl_classify` for single packets when required.
- **[matcher.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/matcher.c)**:
  - Set up a standard 5-tuple field descriptor `ipv4_defs` mapping to `five_tuple_t`.
  - Re-wrote configuration parser `parse_rules_into` to parse both `[GROUPS_SECTION]` and `[FILTERS_SECTION]`, tracking subnets, IP masks, and port ranges.
  - Updated `matcher_init` and `matcher_reload` to construct a new `rte_acl_ctx` dynamically with incremented identifiers, add rule fields (setting ACL priority as `1000 - precedence`), build the trie structure, and swap context pointers atomically.
  - Implemented a `usleep(50000)` (50ms) grace period in the hot-reload function to prevent worker thread crashes before freeing the old context.
- **[worker.c](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c)**:
  - Replaced the per-packet linear search loop with burst classification using `rte_acl_classify`.
  - Processed `nb_rx` packet buffers in parallel, mapping match results (`res - 1`) directly back to their rule descriptors for hit counters, forwarding, and dropping statistics.

### 3. Python Validation & Tools

- **[gen_func_test.py](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/tests/gen_tests/gen_func_test.py)**:
  - Updated the Python rules parser to read the new format.
  - Implemented CIDR prefix mask calculations in the simulator and sorted rules descending by priority (`1000 - precedence`) to mirror DPDK ACL first-match behavior.
  - Updated edge cases to pick `f_l34_http_all` and `f_l34_dns_udp` as targets.
  - Refactored the overlap test case to verify priority resolution between `f_l34_youtube_1` and `f_l34_https_all`.
- **[analyze_pcap.py](file:///c:/Users/ADMIN/Desktop/coding/hpc-spi-classifier/tests/analyze_pcap/analyze_pcap.py)**:
  - Updated the parser to read the new format.
  - Re-wrote `match_packet` to support IP CIDR subnet checks.
