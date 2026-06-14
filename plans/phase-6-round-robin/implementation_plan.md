# Implementation Plan - Burst-Level Round-Robin Load Balancing

This plan outlines the changes required to switch the architecture from a "Hash-Based Per-Packet Distribution" to a "Burst-Level Round-Robin Distribution". This strategy will eliminate the need to iterate through packets on the Master core, significantly reducing CPU overhead, avoiding cache locality breakage, and maximizing the efficiency of `rte_ring_enqueue_burst`.

## Proposed Changes

### 1. Master Core Load Balancing Strategy

#### [MODIFY] [master.c](file:///mnt/70E63C66E63C2EAA/Users/ADMIN/Desktop/coding/hpc-spi-classifier/src/master.c)
- Introduce a `current_worker` state variable outside the main `master_loop`.
- Remove the internal `worker_bufs` array and `worker_buf_count` variables.
- Remove the per-packet `for` loop that was used for extracting `five_tuple_t` and hashing. We will now only loop once purely to calculate `total_rx_bytes` (which is still needed for stats).
- Enqueue the entire `bufs` array of size `nb_rx` directly into `worker_rings[current_worker]`.
- Increment `current_worker` and wrap around using `num_workers` logic.

The rewritten section of the loop will look like this:
```c
        uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
        if (unlikely(nb_rx == 0)) continue;
        
        uint64_t total_rx_bytes = 0;
        for (uint16_t i = 0; i < nb_rx; i++) {
            total_rx_bytes += rte_pktmbuf_pkt_len(bufs[i]);
        }
        
        g_master_rx_packets += nb_rx;
        g_master_rx_bytes += total_rx_bytes;
        
        // Enqueue entire burst to a single worker
        uint16_t nb_tx = rte_ring_enqueue_burst(worker_rings[current_worker], 
            (void * const *)bufs, nb_rx, NULL);
        
        if (unlikely(nb_tx < nb_rx)) {
            uint16_t drop_count = nb_rx - nb_tx;
            g_master_dropped_packets += drop_count;
            rte_pktmbuf_free_bulk(&bufs[nb_tx], drop_count);
        }
        
        // Rotate to the next worker
        current_worker++;
        if (unlikely(current_worker >= num_workers)) {
            current_worker = 0;
        }
```

## Verification Plan

### Automated/Manual Tests
- Build the code using `ninja -C build` to ensure successful compilation.
- Run the performance benchmarks using `sudo ./tests/judge/run_benchmark_tcpreplay.sh` to measure throughput and PPS improvements.
- Check the Drop Rate and Flow Rate output to confirm that packets are still correctly routed to Workers without any drops on the enqueue side.
