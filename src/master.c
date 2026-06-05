#include "master.h"
#include "common.h"
#include "stats.h"
#include "parser.h"
#include <rte_ethdev.h>
#include <rte_jhash.h>
#include <rte_lcore.h>

int master_loop(struct rte_ring *worker_rings[], uint32_t num_workers, uint16_t port_id)
{
	struct rte_mbuf *bufs[BURST_SIZE];
	
	printf("Master started on lcore %u\n", rte_lcore_id());
	
	while (1) {
		stats_print_periodic();
		
		uint16_t nb_rx = rte_eth_rx_burst(port_id, 0, bufs, BURST_SIZE);
		
		if (unlikely(nb_rx == 0)) continue;
		
		uint64_t total_rx_bytes = 0;
		
		// Array to accumulate packets for each worker
		struct rte_mbuf *worker_bufs[MAX_WORKERS][BURST_SIZE];
		uint16_t worker_buf_count[MAX_WORKERS] = {0};
		
		for (uint16_t i = 0; i < nb_rx; i++) {
			struct rte_mbuf *m = bufs[i];
			total_rx_bytes += rte_pktmbuf_pkt_len(m);
			
			five_tuple_t tuple;
			uint32_t target_worker = 0;
			
			if (likely(parse_five_tuple(m, &tuple))) {
				// Software RSS Hashing
				// We hash the 5-tuple to preserve flow affinity
				uint32_t word1 = tuple.src_ip;
				uint32_t word2 = tuple.dst_ip;
				uint32_t word3 = ((uint32_t)tuple.src_port) | (((uint32_t)tuple.dst_port) << 16) | (((uint32_t)tuple.protocol) << 8);
				uint32_t hash = rte_jhash_3words(word1, word2, word3, 0xdeadbeef);
				target_worker = hash % num_workers;
			} else {
				// If not parsable, just distribute round robin
				target_worker = i % num_workers;
			}
			
			worker_bufs[target_worker][worker_buf_count[target_worker]++] = m;
		}
		
		g_master_rx_packets += nb_rx;
		g_master_rx_bytes += total_rx_bytes;
		
		// Enqueue to workers
		for (uint32_t w = 0; w < num_workers; w++) {
			if (worker_buf_count[w] > 0) {
				uint16_t nb_tx = rte_ring_enqueue_burst(worker_rings[w], 
					(void * const *)worker_bufs[w], worker_buf_count[w], NULL);
				
				if (unlikely(nb_tx < worker_buf_count[w])) {
					uint16_t drop_count = worker_buf_count[w] - nb_tx;
					g_master_dropped_packets += drop_count;
					
					// Free dropped packets
					rte_pktmbuf_free_bulk(&worker_bufs[w][nb_tx], drop_count);
				}
			}
		}
	}
	
	return 0;
}
