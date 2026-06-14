# Walkthrough - Double Parsing Optimization

We successfully optimized the packet processing pipeline by implementing a single-parsing strategy using the `rte_mbuf` private data area.

## Key Changes Made

1. **Defined Metadata Structure (`src/common.h`)**:
   Created `pkt_metadata_t` containing `five_tuple_t` and `is_valid` flag to represent the pre-parsed packet metadata.
   
2. **Configured Mempool Private Data (`src/main.c`)**:
   Adjusted the fourth parameter (`priv_size`) in `rte_pktmbuf_pool_create` from `0` to `sizeof(pkt_metadata_t)` so each mbuf allocates enough private memory area.

3. **Master Core Single-Parse & Write (`src/master.c`)**:
   Modified `master_loop` to fetch the private pointer via `rte_mbuf_to_priv(m)`, parse the headers once, write the 5-tuple info to it, set `is_valid = 1`, and then hash-route the packet.

4. **Worker Core Direct-Read (`src/worker.c`)**:
   Updated `worker_loop` to retrieve the private pointer via `rte_mbuf_to_priv(m)`, skip `parse_five_tuple`, and directly match using the stored `tuple` if `is_valid` is set.

## Verification Results
- The build succeeded perfectly with no compilation warnings or errors using `ninja -C build`.
