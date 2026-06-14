# Optimize Double Parsing with Mbuf Private Data

This plan aims to implement the "parse-once" optimization for the hash-based load balancing using DPDK's `rte_mbuf` private data area. This will avoid the overhead of parsing L2/L3/L4 headers twice (once on the Master and once on the Worker), improving overall throughput and saving CPU cycles and L1/L2 cache pollution on Worker cores.

## Proposed Changes

### 1. `src/common.h`
Add the `pkt_metadata_t` struct to hold the parsed 5-tuple and a valid flag.

#### [MODIFY] [common.h](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/common.h)
- Add definition:
  ```c
  typedef struct {
      five_tuple_t tuple;
      uint8_t is_valid;
  } pkt_metadata_t;
  ```

### 2. `src/main.c`
Enable private data area when creating the memory pool.

#### [MODIFY] [main.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/main.c)
- Modify `rte_pktmbuf_pool_create` to use `sizeof(pkt_metadata_t)` instead of `0` for the `priv_size` argument.

### 3. `src/master.c`
Store the parsed 5-tuple into the mbuf's private data.

#### [MODIFY] [master.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c)
- Retrieve private data pointer using `rte_mbuf_to_priv(m)`.
- Use `parse_five_tuple` to populate the `meta->tuple`.
- Set `meta->is_valid = 1` if successful, calculate hash and route.
- Set `meta->is_valid = 0` if failed.

### 4. `src/worker.c`
Read the 5-tuple from the mbuf's private data instead of re-parsing the packet.

#### [MODIFY] [worker.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/worker.c)
- Retrieve private data pointer using `rte_mbuf_to_priv(m)`.
- Check `meta->is_valid`.
- If valid, directly call `match_rule(&meta->tuple)` and skip the `parse_five_tuple` function entirely.

## Verification Plan

### Automated Tests
- Run `ninja -C build` to ensure the project compiles successfully without any warnings or errors.
- Run `sudo ./tests/judge/run_project_tcpreplay.sh` to measure the new performance statistics and verify that rules are still being matched correctly (rule hit counts should remain accurate and throughput should increase).
